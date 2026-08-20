#include "Animation/AnimNode_ProceduralHandIK.h"

#include "Animation/AnimInstanceProxy.h"
#include "TwoBoneIK.h"

DECLARE_CYCLE_STAT(TEXT("ProceduralAnim.AnimNode.HandIK"), STAT_ProceduralAnimHandIK, STATGROUP_Anim);

void FAnimNode_ProceduralHandIK::GatherDebugData(FNodeDebugData& DebugData)
{
    FString Line = DebugData.GetNodeName(this);
    Line += FString::Printf(TEXT(" (Valid %d ADS %.2f LongGun %d)"),
        bTargetsValid ? 1 : 0, ADSAlpha, bLongGun ? 1 : 0);
    DebugData.AddDebugItem(Line);
    ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_ProceduralHandIK::EvaluateSkeletalControl_AnyThread(
    FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
    SCOPE_CYCLE_COUNTER(STAT_ProceduralAnimHandIK);
    if (bSolveRightHand)
    {
        SolveArm(RightArm, RightHandTarget, RightPalmTargetRotation,
            bRightPalmRotationIsAdditive, Output, OutBoneTransforms);
    }
    if (bSolveLeftHand)
    {
        SolveArm(LeftArm, LeftHandTarget, LeftPalmTargetRotation,
            bLeftPalmRotationIsAdditive, Output, OutBoneTransforms);
    }
    if (bApplyProceduralFingerGrip)
    {
        // TPV owns only the support hand. Never overwrite the authored right
        // hand/finger pose that carries the weapon.
        ApplyFingerGrip(LeftArm, LeftFingerChains, Output, OutBoneTransforms);
    }
    OutBoneTransforms.Sort([](const FBoneTransform& A, const FBoneTransform& B)
    {
        return A.BoneIndex < B.BoneIndex;
    });
}

void FAnimNode_ProceduralHandIK::ApplyFingerGrip(
    const FProceduralArmIKChain& Arm,
    const TArray<TArray<FBoneReference>>& FingerChains,
    FComponentSpacePoseContext& Output,
    TArray<FBoneTransform>& OutBoneTransforms) const
{
    const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
    const FCompactPoseBoneIndex HandIndex = Arm.HandBone.GetCompactPoseIndex(Bones);
    const FTransform OriginalHand = Output.Pose.GetComponentSpaceTransform(HandIndex);
    FTransform ModifiedHand = OriginalHand;
    for (const FBoneTransform& Transform : OutBoneTransforms)
    {
        if (Transform.BoneIndex == HandIndex)
        {
            ModifiedHand = Transform.Transform;
            break;
        }
    }

    for (int32 ChainIndex = 0; ChainIndex < FingerChains.Num(); ++ChainIndex)
    {
        const TArray<FBoneReference>& Chain = FingerChains[ChainIndex];
        FTransform OriginalParent = OriginalHand;
        FTransform ModifiedParent = ModifiedHand;
        const bool bThumb = ChainIndex == 0;
        const float BaseDegrees = bThumb ? ThumbCurlDegrees : FingerCurlDegrees;
        for (int32 SegmentIndex = 0; SegmentIndex < Chain.Num(); ++SegmentIndex)
        {
            if (!Chain[SegmentIndex].IsValidToEvaluate(Bones))
            {
                break;
            }
            const FCompactPoseBoneIndex BoneIndex = Chain[SegmentIndex].GetCompactPoseIndex(Bones);
            const FTransform OriginalBone = Output.Pose.GetComponentSpaceTransform(BoneIndex);
            FTransform LocalBone = OriginalBone.GetRelativeTransform(OriginalParent);
            const float SegmentScale = SegmentIndex == 0 ? 0.72f
                : (SegmentIndex == 1 ? 1.0f : 0.68f);
            const FQuat Curl(FVector::RightVector,
                FMath::DegreesToRadians(-BaseDegrees * SegmentScale));
            LocalBone.SetRotation(LocalBone.GetRotation() * Curl);
            const FTransform ModifiedBone = LocalBone * ModifiedParent;
            OutBoneTransforms.Add(FBoneTransform(BoneIndex, ModifiedBone));
            OriginalParent = OriginalBone;
            ModifiedParent = ModifiedBone;
        }
    }
}

void FAnimNode_ProceduralHandIK::SolveArm(
    const FProceduralArmIKChain& Chain,
    const FVector& PalmTarget,
    const FRotator& PalmTargetRotation,
    const bool bPalmRotationIsAdditive,
    FComponentSpacePoseContext& Output,
    TArray<FBoneTransform>& OutBoneTransforms) const
{
    const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
    const FCompactPoseBoneIndex ClavicleIndex = Chain.ClavicleBone.GetCompactPoseIndex(Bones);
    const FCompactPoseBoneIndex UpperIndex = Chain.UpperArmBone.GetCompactPoseIndex(Bones);
    const FCompactPoseBoneIndex LowerIndex = Chain.LowerArmBone.GetCompactPoseIndex(Bones);
    const FCompactPoseBoneIndex HandIndex = Chain.HandBone.GetCompactPoseIndex(Bones);
    const FCompactPoseBoneIndex PalmAnchorIndex = Chain.PalmAnchorBone.GetCompactPoseIndex(Bones);

    FTransform Clavicle = Output.Pose.GetComponentSpaceTransform(ClavicleIndex);
    FTransform Upper = Output.Pose.GetComponentSpaceTransform(UpperIndex);
    FTransform Lower = Output.Pose.GetComponentSpaceTransform(LowerIndex);
    FTransform Hand = Output.Pose.GetComponentSpaceTransform(HandIndex);
    const FTransform PalmAnchor = Output.Pose.GetComponentSpaceTransform(PalmAnchorIndex);

    // middle_01 is useful as a palm position marker, but its axes are finger
    // axes and must never orient the wrist. Build a stable grip frame at that
    // position using hand_r/l orientation, then preserve its offset from wrist.
    const FTransform IncomingGripFrame(Hand.GetRotation(), PalmAnchor.GetTranslation());
    const FTransform GripFrameRelativeToHand = IncomingGripFrame.GetRelativeTransform(Hand);
    // Identity sockets carry only the weapon's rotational delta. Apply that
    // delta to the current animation pose so the grip stays authored while it
    // follows lean/recoil. Rotated sockets remain absolute artist targets.
    const FQuat DesiredPalmRotation = bPalmRotationIsAdditive
        ? PalmTargetRotation.Quaternion() * Hand.GetRotation()
        : PalmTargetRotation.Quaternion();
    const FTransform DesiredPalm(DesiredPalmRotation, PalmTarget);
    const FTransform DesiredHand = GripFrameRelativeToHand.Inverse() * DesiredPalm;
    const FVector Effector = DesiredHand.GetTranslation();

    const float UpperLength = FVector::Distance(Upper.GetTranslation(), Lower.GetTranslation());
    const float LowerLength = FVector::Distance(Lower.GetTranslation(), Hand.GetTranslation());
    const float Reach = FMath::Max(UpperLength + LowerLength, 1.0f);
    const FVector RootToEffector = Effector - Upper.GetTranslation();
    const float RequiredReach = RootToEffector.Size();

    // Rifle/pistol and hip/ADS are constraint variants, not separate clips.
    // ADS permits a firmer shoulder tuck; pistols keep the shoulder travel tighter.
    const float StanceShoulderScale = (bLongGun ? 1.0f : 0.8f)
        * FMath::Lerp(0.75f, 1.0f, FMath::Clamp(ADSAlpha, 0.0f, 1.0f));
    const float AllowedShoulderShift = MaximumShoulderShift * StanceShoulderScale;
    if (AllowedShoulderShift > KINDA_SMALL_NUMBER)
    {
        const float MaximumComfortableReach = Reach * 0.98f;
        float ShiftAmount = 0.0f;
        if (RequiredReach > MaximumComfortableReach)
        {
            ShiftAmount = FMath::Min(RequiredReach - MaximumComfortableReach, AllowedShoulderShift);
        }
        if (ShiftAmount > KINDA_SMALL_NUMBER)
        {
            const FVector Shift = RootToEffector.GetSafeNormal() * ShiftAmount;
            Clavicle.AddToTranslation(Shift);
            Upper.AddToTranslation(Shift);
            Lower.AddToTranslation(Shift);
            Hand.AddToTranslation(Shift);
            OutBoneTransforms.Add(FBoneTransform(ClavicleIndex, Clavicle));
        }
    }

    const FVector SolveAxis = (Effector - Upper.GetTranslation()).GetSafeNormal(
        UE_SMALL_NUMBER, FVector::ForwardVector);
    FVector BendDirection = Lower.GetTranslation() - Upper.GetTranslation();
    BendDirection -= SolveAxis * FVector::DotProduct(BendDirection, SolveAxis);
    if (!BendDirection.Normalize())
    {
        // Stable anatomical fallback only; this is not a weapon-authored pole target.
        BendDirection = FVector::CrossProduct(SolveAxis, FVector::UpVector).GetSafeNormal(
            UE_SMALL_NUMBER, FVector::RightVector);
    }

    const float StanceBendScale = (bLongGun ? 1.0f : 0.78f)
        * FMath::Lerp(1.0f, 0.82f, FMath::Clamp(ADSAlpha, 0.0f, 1.0f));
    const FVector JointTarget = Upper.GetTranslation()
        + SolveAxis * UpperLength * 0.5f
        + BendDirection * BendTargetDistance * StanceBendScale;

    AnimationCore::SolveTwoBoneIK(
        Upper,
        Lower,
        Hand,
        JointTarget,
        Effector,
        true,
        0.98,
        MaximumStretchScale);

    // Two-bone IK owns translation and elbow bend; the socket owns the palm
    // orientation. middle_01_* supplies contact position only.
    Hand.SetRotation(DesiredHand.GetRotation());

    OutBoneTransforms.Add(FBoneTransform(UpperIndex, Upper));
    OutBoneTransforms.Add(FBoneTransform(LowerIndex, Lower));
    OutBoneTransforms.Add(FBoneTransform(HandIndex, Hand));
}

bool FAnimNode_ProceduralHandIK::IsValidToEvaluate(
    const USkeleton*, const FBoneContainer& RequiredBones)
{
    const auto IsChainValid = [&RequiredBones](const FProceduralArmIKChain& Chain)
    {
        return Chain.ClavicleBone.IsValidToEvaluate(RequiredBones)
            && Chain.UpperArmBone.IsValidToEvaluate(RequiredBones)
            && Chain.LowerArmBone.IsValidToEvaluate(RequiredBones)
            && Chain.HandBone.IsValidToEvaluate(RequiredBones)
            && Chain.PalmAnchorBone.IsValidToEvaluate(RequiredBones);
    };
    return bTargetsValid
        && (bSolveRightHand || bSolveLeftHand)
        && (!bSolveRightHand || IsChainValid(RightArm))
        && (!bSolveLeftHand || IsChainValid(LeftArm));
}

void FAnimNode_ProceduralHandIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    const auto InitializeChain = [&RequiredBones](FProceduralArmIKChain& Chain)
    {
        Chain.ClavicleBone.Initialize(RequiredBones);
        Chain.UpperArmBone.Initialize(RequiredBones);
        Chain.LowerArmBone.Initialize(RequiredBones);
        Chain.HandBone.Initialize(RequiredBones);
        Chain.PalmAnchorBone.Initialize(RequiredBones);
    };
    InitializeChain(RightArm);
    InitializeChain(LeftArm);

    const auto InitializeFingers = [&RequiredBones](
        TArray<TArray<FBoneReference>>& Chains, const TCHAR* Side)
    {
        Chains.Reset();
        static const TCHAR* FingerNames[] = {
            TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky")
        };
        for (const TCHAR* FingerName : FingerNames)
        {
            TArray<FBoneReference>& Chain = Chains.AddDefaulted_GetRef();
            for (int32 Segment = 1; Segment <= 3; ++Segment)
            {
                FBoneReference& Bone = Chain.AddDefaulted_GetRef();
                Bone.BoneName = FName(*FString::Printf(TEXT("%s_0%d_%s"),
                    FingerName, Segment, Side));
                Bone.Initialize(RequiredBones);
            }
        }
    };
    InitializeFingers(RightFingerChains, TEXT("r"));
    InitializeFingers(LeftFingerChains, TEXT("l"));
}
