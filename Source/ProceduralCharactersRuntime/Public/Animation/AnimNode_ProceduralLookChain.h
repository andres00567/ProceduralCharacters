#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Data/ProceduralCharacterTypes.h"
#include "AnimNode_ProceduralLookChain.generated.h"

/** Distributes a clamped world-target look correction over a cached bone chain. */
USTRUCT(BlueprintInternalUseOnly)
struct PROCEDURALCHARACTERSRUNTIME_API FAnimNode_ProceduralLookChain : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

    /** Root-to-tip order. P2 automatically uses at most the final two bones. */
    UPROPERTY(EditAnywhere, Category = "Bones") TArray<FBoneReference> LookBones;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State", meta = (PinShownByDefault))
    FProceduralCharacterState ProceduralState;
    UPROPERTY(EditAnywhere, Category = "Performance")
    EProceduralAnimTier MaximumAllowedTier = EProceduralAnimTier::NearbyCrowd;
    UPROPERTY(EditAnywhere, Category = "Look") FVector BoneForwardAxis = FVector::ForwardVector;
    UPROPERTY(EditAnywhere, Category = "Look", meta = (ClampMin = "0.0", ClampMax = "180.0")) float MaximumAngleDegrees = 70.0f;
    UPROPERTY(EditAnywhere, Category = "Look", meta = (ClampMin = "0.0")) float Strength = 1.0f;

    virtual void GatherDebugData(FNodeDebugData& DebugData) override;

protected:
    virtual void EvaluateSkeletalControl_AnyThread(
        FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
};
