#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Data/ProceduralCharacterTypes.h"
#include "AnimNode_ProceduralMovementLean.generated.h"

/** Lightweight acceleration and turn lean applied after authored locomotion. */
USTRUCT(BlueprintInternalUseOnly)
struct PROCEDURALCHARACTERSRUNTIME_API FAnimNode_ProceduralMovementLean : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Bones") FBoneReference RootBone;
    UPROPERTY(EditAnywhere, Category = "Bones") FBoneReference SpineBone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State", meta = (PinShownByDefault))
    FProceduralCharacterState ProceduralState;
    UPROPERTY(EditAnywhere, Category = "Performance")
    EProceduralAnimTier MaximumAllowedTier = EProceduralAnimTier::NearbyCrowd;
    UPROPERTY(EditAnywhere, Category = "Lean", meta = (ClampMin = "1.0")) float AccelerationForFullLean = 1200.0f;
    UPROPERTY(EditAnywhere, Category = "Lean", meta = (ClampMin = "1.0")) float TurnDegreesPerSecondForFullLean = 180.0f;
    UPROPERTY(EditAnywhere, Category = "Lean") float MaximumForwardLeanDegrees = 8.0f;
    UPROPERTY(EditAnywhere, Category = "Lean") float MaximumSideLeanDegrees = 10.0f;
    UPROPERTY(EditAnywhere, Category = "Lean") float MaximumTurnLeanDegrees = 6.0f;
    UPROPERTY(EditAnywhere, Category = "Lean", meta = (ClampMin = "0.0")) float InterpolationSpeed = 10.0f;
    UPROPERTY(EditAnywhere, Category = "Lean", meta = (ClampMin = "0.0", ClampMax = "1.0")) float SpineShare = 0.5f;

    virtual void GatherDebugData(FNodeDebugData& DebugData) override;

protected:
    virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
    virtual void EvaluateSkeletalControl_AnyThread(
        FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

private:
    FRotator SmoothedLean = FRotator::ZeroRotator;
};
