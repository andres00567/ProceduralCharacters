#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/ProceduralCharacterTypes.h"
#include "ProceduralCharacterComponent.generated.h"

class UProceduralCharacterProfile;
class UProceduralAnimationBudgetSubsystem;

UCLASS(ClassGroup = Animation, meta = (BlueprintSpawnableComponent))
class PROCEDURALCHARACTERSRUNTIME_API UProceduralCharacterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UProceduralCharacterComponent();

    UFUNCTION(BlueprintCallable, Category = "Procedural Animation")
    void SetProfile(UProceduralCharacterProfile* InProfile);

    UFUNCTION(BlueprintCallable, Category = "Procedural Animation|Intent")
    void SetLookTarget(const FVector& WorldTarget, float Priority = 1.0f);

    UFUNCTION(BlueprintCallable, Category = "Procedural Animation|Intent")
    void ClearLookTarget();

    UFUNCTION(BlueprintCallable, Category = "Procedural Animation|Intent")
    void SetAimDirection(const FVector& WorldDirection);

    UFUNCTION(BlueprintCallable, Category = "Procedural Animation|Intent")
    void SetAttackTarget(const FVector& WorldTarget);

    UFUNCTION(BlueprintCallable, Category = "Procedural Animation|Intent")
    void ClearAttackTarget();

    UFUNCTION(BlueprintCallable, Category = "Procedural Animation|Impulses")
    void AddProceduralImpulse(const FProceduralImpulse& Impulse);

    UFUNCTION(BlueprintCallable, Category = "Procedural Animation|Budget")
    void RequestPriorityBoost(float MinimumSignificance, float DurationSeconds, FName Reason);

    /** Game-thread-only reference. Animation workers must use GetThreadSafeSnapshot. */
    const FProceduralCharacterState& GetGameThreadState() const;

    /** Copies the published buffer under a short read lock; contains no UObject references. */
    UFUNCTION(BlueprintPure, Category = "Procedural Animation", meta = (BlueprintThreadSafe))
    FProceduralCharacterState GetThreadSafeSnapshot() const;

    UFUNCTION(BlueprintCallable, Category = "Procedural Animation|Budget")
    void SetLocallyControlled(bool bInLocallyControlled);

    UFUNCTION(BlueprintCallable, Category = "Procedural Animation|Budget")
    void SetSpecialEnemy(bool bInSpecialEnemy);

    UFUNCTION(BlueprintCallable, Category = "Procedural Animation|Budget")
    void SetBoss(bool bInBoss);

    UFUNCTION(BlueprintCallable, Category = "Procedural Animation|Budget")
    void SetGameplayThreat(float NormalizedThreat);

    UFUNCTION(BlueprintPure, Category = "Procedural Animation|Budget")
    float GetSignificance() const { return Significance; }

    UFUNCTION(BlueprintPure, Category = "Procedural Animation|Budget")
    EProceduralAnimTier GetAssignedTier() const { return GameThreadState.LOD.Tier; }

    UProceduralCharacterProfile* GetProfile() const { return Profile; }
    bool IsLocallyControlledForBudget() const { return bLocallyControlled; }
    bool IsSpecialEnemyForBudget() const { return bSpecialEnemy; }
    bool IsBossForBudget() const { return bBoss; }
    float GetGameplayThreat() const { return GameplayThreat; }
    const FProceduralPriorityBoost& GetPriorityBoost() const { return PriorityBoost; }
    int32 GetStableRegistrationId() const { return StableRegistrationId; }

    static bool IsPriorityBoostActive(const FProceduralPriorityBoost& Boost, double NowSeconds);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    friend class UProceduralAnimationBudgetSubsystem;

    void GatherScheduledState(float DeltaTime);
    void ApplyBudgetResult(const FProceduralAnimLODState& LOD, float InSignificance);
    void UpdateImpulseSnapshot(float NowSeconds);
    void PublishState();
    void SetStableRegistrationId(int32 InId) { StableRegistrationId = InId; }

    UPROPERTY(EditAnywhere, Category = "Procedural Animation")
    TObjectPtr<UProceduralCharacterProfile> Profile;

    FProceduralCharacterState GameThreadState;
    FProceduralCharacterState PublishedStates[2];
    mutable FRWLock SnapshotLock;
    int32 PublishedStateIndex = 0;

    FVector ExplicitAimDirection = FVector::ForwardVector;
    FVector PreviousVelocity = FVector::ZeroVector;
    FRotator PreviousRotation = FRotator::ZeroRotator;
    float LookPriority = 0.0f;
    float Significance = 0.0f;
    float GameplayThreat = 0.0f;
    bool bHasExplicitAim = false;
    bool bLocallyControlled = false;
    bool bSpecialEnemy = false;
    bool bBoss = false;
    int32 StableRegistrationId = INDEX_NONE;

    static constexpr int32 AbsoluteMaxImpulses = 16;
    TArray<FProceduralImpulse, TInlineAllocator<AbsoluteMaxImpulses>> ActiveImpulses;
    FProceduralPriorityBoost PriorityBoost;
};
