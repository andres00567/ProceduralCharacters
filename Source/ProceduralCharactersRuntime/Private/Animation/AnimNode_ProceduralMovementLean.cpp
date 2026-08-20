#include "Animation/AnimNode_ProceduralMovementLean.h"

#include "Animation/AnimInstanceProxy.h"

DECLARE_CYCLE_STAT(TEXT("ProceduralAnim.AnimNode.Lean"), STAT_ProceduralAnimLean, STATGROUP_Anim);

void FAnimNode_ProceduralMovementLean::GatherDebugData(FNodeDebugData& DebugData)
{
    FString Line = DebugData.GetNodeName(this);
    Line += FString::Printf(TEXT(" (Tier P%d Root %s)"),
        static_cast<int32>(ProceduralState.LOD.Tier), *RootBone.BoneName.ToString());
    DebugData.AddDebugItem(Line);
    ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_ProceduralMovementLean::UpdateInternal(const FAnimationUpdateContext& Context)
{
    Super::UpdateInternal(Context);
    const int32 Mask = ProceduralState.LOD.EnabledFeatureMask;
    const bool bEnabled = ProceduralState.LOD.Tier <= MaximumAllowedTier &&
        ((Mask & static_cast<int32>(EProceduralFeature::MovementLean)) != 0 ||
         (Mask & static_cast<int32>(EProceduralFeature::TurnLean)) != 0);
    if (!bEnabled)
    {
        SmoothedLean = FRotator::ZeroRotator;
        return;
    }
    const float AccelScale = FMath::Max(AccelerationForFullLean, 1.0f);
    const float TurnScale = FMath::Max(TurnDegreesPerSecondForFullLean, 1.0f);
    FRotator Target;
    Target.Pitch = FMath::Clamp(-ProceduralState.LocalAcceleration.X / AccelScale, -1.0f, 1.0f) * MaximumForwardLeanDegrees;
    Target.Roll = FMath::Clamp(ProceduralState.LocalAcceleration.Y / AccelScale, -1.0f, 1.0f) * MaximumSideLeanDegrees;
    Target.Yaw = FMath::Clamp(ProceduralState.AngularVelocity.Z / TurnScale, -1.0f, 1.0f) * MaximumTurnLeanDegrees;
    Target *= ProceduralState.LOD.QualityMultiplier;
    SmoothedLean = FMath::RInterpTo(
        SmoothedLean, Target, Context.GetDeltaTime(), InterpolationSpeed);
}

void FAnimNode_ProceduralMovementLean::EvaluateSkeletalControl_AnyThread(
    FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
    SCOPE_CYCLE_COUNTER(STAT_ProceduralAnimLean);
    const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
    const auto AddRotation = [&Output, &OutBoneTransforms, &Bones](
        const FBoneReference& Bone, const FRotator& Rotation)
    {
        if (!Bone.IsValidToEvaluate(Bones) || Rotation.IsNearlyZero()) return;
        const FCompactPoseBoneIndex Index = Bone.GetCompactPoseIndex(Bones);
        FTransform Transform = Output.Pose.GetComponentSpaceTransform(Index);
        Transform.SetRotation(FQuat(Rotation) * Transform.GetRotation());
        Transform.NormalizeRotation();
        OutBoneTransforms.Add(FBoneTransform(Index, Transform));
    };
    const float ClampedSpineShare = FMath::Clamp(SpineShare, 0.0f, 1.0f);
    AddRotation(RootBone, SmoothedLean * (1.0f - ClampedSpineShare));
    AddRotation(SpineBone, SmoothedLean * ClampedSpineShare);
    OutBoneTransforms.Sort([](const FBoneTransform& A, const FBoneTransform& B)
    {
        return A.BoneIndex < B.BoneIndex;
    });
}

bool FAnimNode_ProceduralMovementLean::IsValidToEvaluate(
    const USkeleton*, const FBoneContainer& RequiredBones)
{
    const int32 Mask = ProceduralState.LOD.EnabledFeatureMask;
    return ProceduralState.LOD.Tier <= MaximumAllowedTier &&
        ((Mask & static_cast<int32>(EProceduralFeature::MovementLean)) != 0 ||
         (Mask & static_cast<int32>(EProceduralFeature::TurnLean)) != 0) &&
        RootBone.IsValidToEvaluate(RequiredBones);
}

void FAnimNode_ProceduralMovementLean::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    RootBone.Initialize(RequiredBones);
    SpineBone.Initialize(RequiredBones);
}
