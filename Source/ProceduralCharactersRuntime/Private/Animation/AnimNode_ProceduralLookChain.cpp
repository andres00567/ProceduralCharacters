#include "Animation/AnimNode_ProceduralLookChain.h"

#include "Animation/AnimInstanceProxy.h"

DECLARE_CYCLE_STAT(TEXT("ProceduralAnim.AnimNode.Look"), STAT_ProceduralAnimLook, STATGROUP_Anim);

void FAnimNode_ProceduralLookChain::GatherDebugData(FNodeDebugData& DebugData)
{
    FString Line = DebugData.GetNodeName(this);
    Line += FString::Printf(TEXT(" (Tier P%d Bones %d Target %s)"),
        static_cast<int32>(ProceduralState.LOD.Tier), LookBones.Num(),
        ProceduralState.bHasLookTarget ? TEXT("yes") : TEXT("no"));
    DebugData.AddDebugItem(Line);
    ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_ProceduralLookChain::EvaluateSkeletalControl_AnyThread(
    FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
    SCOPE_CYCLE_COUNTER(STAT_ProceduralAnimLook);
    const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
    const FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();
    const FVector TargetCS = ComponentTransform.InverseTransformPosition(ProceduralState.LookTargetWorld);
    const int32 TierLimit = ProceduralState.LOD.Tier == EProceduralAnimTier::NearbyCrowd ? 2 : LookBones.Num();
    const int32 FirstBone = FMath::Max(0, LookBones.Num() - TierLimit);
    int32 ValidBoneCount = 0;
    for (int32 Index = FirstBone; Index < LookBones.Num(); ++Index)
    {
        ValidBoneCount += LookBones[Index].IsValidToEvaluate(Bones) ? 1 : 0;
    }
    if (ValidBoneCount == 0) return;
    const float PerBoneStrength = FMath::Clamp(
        Strength * ProceduralState.LOD.QualityMultiplier / ValidBoneCount, 0.0f, 1.0f);
    const FVector SafeForward = BoneForwardAxis.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
    for (int32 BoneArrayIndex = FirstBone; BoneArrayIndex < LookBones.Num(); ++BoneArrayIndex)
    {
        const FBoneReference& Bone = LookBones[BoneArrayIndex];
        if (!Bone.IsValidToEvaluate(Bones)) continue;
        const FCompactPoseBoneIndex Index = Bone.GetCompactPoseIndex(Bones);
        FTransform Transform = Output.Pose.GetComponentSpaceTransform(Index);
        const FVector Desired = (TargetCS - Transform.GetLocation()).GetSafeNormal();
        if (Desired.IsNearlyZero()) continue;
        const FVector Current = Transform.GetRotation().RotateVector(SafeForward).GetSafeNormal();
        FQuat Delta = FQuat::FindBetweenNormals(Current, Desired);
        FVector Axis;
        double Angle;
        Delta.ToAxisAndAngle(Axis, Angle);
        Angle = FMath::Min(FMath::Abs(FMath::UnwindRadians(Angle)),
            FMath::DegreesToRadians(MaximumAngleDegrees));
        Delta = FQuat(Axis.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector), Angle * PerBoneStrength);
        Transform.SetRotation(Delta * Transform.GetRotation());
        Transform.NormalizeRotation();
        OutBoneTransforms.Add(FBoneTransform(Index, Transform));
    }
    OutBoneTransforms.Sort([](const FBoneTransform& A, const FBoneTransform& B)
    {
        return A.BoneIndex < B.BoneIndex;
    });
}

bool FAnimNode_ProceduralLookChain::IsValidToEvaluate(
    const USkeleton*, const FBoneContainer& RequiredBones)
{
    if (!ProceduralState.bHasLookTarget ||
        ProceduralState.LOD.Tier > MaximumAllowedTier ||
        !IsProceduralFeatureEnabled(ProceduralState.LOD, EProceduralFeature::LookAt))
    {
        return false;
    }
    return LookBones.ContainsByPredicate([&RequiredBones](const FBoneReference& Bone)
    {
        return Bone.IsValidToEvaluate(RequiredBones);
    });
}

void FAnimNode_ProceduralLookChain::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    for (FBoneReference& Bone : LookBones) Bone.Initialize(RequiredBones);
}
