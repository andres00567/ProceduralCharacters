#pragma once

#include "CoreMinimal.h"
#include "Data/ProceduralCharacterTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ProceduralAnimationBudgetSubsystem.generated.h"

class UProceduralCharacterComponent;

UCLASS()
class PROCEDURALCHARACTERSRUNTIME_API UProceduralAnimationBudgetSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    void RegisterCharacter(UProceduralCharacterComponent* Character);
    void UnregisterCharacter(UProceduralCharacterComponent* Character);

    UFUNCTION(BlueprintPure, Category = "Procedural Animation|Budget")
    int32 GetRegisteredCharacterCount() const;

    UFUNCTION(BlueprintPure, Category = "Procedural Animation|Budget")
    int32 GetTierPopulation(EProceduralAnimTier Tier) const;

    /** Reserves one query slot. Callers perform the query only when this returns true. */
    bool TryConsumeQueryBudget(const UProceduralCharacterComponent* Character, EProceduralQueryType QueryType);

    int32 GetQueriesConsumedThisFrame(EProceduralQueryType QueryType) const;
    int32 GetQueriesRejectedThisFrame() const { return QueriesRejectedThisFrame; }

    static float CalculateSignificance(const FProceduralSignificanceInput& Input);
    static void AssignTiersByScore(const TArray<float>& Scores, const FProceduralTierCaps& Caps,
        TArray<EProceduralAnimTier>& OutTiers);

private:
    struct FRegisteredCharacter
    {
        TWeakObjectPtr<UProceduralCharacterComponent> Component;
        int32 StableId = INDEX_NONE;
        double LastStateUpdateTime = 0.0;
        double NextStateUpdateTime = 0.0;
        float Significance = 0.0f;
    };

    struct FViewpoint
    {
        FVector Location = FVector::ZeroVector;
        FVector Forward = FVector::ForwardVector;
    };

    void RecalculateSignificanceAndTiers(double NowSeconds);
    void GatherViewpoints(TArray<FViewpoint, TInlineAllocator<4>>& OutViewpoints) const;
    FProceduralSignificanceInput BuildSignificanceInput(
        const UProceduralCharacterComponent& Character,
        TConstArrayView<FViewpoint> Viewpoints, double NowSeconds) const;
    FProceduralAnimLODState BuildLODState(
        const UProceduralCharacterComponent& Character, EProceduralAnimTier Tier,
        bool bRecentlyRendered) const;
    FProceduralTierCaps GetEffectiveCaps() const;
    void DrawDebugState() const;
    void CompactInvalidRegistrations();

    TArray<FRegisteredCharacter> RegisteredCharacters;
    int32 TierPopulations[5] = { 0, 0, 0, 0, 0 };
    int32 QueriesConsumed[4] = { 0, 0, 0, 0 };
    int32 QueriesRejectedThisFrame = 0;
    int32 NextStableId = 0;
    double LastSignificanceUpdateTime = -BIG_NUMBER;
};
