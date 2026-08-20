#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Data/ProceduralCharacterTypes.h"
#include "AnimNode_ProceduralImpulse.generated.h"

USTRUCT(BlueprintType)
struct PROCEDURALCHARACTERSRUNTIME_API FProceduralImpulseBone
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Bone") FBoneReference Bone;
    UPROPERTY(EditAnywhere, Category = "Bone", meta = (ClampMin = "0.0")) float TranslationWeight = 1.0f;
    UPROPERTY(EditAnywhere, Category = "Bone", meta = (ClampMin = "0.0")) float RotationWeight = 1.0f;
};

/** Applies the component's fixed-buffer recoil/landing/hit aggregate without UObject access. */
USTRUCT(BlueprintInternalUseOnly)
struct PROCEDURALCHARACTERSRUNTIME_API FAnimNode_ProceduralImpulse : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Bones") TArray<FProceduralImpulseBone> ImpulseBones;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State", meta = (PinShownByDefault))
    FProceduralCharacterState ProceduralState;
    UPROPERTY(EditAnywhere, Category = "Performance")
    EProceduralAnimTier MaximumAllowedTier = EProceduralAnimTier::NearbyCrowd;
    UPROPERTY(EditAnywhere, Category = "Impulse", meta = (ClampMin = "0.0")) float TranslationScale = 0.01f;
    UPROPERTY(EditAnywhere, Category = "Impulse", meta = (ClampMin = "0.0")) float RotationScale = 0.01f;
    UPROPERTY(EditAnywhere, Category = "Impulse", meta = (ClampMin = "0.0")) float MaximumTranslation = 12.0f;
    UPROPERTY(EditAnywhere, Category = "Impulse", meta = (ClampMin = "0.0")) float MaximumRotationDegrees = 18.0f;

    virtual void GatherDebugData(FNodeDebugData& DebugData) override;

protected:
    virtual void EvaluateSkeletalControl_AnyThread(
        FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
};
