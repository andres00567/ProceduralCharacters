#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_ProceduralHandIK.generated.h"

USTRUCT(BlueprintType)
struct PROCEDURALCHARACTERSRUNTIME_API FProceduralArmIKChain
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Bones") FBoneReference ClavicleBone;
    UPROPERTY(EditAnywhere, Category = "Bones") FBoneReference UpperArmBone;
    UPROPERTY(EditAnywhere, Category = "Bones") FBoneReference LowerArmBone;
    UPROPERTY(EditAnywhere, Category = "Bones") FBoneReference HandBone;
    /** Position-only marker near the middle of the palm (normally middle_01_*).
     *  Its finger rotation is deliberately ignored; HandBone supplies the grip axes. */
    UPROPERTY(EditAnywhere, Category = "Bones") FBoneReference PalmAnchorBone;
};

/**
 * Solves both first-person arms from weapon-authored hand positions. Elbow bend
 * planes come from the incoming pose and shoulder reach is derived at runtime;
 * weapons never need elbow or shoulder sockets.
 */
USTRUCT(BlueprintInternalUseOnly)
struct PROCEDURALCHARACTERSRUNTIME_API FAnimNode_ProceduralHandIK : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Bones") FProceduralArmIKChain RightArm;
    UPROPERTY(EditAnywhere, Category = "Bones") FProceduralArmIKChain LeftArm;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targets", meta = (PinShownByDefault))
    FVector RightHandTarget = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targets", meta = (PinShownByDefault))
    FVector LeftHandTarget = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targets", meta = (PinShownByDefault))
    FRotator RightPalmTargetRotation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targets", meta = (PinShownByDefault))
    FRotator LeftPalmTargetRotation = FRotator::ZeroRotator;
    /** Identity weapon sockets provide an additive weapon-space rotation delta;
     *  authored socket rotations remain absolute targets. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targets", meta = (PinShownByDefault))
    bool bRightPalmRotationIsAdditive = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targets", meta = (PinShownByDefault))
    bool bLeftPalmRotationIsAdditive = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targets", meta = (PinShownByDefault))
    bool bTargetsValid = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stance", meta = (PinShownByDefault, ClampMin = "0.0", ClampMax = "1.0"))
    float ADSAlpha = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stance", meta = (PinShownByDefault))
    bool bLongGun = false;

    /** TPV weapons are parented to hand_r, so solving that hand to its child
     *  would create a feedback loop. FPV enables both; TPV disables right. */
    UPROPERTY(EditAnywhere, Category = "Solve") bool bSolveRightHand = true;
    UPROPERTY(EditAnywhere, Category = "Solve") bool bSolveLeftHand = true;

    /** TPV base poses can leave fingers open; this adds a lightweight grip curl. */
    UPROPERTY(EditAnywhere, Category = "Grip") bool bApplyProceduralFingerGrip = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip", meta = (AlwaysAsPin, ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
    float FingerCurlDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip", meta = (AlwaysAsPin, ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
    float ThumbCurlDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solve", meta = (AlwaysAsPin, ClampMin = "0.0", Units = "cm"))
    float MaximumShoulderShift = 4.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solve", meta = (AlwaysAsPin, ClampMin = "1.0", Units = "cm"))
    float BendTargetDistance = 30.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solve", meta = (AlwaysAsPin, ClampMin = "1.0"))
    float MaximumStretchScale = 1.06f;

    /** Optional compact-reach correction. Zero preserves the authored pose bend. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solve", meta = (AlwaysAsPin, ClampMin = "0.0", ClampMax = "0.95"))
    float MinimumArmReachFraction = 0.0f;

    /** Optional anatomical pole blend. Zero uses the incoming animation bend plane. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solve", meta = (AlwaysAsPin, ClampMin = "0.0", ClampMax = "1.0"))
    float ElbowOutwardBias = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solve", meta = (AlwaysAsPin, ClampMin = "0.0", ClampMax = "1.0"))
    float ElbowDownwardBias = 0.0f;

    virtual void GatherDebugData(FNodeDebugData& DebugData) override;

protected:
    virtual void EvaluateSkeletalControl_AnyThread(
        FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

private:
    void SolveArm(
        const FProceduralArmIKChain& Chain,
        const FVector& PalmTarget,
        const FRotator& PalmTargetRotation,
        bool bPalmRotationIsAdditive,
        FComponentSpacePoseContext& Output,
        TArray<FBoneTransform>& OutBoneTransforms) const;
    void ApplyFingerGrip(
        const FProceduralArmIKChain& Arm,
        const TArray<TArray<FBoneReference>>& FingerChains,
        FComponentSpacePoseContext& Output,
        TArray<FBoneTransform>& OutBoneTransforms) const;

    TArray<TArray<FBoneReference>> RightFingerChains;
    TArray<TArray<FBoneReference>> LeftFingerChains;
};
