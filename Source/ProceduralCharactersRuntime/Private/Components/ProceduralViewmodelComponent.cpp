#include "Components/ProceduralViewmodelComponent.h"

#include "Engine/World.h"

UProceduralViewmodelComponent::UProceduralViewmodelComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    // A useful procedural default that remains fully editable in component
    // details. Assets may replace this inline curve with an external CurveFloat.
    if (FRichCurve* Curve = ReloadMotionCurve.GetRichCurve())
    {
        Curve->AddKey(0.0f, 0.0f);
        Curve->AddKey(0.18f, 1.0f);
        Curve->AddKey(0.78f, 1.0f);
        Curve->AddKey(1.0f, 0.0f);
        Curve->AutoSetTangents();
    }
}

void UProceduralViewmodelComponent::UpdateViewmodel(
    float DeltaSeconds, const FRotator& CameraRotation,
    const FVector& CameraLocation, const FVector& CameraForward,
    const FVector& LocalVelocity, const FVector& LocalAcceleration,
    float MaximumSpeed, float InAimAlpha, bool bGrounded,
    float VerticalSpeed, bool bReloading, float ReloadDuration)
{
    if (!bEnabled || DeltaSeconds <= 0.0f)
    {
        return;
    }

    ElapsedSeconds += DeltaSeconds;
    State.AimAlpha = FMath::Clamp(InAimAlpha, 0.0f, 1.0f);

    const float SpeedAlpha = FMath::Clamp(
        LocalVelocity.Size2D() / FMath::Max(MaximumSpeed, 1.0f), 0.0f, 1.0f);
    if (bGrounded && SpeedAlpha > KINDA_SMALL_NUMBER)
    {
        BobPhase += DeltaSeconds * MovementBobFrequency * (0.65f + SpeedAlpha);
    }

    const FVector BobOffset = bGrounded
        ? FVector(
            FMath::Cos(BobPhase * 2.0f) * MovementBobAmplitude.X,
            FMath::Sin(BobPhase) * MovementBobAmplitude.Y,
            -FMath::Abs(FMath::Cos(BobPhase)) * MovementBobAmplitude.Z) * SpeedAlpha
        : FVector::ZeroVector;

    const float AimMotionScale = FMath::Lerp(1.0f, 0.32f, State.AimAlpha);
    const float BreathingFadeRange = FMath::Max(
        BreathingMovementFadeEnd - BreathingMovementFadeStart, KINDA_SMALL_NUMBER);
    const float BreathingMovementAlpha = FMath::Clamp(
        (SpeedAlpha - BreathingMovementFadeStart) / BreathingFadeRange,
        0.0f, 1.0f);
    const float TargetBreathingAlpha = bGrounded
        ? 1.0f - FMath::SmoothStep(0.0f, 1.0f, BreathingMovementAlpha)
        : 0.0f;
    CurrentBreathingAlpha = FMath::FInterpTo(
        CurrentBreathingAlpha, TargetBreathingAlpha,
        DeltaSeconds, BreathingBlendSpeed);
    const float BreathAngle = ElapsedSeconds * BreathingFrequency * UE_TWO_PI;
    const FVector BreathOffset(
        FMath::Sin(BreathAngle * 0.5f) * BreathingLocationAmplitude.X,
        FMath::Cos(BreathAngle) * BreathingLocationAmplitude.Y,
        FMath::Sin(BreathAngle) * BreathingLocationAmplitude.Z);
    const FRotator BreathRotation(
        FMath::Sin(BreathAngle) * BreathingRotationAmplitude.X,
        FMath::Cos(BreathAngle * 0.5f) * BreathingRotationAmplitude.Y,
        FMath::Sin(BreathAngle * 0.75f) * BreathingRotationAmplitude.Z);

    const FVector NormalizedAcceleration(
        FMath::Clamp(LocalAcceleration.X / 2048.0f, -1.0f, 1.0f),
        FMath::Clamp(LocalAcceleration.Y / 2048.0f, -1.0f, 1.0f),
        FMath::Clamp(LocalAcceleration.Z / 2048.0f, -1.0f, 1.0f));
    const FVector TargetInertiaLocation(
        -NormalizedAcceleration.X * InertiaLocationScale.X,
        -NormalizedAcceleration.Y * InertiaLocationScale.Y,
        -NormalizedAcceleration.Z * InertiaLocationScale.Z);
    const FRotator TargetInertiaRotation(
        -NormalizedAcceleration.X * InertiaRotationScale.X,
        0.0f,
        NormalizedAcceleration.Y * InertiaRotationScale.Z);
    SmoothedInertiaLocation = FMath::VInterpTo(
        SmoothedInertiaLocation, TargetInertiaLocation, DeltaSeconds, MotionInterpSpeed);
    SmoothedInertiaRotation = FMath::RInterpTo(
        SmoothedInertiaRotation, TargetInertiaRotation, DeltaSeconds, MotionInterpSpeed);

    FRotator CameraDelta = FRotator::ZeroRotator;
    if (bCameraInitialized)
    {
        CameraDelta = CameraRotation - LastCameraRotation;
        CameraDelta.Normalize();
    }
    LastCameraRotation = CameraRotation;
    bCameraInitialized = true;

    const FVector TargetLookLagLocation(
        0.0f, -CameraDelta.Yaw * LookLagLocationScale.Y,
        -CameraDelta.Pitch * LookLagLocationScale.Z);
    const FRotator TargetLookLagRotation(
        -CameraDelta.Pitch * LookLagRotationScale.X,
        -CameraDelta.Yaw * LookLagRotationScale.Y,
        CameraDelta.Yaw * LookLagRotationScale.Z);
    SmoothedLookLagLocation = FMath::VInterpTo(
        SmoothedLookLagLocation, TargetLookLagLocation, DeltaSeconds, MotionInterpSpeed);
    SmoothedLookLagRotation = FMath::RInterpTo(
        SmoothedLookLagRotation, TargetLookLagRotation, DeltaSeconds, MotionInterpSpeed);

    WallProbeAccumulator += DeltaSeconds;
    const float ProbeInterval = 1.0f / FMath::Max(WallProbeRateHz, 1.0f);
    const float WallADSBlend = FMath::SmoothStep(0.0f, 1.0f, State.AimAlpha);
    const float EffectiveWallProbeDistance = FMath::Lerp(
        WallProbeDistance, ADSWallProbeDistance, WallADSBlend);
    if (WallProbeAccumulator >= ProbeInterval)
    {
        WallProbeAccumulator = FMath::Fmod(WallProbeAccumulator, ProbeInterval);
        TargetWallAvoidanceAlpha = 0.0f;
        if (UWorld* World = GetWorld(); World
            && EffectiveWallProbeDistance > KINDA_SMALL_NUMBER)
        {
            FHitResult Hit;
            FCollisionQueryParams Params(
                SCENE_QUERY_STAT(ProceduralViewmodelWallProbe), false, GetOwner());
            const FVector ProbeDirection = CameraForward.GetSafeNormal();
            if (!ProbeDirection.IsNearlyZero() && World->LineTraceSingleByChannel(
                    Hit, CameraLocation,
                    CameraLocation + ProbeDirection * EffectiveWallProbeDistance,
                    ECC_Visibility, Params))
            {
                TargetWallAvoidanceAlpha = 1.0f - FMath::Clamp(
                    Hit.Distance / EffectiveWallProbeDistance, 0.0f, 1.0f);
            }
        }
    }
    State.WallAvoidanceAlpha = FMath::FInterpTo(
        State.WallAvoidanceAlpha, TargetWallAvoidanceAlpha,
        DeltaSeconds, MotionInterpSpeed);

    CurrentImpulseLocation = FMath::VInterpTo(
        CurrentImpulseLocation, TargetImpulseLocation, DeltaSeconds, 28.0f);
    CurrentImpulseRotation = FMath::RInterpTo(
        CurrentImpulseRotation, TargetImpulseRotation, DeltaSeconds, 28.0f);
    TargetImpulseLocation = FMath::VInterpTo(
        TargetImpulseLocation, FVector::ZeroVector, DeltaSeconds, 10.0f);
    TargetImpulseRotation = FMath::RInterpTo(
        TargetImpulseRotation, FRotator::ZeroRotator, DeltaSeconds, 10.0f);

    const float AirAlpha = bGrounded
        ? 0.0f : FMath::Clamp(VerticalSpeed / 650.0f, -1.0f, 1.0f);
    const FVector AirOffset(0.0f, 0.0f, -AirAlpha * JumpFallOffset);
    const FRotator AirRotation(-AirAlpha * JumpFallPitch, 0.0f, 0.0f);
    const float EffectiveWallStrength = FMath::Lerp(
        1.0f, ADSWallAvoidanceStrength, WallADSBlend);
    const float EffectiveWallPushbackDistance = FMath::Lerp(
        WallPushbackDistance, ADSWallPushbackDistance, WallADSBlend);
    const FRotator EffectiveWallRotation = FMath::Lerp(
        WallAvoidanceRotation, ADSWallAvoidanceRotation, WallADSBlend);
    const float EffectiveWallAlpha = State.WallAvoidanceAlpha * EffectiveWallStrength;
    const FVector WallOffset(
        -EffectiveWallPushbackDistance * EffectiveWallAlpha, 0.0f, 0.0f);
    const FRotator WallRotation = EffectiveWallRotation * EffectiveWallAlpha;

    const FVector FinalLocation =
        (BobOffset + BreathOffset * CurrentBreathingAlpha
            + SmoothedInertiaLocation + SmoothedLookLagLocation)
            * AimMotionScale
        + AirOffset + CurrentImpulseLocation + WallOffset;
    const FRotator FinalRotation =
        (BreathRotation * CurrentBreathingAlpha
            + SmoothedInertiaRotation + SmoothedLookLagRotation)
            * AimMotionScale
        + AirRotation + CurrentImpulseRotation + WallRotation;

    if (bEnableProceduralReload && bReloading)
    {
        if (!bReloadWasActive)
        {
            ReloadElapsedSeconds = 0.0f;
        }
        ReloadElapsedSeconds += DeltaSeconds;
        const float Duration = FMath::Max(ReloadDuration, KINDA_SMALL_NUMBER);
        const float Progress = FMath::Clamp(ReloadElapsedSeconds / Duration, 0.0f, 1.0f);
        State.ReloadProgress = Progress;
        const FRichCurve* ReloadCurve = ReloadMotionCurve.GetRichCurveConst();
        if (ReloadCurve && ReloadCurve->GetNumKeys() > 0)
        {
            // Do not clamp the curve value: controlled overshoot and reverse
            // accents are legitimate procedural animation authoring tools.
            CurrentReloadAlpha = ReloadCurve->Eval(Progress);
        }
        else
        {
            const float Entry = FMath::SmoothStep(0.0f,
                FMath::Max(ReloadEntryFraction, 0.01f), Progress);
            const float Exit = 1.0f - FMath::SmoothStep(
                1.0f - FMath::Max(ReloadExitFraction, 0.01f), 1.0f, Progress);
            CurrentReloadAlpha = Entry * Exit;
        }
    }
    else
    {
        CurrentReloadAlpha = FMath::FInterpTo(CurrentReloadAlpha, 0.0f,
            DeltaSeconds, ReloadInterruptedReturnSpeed);
        if (FMath::IsNearlyZero(CurrentReloadAlpha, 0.001f))
        {
            CurrentReloadAlpha = 0.0f;
            ReloadElapsedSeconds = 0.0f;
            State.ReloadProgress = 0.0f;
        }
    }
    bReloadWasActive = bEnableProceduralReload && bReloading;

    State.ReloadAlpha = CurrentReloadAlpha;
    State.ReloadOffset = FTransform(
        ReloadRotationOffset * CurrentReloadAlpha,
        ReloadLocationOffset * CurrentReloadAlpha);
    State.ViewmodelOffset = State.ReloadOffset * FTransform(FinalRotation, FinalLocation);
    State.RecoilAlpha = FMath::Clamp(
        CurrentImpulseLocation.Size() / 8.0f
            + CurrentImpulseRotation.GetManhattanDistance(FRotator::ZeroRotator) / 24.0f,
        0.0f, 1.0f);
}

