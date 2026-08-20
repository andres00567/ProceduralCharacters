#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Data/ProceduralCharacterTypes.h"
#include "ProceduralAnimationBudgetSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Procedural Animation Budget"))
class PROCEDURALCHARACTERSRUNTIME_API UProceduralAnimationBudgetSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

    UPROPERTY(EditAnywhere, Config, Category = "Budget")
    FProceduralTierCaps TierCaps;

    UPROPERTY(EditAnywhere, Config, Category = "Budget")
    FProceduralQueryBudget QueryBudget;

    UPROPERTY(EditAnywhere, Config, Category = "Budget", meta = (ClampMin = "0.02", Units = "s"))
    float SignificanceUpdateInterval = 0.1f;

    UPROPERTY(EditAnywhere, Config, Category = "Budget", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DemotionHysteresisBonus = 0.06f;

    UPROPERTY(EditAnywhere, Config, Category = "Budget", meta = (ClampMin = "100.0", Units = "cm"))
    float MaximumSignificanceDistance = 15000.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Budget", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RuntimeBudgetScale = 1.0f;
};
