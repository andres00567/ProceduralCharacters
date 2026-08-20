#include "Commandlets/ProceduralAnimGraphCommandlet.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimNodeBase.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "Animation/AnimGraphNode_ProceduralImpulse.h"
#include "Animation/AnimGraphNode_ProceduralHandIK.h"
#include "Animation/AnimGraphNode_ProceduralLookChain.h"
#include "Animation/AnimGraphNode_ProceduralMovementLean.h"
#include "AnimGraphNode_Root.h"
#include "Animation/ProceduralAnimationBlueprintLibrary.h"
#include "Data/ProceduralCharacterTypes.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Animation/Skeleton.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_Self.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
    template <typename NodeType>
    NodeType* CreateGraphNode(UEdGraph& Graph, int32 X, int32 Y)
    {
        FGraphNodeCreator<NodeType> Creator(Graph);
        NodeType* Node = Creator.CreateNode();
        Node->NodePosX = X;
        Node->NodePosY = Y;
        Creator.Finalize();
        return Node;
    }

    UEdGraphPin* FindPosePin(UEdGraphNode& Node, EEdGraphPinDirection Direction, const UScriptStruct* PoseType)
    {
        for (UEdGraphPin* Pin : Node.Pins)
        {
            if (Pin && Pin->Direction == Direction &&
                Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
                Pin->PinType.PinSubCategoryObject.Get() == PoseType)
            {
                return Pin;
            }
        }
        return nullptr;
    }

    bool Connect(UEdGraph& Graph, UEdGraphPin* Output, UEdGraphPin* Input)
    {
        return Output && Input && Graph.GetSchema()->TryCreateConnection(Output, Input);
    }

    bool AddProceduralStateUpdate(UAnimBlueprint& Blueprint, FName VariableName)
    {
        UEdGraph* EventGraph = Blueprint.UbergraphPages.IsEmpty() ? nullptr : Blueprint.UbergraphPages[0];
        if (!EventGraph) return false;
        UK2Node_Event* UpdateEvent = nullptr;
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            if (UK2Node_Event* Event = Cast<UK2Node_Event>(Node);
                Event && Event->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Contains(TEXT("BlueprintUpdateAnimation")))
            {
                UpdateEvent = Event;
                break;
            }
        }
        if (!UpdateEvent) return false;

        TSet<UEdGraphNode*> Reachable;
        TArray<UEdGraphNode*> Pending;
        Pending.Add(UpdateEvent);
        while (!Pending.IsEmpty())
        {
            UEdGraphNode* Node = Pending.Pop(EAllowShrinking::No);
            if (!Node || Reachable.Contains(Node)) continue;
            Reachable.Add(Node);
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || Pin->Direction != EGPD_Output ||
                    Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
                for (UEdGraphPin* Linked : Pin->LinkedTo)
                {
                    if (Linked && Linked->GetOwningNode()) Pending.Add(Linked->GetOwningNode());
                }
            }
        }
        TArray<UEdGraphPin*> Terminals;
        for (UEdGraphNode* Node : Reachable)
        {
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Output &&
                    Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->LinkedTo.IsEmpty())
                {
                    Terminals.Add(Pin);
                }
            }
        }
        if (Terminals.IsEmpty()) return false;

        UFunction* GetterFunction = UProceduralAnimationBlueprintLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UProceduralAnimationBlueprintLibrary, GetProceduralStateFromAnimInstance));
        if (!GetterFunction) return false;
        UK2Node_CallFunction* Getter = CreateGraphNode<UK2Node_CallFunction>(
            *EventGraph, UpdateEvent->NodePosX + 1500, UpdateEvent->NodePosY + 350);
        Getter->SetFromFunction(GetterFunction);
        Getter->ReconstructNode();
        UK2Node_Self* Self = CreateGraphNode<UK2Node_Self>(
            *EventGraph, Getter->NodePosX - 180, Getter->NodePosY + 80);
        UEdGraphPin* SelfOut = Self->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Output);
        if (!SelfOut)
        {
            SelfOut = Self->Pins.IsEmpty() ? nullptr : Self->Pins[0];
        }
        UEdGraphPin* AnimInstanceInput = Getter->FindPin(TEXT("AnimInstance"), EGPD_Input);
        UEdGraphPin* StateOutput = Getter->GetReturnValuePin();
        if (!Connect(*EventGraph, SelfOut, AnimInstanceInput) || !StateOutput) return false;

        int32 SetterOffset = 0;
        for (UEdGraphPin* Terminal : Terminals)
        {
            UK2Node_VariableSet* Setter = CreateGraphNode<UK2Node_VariableSet>(
                *EventGraph, Terminal->GetOwningNode()->NodePosX + 320,
                Terminal->GetOwningNode()->NodePosY + SetterOffset);
            Setter->VariableReference.SetSelfMember(VariableName);
            Setter->ReconstructNode();
            UEdGraphPin* ExecIn = Setter->GetExecPin();
            UEdGraphPin* ValueIn = Setter->FindPin(VariableName, EGPD_Input);
            if (!Connect(*EventGraph, Terminal, ExecIn) || !Connect(*EventGraph, StateOutput, ValueIn))
            {
                return false;
            }
            SetterOffset += 120;
        }
        return true;
    }

    bool ApplyProceduralChain(UAnimBlueprint& Blueprint)
    {
        TArray<UEdGraph*> Graphs;
        Blueprint.GetAllGraphs(Graphs);
        UEdGraph* AnimGraph = nullptr;
        for (UEdGraph* Graph : Graphs)
        {
            if (Graph && Graph->GetName() == TEXT("AnimGraph"))
            {
                AnimGraph = Graph;
                break;
            }
        }
        if (!AnimGraph) return false;
        if (AnimGraph->Nodes.ContainsByPredicate([](const UEdGraphNode* Node)
            { return Node && Node->IsA<UAnimGraphNode_ProceduralMovementLean>(); }))
        {
            UE_LOG(LogTemp, Display, TEXT("Procedural chain already exists; no changes made."));
            return true;
        }
        // Adding a member variable structurally refreshes every graph. Do it before
        // creating source nodes or the refresh can detach newly created custom nodes.
        const FName StateVariable(TEXT("ProceduralState"));
        if (FBlueprintEditorUtils::FindNewVariableIndex(&Blueprint, StateVariable) == INDEX_NONE)
        {
            FEdGraphPinType Type;
            Type.PinCategory = UEdGraphSchema_K2::PC_Struct;
            Type.PinSubCategoryObject = FProceduralCharacterState::StaticStruct();
            if (!FBlueprintEditorUtils::AddMemberVariable(&Blueprint, StateVariable, Type)) return false;
        }
        if (!AddProceduralStateUpdate(Blueprint, StateVariable)) return false;

        TArray<UAnimGraphNode_Root*> Roots;
        AnimGraph->GetNodesOfClass(Roots);
        if (Roots.Num() != 1) return false;
        UEdGraphPin* RootInput = FindPosePin(*Roots[0], EGPD_Input, FPoseLink::StaticStruct());
        if (!RootInput || RootInput->LinkedTo.Num() != 1) return false;
        UEdGraphPin* AuthoredOutput = RootInput->LinkedTo[0];
        RootInput->BreakLinkTo(AuthoredOutput);

        const int32 X = Roots[0]->NodePosX;
        const int32 Y = Roots[0]->NodePosY;
        Roots[0]->NodePosX = X + 1500;
        UAnimGraphNode_LocalToComponentSpace* ToComponent =
            CreateGraphNode<UAnimGraphNode_LocalToComponentSpace>(*AnimGraph, X, Y);
        UAnimGraphNode_ProceduralMovementLean* Lean =
            CreateGraphNode<UAnimGraphNode_ProceduralMovementLean>(*AnimGraph, X + 300, Y);
        UAnimGraphNode_ProceduralLookChain* Look =
            CreateGraphNode<UAnimGraphNode_ProceduralLookChain>(*AnimGraph, X + 600, Y);
        UAnimGraphNode_ProceduralImpulse* Impulse =
            CreateGraphNode<UAnimGraphNode_ProceduralImpulse>(*AnimGraph, X + 900, Y);
        UAnimGraphNode_ComponentToLocalSpace* ToLocal =
            CreateGraphNode<UAnimGraphNode_ComponentToLocalSpace>(*AnimGraph, X + 1200, Y);

        Lean->Node.RootBone.BoneName = TEXT("pelvis");
        Lean->Node.SpineBone.BoneName = TEXT("spine_01");
        Look->Node.LookBones.SetNum(5);
        Look->Node.LookBones[0].BoneName = TEXT("spine_01");
        Look->Node.LookBones[1].BoneName = TEXT("spine_02");
        Look->Node.LookBones[2].BoneName = TEXT("spine_03");
        Look->Node.LookBones[3].BoneName = TEXT("neck_01");
        Look->Node.LookBones[4].BoneName = TEXT("head");
        Impulse->Node.ImpulseBones.SetNum(3);
        Impulse->Node.ImpulseBones[0].Bone.BoneName = TEXT("pelvis");
        Impulse->Node.ImpulseBones[0].TranslationWeight = 0.35f;
        Impulse->Node.ImpulseBones[0].RotationWeight = 0.35f;
        Impulse->Node.ImpulseBones[1].Bone.BoneName = TEXT("spine_01");
        Impulse->Node.ImpulseBones[1].TranslationWeight = 0.15f;
        Impulse->Node.ImpulseBones[1].RotationWeight = 0.5f;
        Impulse->Node.ImpulseBones[2].Bone.BoneName = TEXT("spine_03");
        Impulse->Node.ImpulseBones[2].TranslationWeight = 0.05f;
        Impulse->Node.ImpulseBones[2].RotationWeight = 1.0f;
        Lean->ReconstructNode();
        Look->ReconstructNode();
        Impulse->ReconstructNode();

        const bool bPoseConnected =
            Connect(*AnimGraph, AuthoredOutput, FindPosePin(*ToComponent, EGPD_Input, FPoseLink::StaticStruct())) &&
            Connect(*AnimGraph, FindPosePin(*ToComponent, EGPD_Output, FComponentSpacePoseLink::StaticStruct()),
                FindPosePin(*Lean, EGPD_Input, FComponentSpacePoseLink::StaticStruct())) &&
            Connect(*AnimGraph, FindPosePin(*Lean, EGPD_Output, FComponentSpacePoseLink::StaticStruct()),
                FindPosePin(*Look, EGPD_Input, FComponentSpacePoseLink::StaticStruct())) &&
            Connect(*AnimGraph, FindPosePin(*Look, EGPD_Output, FComponentSpacePoseLink::StaticStruct()),
                FindPosePin(*Impulse, EGPD_Input, FComponentSpacePoseLink::StaticStruct())) &&
            Connect(*AnimGraph, FindPosePin(*Impulse, EGPD_Output, FComponentSpacePoseLink::StaticStruct()),
                FindPosePin(*ToLocal, EGPD_Input, FComponentSpacePoseLink::StaticStruct())) &&
            Connect(*AnimGraph, FindPosePin(*ToLocal, EGPD_Output, FPoseLink::StaticStruct()), RootInput);
        if (!bPoseConnected) return false;

        UK2Node_VariableGet* StateGetter = CreateGraphNode<UK2Node_VariableGet>(
            *AnimGraph, X + 500, Y + 450);
        StateGetter->VariableReference.SetSelfMember(StateVariable);
        StateGetter->ReconstructNode();
        UEdGraphPin* StateOutput = StateGetter->GetValuePin();
        if (!Connect(*AnimGraph, StateOutput, Lean->FindPin(TEXT("ProceduralState"), EGPD_Input)) ||
            !Connect(*AnimGraph, StateOutput, Look->FindPin(TEXT("ProceduralState"), EGPD_Input)) ||
            !Connect(*AnimGraph, StateOutput, Impulse->FindPin(TEXT("ProceduralState"), EGPD_Input)))
        {
            return false;
        }
        return true;
    }

    bool EnsureMemberVariable(UAnimBlueprint& Blueprint, FName Name, const FEdGraphPinType& Type)
    {
        return FBlueprintEditorUtils::FindNewVariableIndex(&Blueprint, Name) != INDEX_NONE
            || FBlueprintEditorUtils::AddMemberVariable(&Blueprint, Name, Type);
    }

    bool ApplyWeaponHandIK(UAnimBlueprint& Blueprint, bool bSolveRightHand, float MaximumShoulderShift)
    {
        TArray<UEdGraph*> Graphs;
        Blueprint.GetAllGraphs(Graphs);
        UEdGraph* AnimGraph = nullptr;
        for (UEdGraph* Graph : Graphs)
        {
            if (Graph && Graph->GetName() == TEXT("AnimGraph"))
            {
                AnimGraph = Graph;
                break;
            }
        }
        if (!AnimGraph) return false;

        FEdGraphPinType VectorType;
        VectorType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        FEdGraphPinType RotatorType;
        RotatorType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        RotatorType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
        FEdGraphPinType BoolType;
        BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        FEdGraphPinType FloatType;
        FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        if (!EnsureMemberVariable(Blueprint, TEXT("RightHandIKTarget"), VectorType)
            || !EnsureMemberVariable(Blueprint, TEXT("LeftHandIKTarget"), VectorType)
            || !EnsureMemberVariable(Blueprint, TEXT("RightHandIKRotation"), RotatorType)
            || !EnsureMemberVariable(Blueprint, TEXT("LeftHandIKRotation"), RotatorType)
            || !EnsureMemberVariable(Blueprint, TEXT("bRightHandIKRotationAdditive"), BoolType)
            || !EnsureMemberVariable(Blueprint, TEXT("bLeftHandIKRotationAdditive"), BoolType)
            || !EnsureMemberVariable(Blueprint, TEXT("bHandIKTargetsValid"), BoolType)
            || !EnsureMemberVariable(Blueprint, TEXT("HandIKADSAlpha"), FloatType)
            || !EnsureMemberVariable(Blueprint, TEXT("bHandIKLongGun"), BoolType)
            || !EnsureMemberVariable(Blueprint, TEXT("HandIKMaximumShoulderShift"), FloatType)
            || !EnsureMemberVariable(Blueprint, TEXT("HandIKBendTargetDistance"), FloatType)
            || !EnsureMemberVariable(Blueprint, TEXT("HandIKMaximumStretchScale"), FloatType)
            || !EnsureMemberVariable(Blueprint, TEXT("HandIKMinimumArmReachFraction"), FloatType)
            || !EnsureMemberVariable(Blueprint, TEXT("HandIKElbowOutwardBias"), FloatType)
            || !EnsureMemberVariable(Blueprint, TEXT("HandIKElbowDownwardBias"), FloatType)
            || !EnsureMemberVariable(Blueprint, TEXT("HandIKFingerCurlDegrees"), FloatType)
            || !EnsureMemberVariable(Blueprint, TEXT("HandIKThumbCurlDegrees"), FloatType))
        {
            return false;
        }

        for (UEdGraphNode* GraphNode : AnimGraph->Nodes)
        {
            if (UAnimGraphNode_ProceduralHandIK* Existing = Cast<UAnimGraphNode_ProceduralHandIK>(GraphNode))
            {
                Existing->Modify();
                Existing->Node.RightArm.PalmAnchorBone.BoneName = TEXT("middle_01_r");
                Existing->Node.LeftArm.PalmAnchorBone.BoneName = TEXT("middle_01_l");
                Existing->Node.bSolveRightHand = bSolveRightHand;
                Existing->Node.bSolveLeftHand = true;
                Existing->Node.bApplyProceduralFingerGrip = !bSolveRightHand;
                Existing->Node.MaximumShoulderShift = MaximumShoulderShift;
                Existing->ReconstructNode();

                const struct { FName Variable; FName Pin; } RuntimeBindings[] = {
                    { TEXT("RightHandIKRotation"), TEXT("RightPalmTargetRotation") },
                    { TEXT("LeftHandIKRotation"), TEXT("LeftPalmTargetRotation") },
                    { TEXT("bRightHandIKRotationAdditive"), TEXT("bRightPalmRotationIsAdditive") },
                    { TEXT("bLeftHandIKRotationAdditive"), TEXT("bLeftPalmRotationIsAdditive") },
                    { TEXT("HandIKMaximumShoulderShift"), TEXT("MaximumShoulderShift") },
                    { TEXT("HandIKBendTargetDistance"), TEXT("BendTargetDistance") },
                    { TEXT("HandIKMaximumStretchScale"), TEXT("MaximumStretchScale") },
                    { TEXT("HandIKMinimumArmReachFraction"), TEXT("MinimumArmReachFraction") },
                    { TEXT("HandIKElbowOutwardBias"), TEXT("ElbowOutwardBias") },
                    { TEXT("HandIKElbowDownwardBias"), TEXT("ElbowDownwardBias") },
                    { TEXT("HandIKFingerCurlDegrees"), TEXT("FingerCurlDegrees") },
                    { TEXT("HandIKThumbCurlDegrees"), TEXT("ThumbCurlDegrees") },
                };
                int32 GetterY = Existing->NodePosY + 550;
                for (const auto& Binding : RuntimeBindings)
                {
                    UEdGraphPin* TargetPin = Existing->FindPin(Binding.Pin, EGPD_Input);
                    if (!TargetPin)
                    {
                        UE_LOG(LogTemp, Error, TEXT("Hand IK runtime input pin not found: %s"),
                            *Binding.Pin.ToString());
                        return false;
                    }
                    if (TargetPin->LinkedTo.IsEmpty())
                    {
                        UK2Node_VariableGet* Getter = CreateGraphNode<UK2Node_VariableGet>(
                            *AnimGraph, Existing->NodePosX - 260, GetterY);
                        Getter->VariableReference.SetSelfMember(Binding.Variable);
                        Getter->ReconstructNode();
                        if (!Connect(*AnimGraph, Getter->GetValuePin(), TargetPin))
                        {
                            UE_LOG(LogTemp, Error, TEXT("Failed binding hand IK variable %s to pin %s"),
                                *Binding.Variable.ToString(), *Binding.Pin.ToString());
                            return false;
                        }
                    }
                    GetterY += 100;
                }
                UE_LOG(LogTemp, Display, TEXT("Updated existing weapon hand IK; SolveRight=%d."),
                    bSolveRightHand ? 1 : 0);
                return true;
            }
        }

        TArray<UAnimGraphNode_Root*> Roots;
        AnimGraph->GetNodesOfClass(Roots);
        if (Roots.Num() != 1) return false;
        UEdGraphPin* RootInput = FindPosePin(*Roots[0], EGPD_Input, FPoseLink::StaticStruct());
        if (!RootInput || RootInput->LinkedTo.Num() != 1) return false;
        UEdGraphPin* AuthoredOutput = RootInput->LinkedTo[0];
        RootInput->BreakLinkTo(AuthoredOutput);

        const int32 X = Roots[0]->NodePosX;
        const int32 Y = Roots[0]->NodePosY;
        Roots[0]->NodePosX = X + 900;
        UAnimGraphNode_LocalToComponentSpace* ToComponent =
            CreateGraphNode<UAnimGraphNode_LocalToComponentSpace>(*AnimGraph, X, Y);
        UAnimGraphNode_ProceduralHandIK* HandIK =
            CreateGraphNode<UAnimGraphNode_ProceduralHandIK>(*AnimGraph, X + 300, Y);
        UAnimGraphNode_ComponentToLocalSpace* ToLocal =
            CreateGraphNode<UAnimGraphNode_ComponentToLocalSpace>(*AnimGraph, X + 600, Y);

        HandIK->Node.RightArm.ClavicleBone.BoneName = TEXT("clavicle_r");
        HandIK->Node.RightArm.UpperArmBone.BoneName = TEXT("upperarm_r");
        HandIK->Node.RightArm.LowerArmBone.BoneName = TEXT("lowerarm_r");
        HandIK->Node.RightArm.HandBone.BoneName = TEXT("hand_r");
        HandIK->Node.RightArm.PalmAnchorBone.BoneName = TEXT("middle_01_r");
        HandIK->Node.LeftArm.ClavicleBone.BoneName = TEXT("clavicle_l");
        HandIK->Node.LeftArm.UpperArmBone.BoneName = TEXT("upperarm_l");
        HandIK->Node.LeftArm.LowerArmBone.BoneName = TEXT("lowerarm_l");
        HandIK->Node.LeftArm.HandBone.BoneName = TEXT("hand_l");
        HandIK->Node.LeftArm.PalmAnchorBone.BoneName = TEXT("middle_01_l");
        HandIK->Node.bSolveRightHand = bSolveRightHand;
        HandIK->Node.bSolveLeftHand = true;
        HandIK->Node.bApplyProceduralFingerGrip = !bSolveRightHand;
        HandIK->Node.MaximumShoulderShift = MaximumShoulderShift;
        HandIK->ReconstructNode();

        const bool bPoseConnected =
            Connect(*AnimGraph, AuthoredOutput, FindPosePin(*ToComponent, EGPD_Input, FPoseLink::StaticStruct()))
            && Connect(*AnimGraph, FindPosePin(*ToComponent, EGPD_Output, FComponentSpacePoseLink::StaticStruct()),
                FindPosePin(*HandIK, EGPD_Input, FComponentSpacePoseLink::StaticStruct()))
            && Connect(*AnimGraph, FindPosePin(*HandIK, EGPD_Output, FComponentSpacePoseLink::StaticStruct()),
                FindPosePin(*ToLocal, EGPD_Input, FComponentSpacePoseLink::StaticStruct()))
            && Connect(*AnimGraph, FindPosePin(*ToLocal, EGPD_Output, FPoseLink::StaticStruct()), RootInput);
        if (!bPoseConnected) return false;

        struct FPinBinding
        {
            FName Variable;
            FName Pin;
        };
        const FPinBinding Bindings[] = {
            { TEXT("RightHandIKTarget"), TEXT("RightHandTarget") },
            { TEXT("LeftHandIKTarget"), TEXT("LeftHandTarget") },
            { TEXT("RightHandIKRotation"), TEXT("RightPalmTargetRotation") },
            { TEXT("LeftHandIKRotation"), TEXT("LeftPalmTargetRotation") },
            { TEXT("bRightHandIKRotationAdditive"), TEXT("bRightPalmRotationIsAdditive") },
            { TEXT("bLeftHandIKRotationAdditive"), TEXT("bLeftPalmRotationIsAdditive") },
            { TEXT("bHandIKTargetsValid"), TEXT("bTargetsValid") },
            { TEXT("HandIKADSAlpha"), TEXT("ADSAlpha") },
            { TEXT("bHandIKLongGun"), TEXT("bLongGun") },
            { TEXT("HandIKMaximumShoulderShift"), TEXT("MaximumShoulderShift") },
            { TEXT("HandIKBendTargetDistance"), TEXT("BendTargetDistance") },
            { TEXT("HandIKMaximumStretchScale"), TEXT("MaximumStretchScale") },
            { TEXT("HandIKMinimumArmReachFraction"), TEXT("MinimumArmReachFraction") },
            { TEXT("HandIKElbowOutwardBias"), TEXT("ElbowOutwardBias") },
            { TEXT("HandIKElbowDownwardBias"), TEXT("ElbowDownwardBias") },
            { TEXT("HandIKFingerCurlDegrees"), TEXT("FingerCurlDegrees") },
            { TEXT("HandIKThumbCurlDegrees"), TEXT("ThumbCurlDegrees") },
        };
        int32 GetterY = Y + 350;
        for (const FPinBinding& Binding : Bindings)
        {
            UK2Node_VariableGet* Getter = CreateGraphNode<UK2Node_VariableGet>(
                *AnimGraph, X + 40, GetterY);
            Getter->VariableReference.SetSelfMember(Binding.Variable);
            Getter->ReconstructNode();
            if (!Connect(*AnimGraph, Getter->GetValuePin(), HandIK->FindPin(Binding.Pin, EGPD_Input)))
            {
                return false;
            }
            GetterY += 100;
        }
        return true;
    }
}