void UProceduralViewmodelComponent::AddRecoilImpulse(float Strength)
{
    if (!bEnabled || Strength <= 0.0f)
    {
        return;
    }

    const float ClampedStrength = FMath::Clamp(Strength, 0.0f, 12.0f);
    TargetImpulseLocation += FVector(
        -0.34f * ClampedStrength,
        FMath::FRandRange(-0.08f, 0.08f) * ClampedStrength,
        -0.05f * ClampedStrength);
    TargetImpulseRotation += FRotator(
        0.48f * ClampedStrength,
        FMath::FRandRange(-0.16f, 0.16f) * ClampedStrength,
        FMath::FRandRange(-0.22f, 0.22f) * ClampedStrength);
    TargetImpulseLocation = TargetImpulseLocation.GetClampedToMaxSize(12.0f);
    TargetImpulseRotation.Pitch = FMath::Clamp(TargetImpulseRotation.Pitch, -12.0f, 12.0f);
    TargetImpulseRotation.Yaw = FMath::Clamp(TargetImpulseRotation.Yaw, -6.0f, 6.0f);
    TargetImpulseRotation.Roll = FMath::Clamp(TargetImpulseRotation.Roll, -8.0f, 8.0f);
}

void UProceduralViewmodelComponent::AddLandingImpulse(float VerticalImpactSpeed)
{
    if (!bEnabled || VerticalImpactSpeed <= 0.0f)
    {
        return;
    }

    const float Strength = FMath::Clamp(
        (VerticalImpactSpeed - 250.0f) / 650.0f, 0.0f, 1.0f);
    TargetImpulseLocation += FVector(-1.0f, 0.0f, -6.0f) * Strength;
    TargetImpulseRotation += FRotator(5.5f * Strength, 0.0f, 1.5f * Strength);
}

void UProceduralViewmodelComponent::ResetViewmodel()
{
    State = FProceduralViewmodelState();
    LastCameraRotation = FRotator::ZeroRotator;
    SmoothedInertiaLocation = FVector::ZeroVector;
    SmoothedInertiaRotation = FRotator::ZeroRotator;
    SmoothedLookLagLocation = FVector::ZeroVector;
    SmoothedLookLagRotation = FRotator::ZeroRotator;
    CurrentImpulseLocation = FVector::ZeroVector;
    CurrentImpulseRotation = FRotator::ZeroRotator;
    TargetImpulseLocation = FVector::ZeroVector;
    TargetImpulseRotation = FRotator::ZeroRotator;
    ElapsedSeconds = 0.0f;
    BobPhase = 0.0f;
    CurrentBreathingAlpha = 1.0f;
    WallProbeAccumulator = 0.0f;
    TargetWallAvoidanceAlpha = 0.0f;
    ReloadElapsedSeconds = 0.0f;
    CurrentReloadAlpha = 0.0f;
    bCameraInitialized = false;
    bReloadWasActive = false;
}
