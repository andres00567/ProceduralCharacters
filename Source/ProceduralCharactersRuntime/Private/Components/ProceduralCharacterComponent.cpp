#include "Components/ProceduralCharacterComponent.h"

#include "Data/ProceduralCharacterProfile.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Subsystems/ProceduralAnimationBudgetSubsystem.h"

UProceduralCharacterComponent::UProceduralCharacterComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UProceduralCharacterComponent::BeginPlay()
{
    Super::BeginPlay();
    PreviousVelocity = GetOwner() ? GetOwner()->GetVelocity() : FVector::ZeroVector;
    PreviousRotation = GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        bLocallyControlled = Pawn->IsLocallyControlled();
    }
    if (const AActor* Owner = GetOwner())
    {
        bSpecialEnemy = Owner->ActorHasTag(TEXT("Enemy.Special"));
        bBoss = Owner->ActorHasTag(TEXT("Enemy.Boss")) || Owner->ActorHasTag(TEXT("Enemy.Tank"));
    }
    if (UWorld* World = GetWorld())
    {
        if (UProceduralAnimationBudgetSubsystem* Budget = World->GetSubsystem<UProceduralAnimationBudgetSubsystem>())
        {
            Budget->RegisterCharacter(this);
        }
    }
}

void UProceduralCharacterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UProceduralAnimationBudgetSubsystem* Budget = World->GetSubsystem<UProceduralAnimationBudgetSubsystem>())
        {
            Budget->UnregisterCharacter(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void UProceduralCharacterComponent::SetProfile(UProceduralCharacterProfile* InProfile)
{
    Profile = InProfile;
}

void UProceduralCharacterComponent::SetLookTarget(const FVector& WorldTarget, float Priority)
{
    if (!GameThreadState.bHasLookTarget || Priority >= LookPriority)
    {
        GameThreadState.LookTargetWorld = WorldTarget;
        GameThreadState.bHasLookTarget = true;
        LookPriority = Priority;
        PublishState();
    }
}

void UProceduralCharacterComponent::ClearLookTarget()
{
    GameThreadState.bHasLookTarget = false;
    LookPriority = 0.0f;
    PublishState();
}

void UProceduralCharacterComponent::SetAimDirection(const FVector& WorldDirection)
{
    ExplicitAimDirection = WorldDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
    bHasExplicitAim = true;
    GameThreadState.AimDirection = ExplicitAimDirection;
    PublishState();
}

void UProceduralCharacterComponent::SetAttackTarget(const FVector& WorldTarget)
{
    GameThreadState.AttackTargetWorld = WorldTarget;
    GameThreadState.bHasAttackTarget = true;
    PublishState();
}

void UProceduralCharacterComponent::ClearAttackTarget()
{
    GameThreadState.bHasAttackTarget = false;
    PublishState();
}

void UProceduralCharacterComponent::AddProceduralImpulse(const FProceduralImpulse& InImpulse)
{
    FProceduralImpulse Impulse = InImpulse;
    Impulse.StartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const int32 Capacity = FMath::Clamp(Profile ? Profile->MaxActiveImpulses : 4, 1, AbsoluteMaxImpulses);
    if (ActiveImpulses.Num() < Capacity)
    {
        ActiveImpulses.Add(Impulse);
    }
    else
    {
        int32 ReplacementIndex = 0;
        for (int32 Index = 1; Index < ActiveImpulses.Num(); ++Index)
        {
            const FProceduralImpulse& Candidate = ActiveImpulses[Index];
            const FProceduralImpulse& Current = ActiveImpulses[ReplacementIndex];
            if (Candidate.Strength < Current.Strength ||
                (FMath::IsNearlyEqual(Candidate.Strength, Current.Strength) && Candidate.StartTimeSeconds < Current.StartTimeSeconds))
            {
                ReplacementIndex = Index;
            }
        }
        ActiveImpulses[ReplacementIndex] = Impulse;
    }
    UpdateImpulseSnapshot(Impulse.StartTimeSeconds);
    PublishState();
}

void UProceduralCharacterComponent::RequestPriorityBoost(float MinimumSignificance, float DurationSeconds, FName Reason)
{
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const double NewExpiration = Now + FMath::Max(0.0f, DurationSeconds);
    if (!IsPriorityBoostActive(PriorityBoost, Now) ||
        MinimumSignificance >= PriorityBoost.MinimumSignificance ||
        NewExpiration > PriorityBoost.ExpirationTimeSeconds)
    {
        PriorityBoost.MinimumSignificance = FMath::Clamp(MinimumSignificance, 0.0f, 1.0f);
        PriorityBoost.ExpirationTimeSeconds = NewExpiration;
        PriorityBoost.Reason = Reason;
    }
}

const FProceduralCharacterState& UProceduralCharacterComponent::GetGameThreadState() const
{
    check(IsInGameThread());
    return GameThreadState;
}

FProceduralCharacterState UProceduralCharacterComponent::GetThreadSafeSnapshot() const
{
    FReadScopeLock Lock(SnapshotLock);
    return PublishedStates[PublishedStateIndex];
}

void UProceduralCharacterComponent::SetLocallyControlled(bool bInLocallyControlled)
{
    bLocallyControlled = bInLocallyControlled;
}

void UProceduralCharacterComponent::SetSpecialEnemy(bool bInSpecialEnemy)
{
    bSpecialEnemy = bInSpecialEnemy;
}

void UProceduralCharacterComponent::SetBoss(bool bInBoss)
{
    bBoss = bInBoss;
}

void UProceduralCharacterComponent::SetGameplayThreat(float NormalizedThreat)
{
    GameplayThreat = FMath::Clamp(NormalizedThreat, 0.0f, 1.0f);
}

bool UProceduralCharacterComponent::IsPriorityBoostActive(const FProceduralPriorityBoost& Boost, double NowSeconds)
{
    return Boost.MinimumSignificance > 0.0f && Boost.ExpirationTimeSeconds > NowSeconds;
}

void UProceduralCharacterComponent::GatherScheduledState(float DeltaTime)
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    const float SafeDelta = FMath::Max(DeltaTime, SMALL_NUMBER);
    GameThreadState.WorldVelocity = Owner->GetVelocity();
    GameThreadState.LocalVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(GameThreadState.WorldVelocity);
    GameThreadState.WorldAcceleration = (GameThreadState.WorldVelocity - PreviousVelocity) / SafeDelta;
    GameThreadState.LocalAcceleration = Owner->GetActorTransform().InverseTransformVectorNoScale(GameThreadState.WorldAcceleration);
    const FRotator Rotation = Owner->GetActorRotation();
    const FRotator DeltaRotation = (Rotation - PreviousRotation).GetNormalized();
    GameThreadState.AngularVelocity = FVector(DeltaRotation.Roll, DeltaRotation.Pitch, DeltaRotation.Yaw) / SafeDelta;
    GameThreadState.GroundSpeed = FVector(GameThreadState.WorldVelocity.X, GameThreadState.WorldVelocity.Y, 0.0f).Size();
    GameThreadState.VerticalSpeed = GameThreadState.WorldVelocity.Z;
    GameThreadState.DesiredMoveDirection = GameThreadState.WorldVelocity.GetSafeNormal(
        UE_SMALL_NUMBER, Owner->GetActorForwardVector());
    if (GameThreadState.GroundSpeed > KINDA_SMALL_NUMBER)
    {
        GameThreadState.MovementDirectionDegrees = FMath::RadiansToDegrees(
            FMath::Atan2(GameThreadState.LocalVelocity.Y, GameThreadState.LocalVelocity.X));
    }
    GameThreadState.AimDirection = bHasExplicitAim ? ExplicitAimDirection : Owner->GetActorForwardVector();

    if (const ACharacter* Character = Cast<ACharacter>(Owner))
    {
        if (const UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            GameThreadState.WorldAcceleration = Movement->GetCurrentAcceleration();
            GameThreadState.LocalAcceleration = Owner->GetActorTransform().InverseTransformVectorNoScale(GameThreadState.WorldAcceleration);
            GameThreadState.bGrounded = Movement->IsMovingOnGround();
            if (Movement->CurrentFloor.IsWalkableFloor())
            {
                GameThreadState.FloorNormal = Movement->CurrentFloor.HitResult.ImpactNormal;
                GameThreadState.FloorDistance = Movement->CurrentFloor.FloorDist;
                GameThreadState.SlopeAngleDegrees = FMath::RadiansToDegrees(
                    FMath::Acos(FMath::Clamp(GameThreadState.FloorNormal.Z, -1.0f, 1.0f)));
            }
        }
    }
    else if (const UMovementComponent* Movement = Owner->FindComponentByClass<UMovementComponent>())
    {
        GameThreadState.WorldVelocity = Movement->Velocity;
    }

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    UpdateImpulseSnapshot(Now);

    PreviousVelocity = GameThreadState.WorldVelocity;
    PreviousRotation = Rotation;
    PublishState();
}

void UProceduralCharacterComponent::UpdateImpulseSnapshot(float NowSeconds)
{
    GameThreadState.ProceduralLinearImpulse = FVector::ZeroVector;
    GameThreadState.ProceduralAngularImpulse = FVector::ZeroVector;
    GameThreadState.ProceduralImpulseAlpha = 0.0f;
    GameThreadState.ActiveImpulseFeatureMask = 0;
    ActiveImpulses.RemoveAllSwap([NowSeconds](const FProceduralImpulse& Impulse)
    {
        return NowSeconds >= Impulse.StartTimeSeconds + Impulse.Duration;
    }, EAllowShrinking::No);
    for (const FProceduralImpulse& Impulse : ActiveImpulses)
    {
        const float Duration = FMath::Max(Impulse.Duration, SMALL_NUMBER);
        const float Envelope = FMath::Clamp(
            1.0f - (NowSeconds - Impulse.StartTimeSeconds) / Duration, 0.0f, 1.0f);
        const float Weight = FMath::Max(0.0f, Impulse.Strength) * Envelope;
        GameThreadState.ProceduralLinearImpulse += Impulse.LinearImpulse * Weight;
        GameThreadState.ProceduralAngularImpulse += Impulse.AngularImpulse * Weight;
        GameThreadState.ProceduralImpulseAlpha = FMath::Max(
            GameThreadState.ProceduralImpulseAlpha, Weight);
        EProceduralFeature RequiredFeature = EProceduralFeature::HitReactions;
        if (Impulse.Type == EProceduralImpulseType::Landing ||
            Impulse.Type == EProceduralImpulseType::JumpLaunch ||
            Impulse.Type == EProceduralImpulseType::SuddenStop)
        {
            RequiredFeature = EProceduralFeature::LandingResponse;
        }
        else if (Impulse.Type == EProceduralImpulseType::WeaponRecoil)
        {
            RequiredFeature = EProceduralFeature::WeaponHandling;
        }
        GameThreadState.ActiveImpulseFeatureMask |= static_cast<int32>(RequiredFeature);
    }
}

void UProceduralCharacterComponent::ApplyBudgetResult(const FProceduralAnimLODState& LOD, float InSignificance)
{
    GameThreadState.LOD = LOD;
    Significance = InSignificance;
    PublishState();
}

void UProceduralCharacterComponent::PublishState()
{
    FWriteScopeLock Lock(SnapshotLock);
    const int32 WriteIndex = 1 - PublishedStateIndex;
    PublishedStates[WriteIndex] = GameThreadState;
    PublishedStateIndex = WriteIndex;
}
