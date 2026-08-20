#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "ProceduralViewmodelComponent.generated.h"

USTRUCT(BlueprintType)
struct PROCEDURALCHARACTERSRUNTIME_API FProceduralViewmodelState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Procedural Viewmodel")
    FTransform ViewmodelOffset = FTransform::Identity;

    UPROPERTY(BlueprintReadOnly, Category="Procedural Viewmodel")
    float AimAlpha = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Procedural Viewmodel")
    float WallAvoidanceAlpha = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Procedural Viewmodel")
    float RecoilAlpha = 0.0f;

    /** Reload-only root motion, shared by the FPV weapon and arms hierarchy. */
    UPROPERTY(BlueprintReadOnly, Category="Procedural Viewmodel")
    FTransform ReloadOffset = FTransform::Identity;

    UPROPERTY(BlueprintReadOnly, Category="Procedural Viewmodel")
    float ReloadAlpha = 0.0f;

    /** Normalized procedural reload clock, independent of any guide clip. */
    UPROPERTY(BlueprintReadOnly, Category="Procedural Viewmodel")
    float ReloadProgress = 0.0f;
};

/** Local-player procedural layer for the rendered first-person weapon/arms hierarchy. */
UCLASS(ClassGroup=Animation, meta=(BlueprintSpawnableComponent))
class PROCEDURALCHARACTERSRUNTIME_API UProceduralViewmodelComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UProceduralViewmodelComponent();

    void UpdateViewmodel(
        float DeltaSeconds, const FRotator& CameraRotation,
        const FVector& CameraLocation, const FVector& CameraForward,
        const FVector& LocalVelocity, const FVector& LocalAcceleration,
        float MaximumSpeed, float InAimAlpha, bool bGrounded,
        float VerticalSpeed, bool bReloading, float ReloadDuration);

    UFUNCTION(BlueprintCallable, Category="Procedural Viewmodel")
    void AddRecoilImpulse(float Strength);

    UFUNCTION(BlueprintCallable, Category="Procedural Viewmodel")
    void AddLandingImpulse(float VerticalImpactSpeed);

    UFUNCTION(BlueprintPure, Category="Procedural Viewmodel")
    const FProceduralViewmodelState& GetViewmodelState() const { return State; }

    UFUNCTION(BlueprintCallable, Category="Procedural Viewmodel")
    void ResetViewmodel();

    bool IsFirstPersonHandIKEnabled() const { return bEnableFirstPersonHandIK; }
    bool IsThirdPersonHandIKEnabled() const { return bEnableThirdPersonHandIK; }
    bool ShouldUseWeaponGripSocketRotations() const { return bUseWeaponGripSocketRotations; }
    bool ShouldPreservePoseForIdentityGripRotation() const { return bPreservePoseForIdentityGripRotation; }
    bool ShouldDrawHandIKDebug() const { return bDrawHandIKDebug; }
    FVector GetRightGripLocationOffset() const { return RightGripLocationOffset; }
    FRotator GetRightGripRotationOffset() const { return RightGripRotationOffset; }
    FVector GetLeftGripLocationOffset() const { return LeftGripLocationOffset; }
    FRotator GetLeftGripRotationOffset() const { return LeftGripRotationOffset; }
    float GetHandIKMaximumShoulderShift(bool bFirstPerson) const
    {
        return bFirstPerson ? FirstPersonMaximumShoulderShift : ThirdPersonMaximumShoulderShift;
    }
    float GetHandIKBendTargetDistance() const { return HandIKBendTargetDistance; }
    float GetHandIKMaximumStretchScale() const { return HandIKMaximumStretchScale; }
    float GetHandIKMinimumArmReachFraction() const { return HandIKMinimumArmReachFraction; }
    float GetHandIKElbowOutwardBias() const { return HandIKElbowOutwardBias; }
    float GetHandIKElbowDownwardBias() const { return HandIKElbowDownwardBias; }
    bool ShouldAlignThirdPersonWeaponToRightPalm() const { return bAlignThirdPersonWeaponToRightPalm; }
    FVector GetThirdPersonRightGripLocationOffset() const { return ThirdPersonRightGripLocationOffset; }
    FRotator GetThirdPersonRightGripRotationOffset() const { return ThirdPersonRightGripRotationOffset; }
    FVector GetThirdPersonLeftGripLocationOffset() const { return ThirdPersonLeftGripLocationOffset; }
    FRotator GetThirdPersonLeftGripRotationOffset() const { return ThirdPersonLeftGripRotationOffset; }
    float GetThirdPersonFingerCurlDegrees() const { return ThirdPersonFingerCurlDegrees; }
    float GetThirdPersonThumbCurlDegrees() const { return ThirdPersonThumbCurlDegrees; }
    FRotator GetWallAvoidanceRotation() const
    {
        const float ADSBlend = FMath::SmoothStep(0.0f, 1.0f, State.AimAlpha);
        return FMath::Lerp(WallAvoidanceRotation, ADSWallAvoidanceRotation, ADSBlend)
            * FMath::Lerp(1.0f, ADSWallAvoidanceStrength, ADSBlend);
    }
    bool ShouldUseReloadAnimationGuide() const { return bUseReloadAnimationGuide; }
    float GetReloadAnimationGuideStrength() const { return ReloadAnimationGuideStrength; }

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Movement", meta=(ClampMin="0.0"))
    float MovementBobFrequency = 4.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Movement", meta=(Units="cm"))
    FVector MovementBobAmplitude = FVector(0.45f, 1.35f, 1.1f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Inertia", meta=(Units="cm"))
    FVector InertiaLocationScale = FVector(2.2f, 2.8f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Inertia", meta=(Units="deg"))
    FVector InertiaRotationScale = FVector(1.5f, 1.0f, 3.5f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Look Lag", meta=(Units="cm"))
    FVector LookLagLocationScale = FVector(0.0f, 0.22f, 0.28f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Look Lag", meta=(Units="deg"))
    FVector LookLagRotationScale = FVector(0.75f, 0.55f, 0.18f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Smoothing", meta=(ClampMin="0.1"))
    float MotionInterpSpeed = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Breathing", meta=(ClampMin="0.0"))
    float BreathingFrequency = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Breathing", meta=(Units="cm"))
    FVector BreathingLocationAmplitude = FVector(0.04f, 0.04f, 0.09f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Breathing", meta=(Units="deg"))
    FVector BreathingRotationAmplitude = FVector(0.07f, 0.04f, 0.09f);

    /** Breathing begins fading once locomotion reaches this fraction of max speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Breathing", meta=(ClampMin="0.0", ClampMax="1.0"))
    float BreathingMovementFadeStart = 0.05f;

    /** Breathing is fully suppressed at and above this fraction of max speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Breathing", meta=(ClampMin="0.0", ClampMax="1.0"))
    float BreathingMovementFadeEnd = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Breathing", meta=(ClampMin="0.1"))
    float BreathingBlendSpeed = 7.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Airborne", meta=(Units="cm"))
    float JumpFallOffset = 3.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Airborne", meta=(Units="deg"))
    float JumpFallPitch = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Wall Avoidance", meta=(ClampMin="0.0", Units="cm"))
    float WallProbeDistance = 95.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Wall Avoidance", meta=(ClampMin="1.0", Units="Hz"))
    float WallProbeRateHz = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Wall Avoidance", meta=(ClampMin="0.0", Units="cm"))
    float WallPushbackDistance = 22.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Wall Avoidance", meta=(Units="deg"))
    FRotator WallAvoidanceRotation = FRotator(35.0f, 10.0f, -5.0f);

    /** ADS gets a shorter probe so the weapon does not begin retracting while
     * the sight still has comfortable clearance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Wall Avoidance|ADS",
        meta=(ClampMin="0.0", Units="cm"))
    float ADSWallProbeDistance = 65.0f;

    /** Maximum camera-local rearward travel while fully aimed. Keep this well
     * below the hip value to prevent the arms/receiver entering the camera. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Wall Avoidance|ADS",
        meta=(ClampMin="0.0", Units="cm"))
    float ADSWallPushbackDistance = 6.0f;

    /** ADS can favor rotating the muzzle away from the wall instead of pulling
     * the entire viewmodel back through the camera. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Wall Avoidance|ADS",
        meta=(Units="deg"))
    FRotator ADSWallAvoidanceRotation = FRotator(42.0f, 8.0f, -4.0f);

    /** Overall ADS wall-response multiplier. Zero disables ADS wall motion;
     * one applies the full ADS distance and rotation values above. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Wall Avoidance|ADS",
        meta=(ClampMin="0.0", ClampMax="1.0"))
    float ADSWallAvoidanceStrength = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK")
    bool bEnableFirstPersonHandIK = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK")
    bool bEnableThirdPersonHandIK = true;

    /** When enabled, non-zero FP_Grip_R/L socket rotations orient the hands. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Grip Rotation")
    bool bUseWeaponGripSocketRotations = true;

    /** Identity socket rotations mean position-only and preserve the base pose. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Grip Rotation")
    bool bPreservePoseForIdentityGripRotation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Right Hand", meta=(Units="cm"))
    FVector RightGripLocationOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Right Hand", meta=(Units="deg"))
    FRotator RightGripRotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Left Hand", meta=(Units="cm"))
    FVector LeftGripLocationOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Left Hand", meta=(Units="deg"))
    FRotator LeftGripRotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Solve", meta=(ClampMin="0.0", Units="cm"))
    float FirstPersonMaximumShoulderShift = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Solve", meta=(ClampMin="0.0", Units="cm"))
    float ThirdPersonMaximumShoulderShift = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Solve", meta=(ClampMin="1.0", Units="cm"))
    float HandIKBendTargetDistance = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Solve", meta=(ClampMin="1.0"))
    float HandIKMaximumStretchScale = 1.06f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Solve",
        meta=(ClampMin="0.0", ClampMax="0.95"))
    float HandIKMinimumArmReachFraction = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Solve",
        meta=(ClampMin="0.0", ClampMax="1.0"))
    float HandIKElbowOutwardBias = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Solve",
        meta=(ClampMin="0.0", ClampMax="1.0"))
    float HandIKElbowDownwardBias = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Debug")
    bool bDrawHandIKDebug = false;

    /** Aligns TP_Grip_R/FP_Grip_R to the right palm without solving the right arm
     *  against a weapon that is already parented to that arm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Third Person")
    // TPV preserves the authored weapon/right-arm contract. The procedural
    // solver owns only the left support arm unless a project explicitly opts in.
    bool bAlignThirdPersonWeaponToRightPalm = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Third Person|Right Hand", meta=(Units="cm"))
    FVector ThirdPersonRightGripLocationOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Third Person|Right Hand", meta=(Units="deg"))
    FRotator ThirdPersonRightGripRotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Third Person|Left Hand", meta=(Units="cm"))
    FVector ThirdPersonLeftGripLocationOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Third Person|Left Hand", meta=(Units="deg"))
    FRotator ThirdPersonLeftGripRotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Third Person|Fingers", meta=(ClampMin="0.0", ClampMax="90.0", Units="deg"))
    float ThirdPersonFingerCurlDegrees = 52.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Hand IK|Third Person|Fingers", meta=(ClampMin="0.0", ClampMax="90.0", Units="deg"))
    float ThirdPersonThumbCurlDegrees = 28.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Reload")
    bool bEnableProceduralReload = true;

    /** Lets an optional per-weapon reload animation guide the procedural weapon
     *  trajectory. The animation supplies intent; the procedural layer keeps
     *  ownership of the rendered weapon transform and timing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Reload|Animation Guide")
    bool bUseReloadAnimationGuide = true;

    /** Global blend strength for animation-guided reload motion. This is
     *  multiplied by the per-weapon guide strength and the reload envelope. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Reload|Animation Guide",
        meta=(ClampMin="0.0", ClampMax="1.0"))
    float ReloadAnimationGuideStrength = 1.0f;

    /** Primary procedural reload shape. X is normalized reload time (0-1), Y
     *  scales Reload Location/Rotation Offset. An external CurveFloat can be
     *  assigned from the same picker. Empty curves use Entry/Exit Fraction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Reload")
    FRuntimeFloatCurve ReloadMotionCurve;

    /** Peak camera-local movement of the weapon and arms during reload. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Reload", meta=(Units="cm"))
    FVector ReloadLocationOffset = FVector(-7.0f, 5.0f, -13.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Reload", meta=(Units="deg"))
    FRotator ReloadRotationOffset = FRotator(-12.0f, 18.0f, -22.0f);

    /** Fraction of reload duration used to move into the procedural reload pose. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Reload", meta=(ClampMin="0.01", ClampMax="0.45"))
    float ReloadEntryFraction = 0.18f;

    /** Fraction of reload duration used to return from the procedural reload pose. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Reload", meta=(ClampMin="0.01", ClampMax="0.45"))
    float ReloadExitFraction = 0.22f;

    /** Recovery speed if reload is interrupted before its normal exit phase. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Viewmodel|Reload", meta=(ClampMin="0.1"))
    float ReloadInterruptedReturnSpeed = 12.0f;

private:
    FProceduralViewmodelState State;
    FRotator LastCameraRotation = FRotator::ZeroRotator;
    FVector SmoothedInertiaLocation = FVector::ZeroVector;
    FRotator SmoothedInertiaRotation = FRotator::ZeroRotator;
    FVector SmoothedLookLagLocation = FVector::ZeroVector;
    FRotator SmoothedLookLagRotation = FRotator::ZeroRotator;
    FVector CurrentImpulseLocation = FVector::ZeroVector;
    FRotator CurrentImpulseRotation = FRotator::ZeroRotator;
    FVector TargetImpulseLocation = FVector::ZeroVector;
    FRotator TargetImpulseRotation = FRotator::ZeroRotator;
    float ElapsedSeconds = 0.0f;
    float BobPhase = 0.0f;
    float CurrentBreathingAlpha = 1.0f;
    float WallProbeAccumulator = 0.0f;
    float TargetWallAvoidanceAlpha = 0.0f;
    float ReloadElapsedSeconds = 0.0f;
    float CurrentReloadAlpha = 0.0f;
    bool bCameraInitialized = false;
    bool bReloadWasActive = false;
};