UProceduralAnimGraphCommandlet::UProceduralAnimGraphCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 UProceduralAnimGraphCommandlet::Main(const FString& Params)
{
    FString AssetPath;
    FParse::Value(*Params, TEXT("Asset="), AssetPath);
    if (AssetPath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralAnimGraph requires -Asset=/Game/Path/Asset"));
        return 1;
    }
    UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, *AssetPath);
    if (!Blueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load AnimBP %s"), *AssetPath);
        return 2;
    }

    if (FParse::Param(*Params, TEXT("Apply")))
    {
        Blueprint->Modify();
        const bool bFirstPersonHandIK = FParse::Param(*Params, TEXT("FirstPersonHandIK"));
        const bool bThirdPersonHandIK = FParse::Param(*Params, TEXT("ThirdPersonHandIK"));
        const bool bApplied = (bFirstPersonHandIK || bThirdPersonHandIK)
            ? ApplyWeaponHandIK(*Blueprint, !bThirdPersonHandIK, bFirstPersonHandIK ? 4.0f : 12.0f)
            : ApplyProceduralChain(*Blueprint);
        if (!bApplied)
        {
            UE_LOG(LogTemp, Error, TEXT("Guarded procedural graph insertion failed for %s"), *AssetPath);
            return 3;
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (Blueprint->Status == BS_Error)
        {
            UE_LOG(LogTemp, Error, TEXT("AnimBP compilation failed after insertion; package was not saved."));
            return 4;
        }
        const FString Filename = FPackageName::LongPackageNameToFilename(
            Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, SaveArgs))
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to save %s"), *Filename);
            return 5;
        }
        UE_LOG(LogTemp, Display, TEXT("PROC_APPLY_SUCCESS Asset=%s File=%s"), *AssetPath, *Filename);
    }

    UE_LOG(LogTemp, Display, TEXT("PROC_INSPECT Asset=%s Skeleton=%s Variables=%d"),
        *Blueprint->GetPathName(), *GetNameSafe(Blueprint->TargetSkeleton), Blueprint->NewVariables.Num());
    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        UE_LOG(LogTemp, Display, TEXT("PROC_VAR Name=%s Category=%s SubCategory=%s Object=%s"),
            *Variable.VarName.ToString(), *Variable.VarType.PinCategory.ToString(),
            *Variable.VarType.PinSubCategory.ToString(),
            *GetNameSafe(Variable.VarType.PinSubCategoryObject.Get()));
    }
    if (Blueprint->TargetSkeleton)
    {
        const FReferenceSkeleton& RefSkeleton = Blueprint->TargetSkeleton->GetReferenceSkeleton();
        FString BoneList;
        for (int32 Index = 0; Index < RefSkeleton.GetNum(); ++Index)
        {
            if (!BoneList.IsEmpty()) BoneList += TEXT(",");
            BoneList += RefSkeleton.GetBoneName(Index).ToString();
        }
        UE_LOG(LogTemp, Display, TEXT("PROC_BONES %s"), *BoneList);
    }

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (const UEdGraph* Graph : Graphs)
    {
        UE_LOG(LogTemp, Display, TEXT("PROC_GRAPH Name=%s Schema=%s Nodes=%d"),
            *Graph->GetName(), *GetNameSafe(Graph->GetSchema()), Graph->Nodes.Num());
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            UE_LOG(LogTemp, Display, TEXT("PROC_NODE Graph=%s Class=%s Name=%s Title=%s X=%d Y=%d"),
                *Graph->GetName(), *Node->GetClass()->GetName(), *Node->GetName(),
                *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"), TEXT(" | ")),
                Node->NodePosX, Node->NodePosY);
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                UE_LOG(LogTemp, Display,
                    TEXT("PROC_PIN Node=%s Name=%s Dir=%s Category=%s SubObject=%s Links=%d Hidden=%d"),
                    *Node->GetName(), *Pin->PinName.ToString(),
                    Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out"),
                    *Pin->PinType.PinCategory.ToString(),
                    *GetNameSafe(Pin->PinType.PinSubCategoryObject.Get()),
                    Pin->LinkedTo.Num(), Pin->bHidden ? 1 : 0);
            }
        }
    }
    return 0;
}
