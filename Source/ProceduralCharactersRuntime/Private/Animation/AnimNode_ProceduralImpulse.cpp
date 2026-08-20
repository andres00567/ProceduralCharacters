#include "Animation/AnimNode_ProceduralImpulse.h"

#include "Animation/AnimInstanceProxy.h"

DECLARE_CYCLE_STAT(TEXT("ProceduralAnim.AnimNode.Impulse"), STAT_ProceduralAnimImpulse, STATGROUP_Anim);

void FAnimNode_ProceduralImpulse::GatherDebugData(FNodeDebugData& DebugData)
{
    FString Line = DebugData.GetNodeName(this);
    Line += FString::Printf(TEXT(" (Tier P%d Alpha %.2f)"),
        static_cast<int32>(ProceduralState.LOD.Tier), ProceduralState.ProceduralImpulseAlpha);
    DebugData.AddDebugItem(Line);
    ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_ProceduralImpulse::EvaluateSkeletalControl_AnyThread(
    FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
    SCOPE_CYCLE_COUNTER(STAT_ProceduralAnimImpulse);
    const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
    const FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();
    FVector Translation = ComponentTransform.InverseTransformVectorNoScale(
        ProceduralState.ProceduralLinearImpulse) * TranslationScale;
    Translation = Translation.GetClampedToMaxSize(MaximumTranslation);
    FVector RotationVector = ComponentTransform.InverseTransformVectorNoScale(
        ProceduralState.ProceduralAngularImpulse) * RotationScale;
    RotationVector = RotationVector.GetClampedToMaxSize(MaximumRotationDegrees);
    const float Quality = ProceduralState.LOD.QualityMultiplier;
    for (const FProceduralImpulseBone& Entry : ImpulseBones)
    {
        if (!Entry.Bone.IsValidToEvaluate(Bones)) continue;
        const FCompactPoseBoneIndex Index = Entry.Bone.GetCompactPoseIndex(Bones);
        FTransform Transform = Output.Pose.GetComponentSpaceTransform(Index);
        Transform.AddToTranslation(Translation * Entry.TranslationWeight * Quality);
        const FRotator Rotation(
            RotationVector.Y * Entry.RotationWeight * Quality,
            RotationVector.Z * Entry.RotationWeight * Quality,
            RotationVector.X * Entry.RotationWeight * Quality);
        Transform.SetRotation(FQuat(Rotation) * Transform.GetRotation());
        Transform.NormalizeRotation();
        OutBoneTransforms.Add(FBoneTransform(Index, Transform));
    }
    OutBoneTransforms.Sort([](const FBoneTransform& A, const FBoneTransform& B)
    {
        return A.BoneIndex < B.BoneIndex;
    });
}

bool FAnimNode_ProceduralImpulse::IsValidToEvaluate(
    const USkeleton*, const FBoneContainer& RequiredBones)
{
    if (ProceduralState.LOD.Tier > MaximumAllowedTier ||
        ProceduralState.ProceduralImpulseAlpha <= KINDA_SMALL_NUMBER ||
        (ProceduralState.ActiveImpulseFeatureMask & ProceduralState.LOD.EnabledFeatureMask) == 0)
    {
        return false;
    }
    return ImpulseBones.ContainsByPredicate([&RequiredBones](const FProceduralImpulseBone& Entry)
    {
        return Entry.Bone.IsValidToEvaluate(RequiredBones);
    });
}

void FAnimNode_ProceduralImpulse::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    for (FProceduralImpulseBone& Entry : ImpulseBones) Entry.Bone.Initialize(RequiredBones);
}
