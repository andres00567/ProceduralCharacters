#include "Animation/AnimGraphNode_ProceduralMovementLean.h"

#include "Kismet2/CompilerResultsLog.h"

#define LOCTEXT_NAMESPACE "ProceduralMovementLean"

FText UAnimGraphNode_ProceduralMovementLean::GetNodeTitle(ENodeTitleType::Type) const
{
    return GetControllerDescription();
}

FText UAnimGraphNode_ProceduralMovementLean::GetTooltipText() const
{
    return LOCTEXT("Tooltip", "Adds budgeted native acceleration and turn lean to an authored component-space pose.");
}

FString UAnimGraphNode_ProceduralMovementLean::GetNodeCategory() const
{
    return TEXT("Procedural Animation");
}

FText UAnimGraphNode_ProceduralMovementLean::GetControllerDescription() const
{
    return LOCTEXT("Title", "Procedural Movement Lean");
}

void UAnimGraphNode_ProceduralMovementLean::ValidateAnimNodeDuringCompilation(
    USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
    if (Node.RootBone.BoneName == NAME_None)
    {
        MessageLog.Warning(TEXT("@@ requires a root or pelvis bone."), this);
    }
    Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

#undef LOCTEXT_NAMESPACE
