#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/ProceduralCharacterTypes.h"
#include "ProceduralCharacterProfile.generated.h"

UENUM(BlueprintType)
enum class EProceduralCharacterArchetype : uint8
{
    FirstPersonArms, Survivor, Cultist, Zombie, SpecialZombie, Creature, Boss
};

USTRUCT(BlueprintType)
struct PROCEDURALCHARACTERSRUNTIME_API FProceduralLimbDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limb") FName LimbID;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limb") FName RootBone;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limb") FName MidBone;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limb") FName EndBone;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limb") FName PoleBone;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limb", meta = (ClampMin = "0.0")) float MaximumExtension = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limb", meta = (ClampMin = "0.0")) float SolverStiffness = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limb") bool bSupportsEnvironmentContact = false;
};

USTRUCT(BlueprintType)
struct PROCEDURALCHARACTERSRUNTIME_API FProceduralTierSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier") EProceduralAnimTier Tier = EProceduralAnimTier::Dormant;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0.0")) float PoseUpdateRateHz = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0.0")) float ProceduralUpdateRateHz = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0.0")) float EnvironmentQueryRateHz = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (Bitmask, BitmaskEnum = "/Script/ProceduralCharactersRuntime.EProceduralFeature")) int32 EnabledFeatureMask = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0")) int32 SolverIterations = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0")) int32 MaxLookBones = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0")) int32 MaxSecondaryMotionBones = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier") bool bInterpolateSkippedFrames = true;
};

UCLASS(BlueprintType)
class PROCEDURALCHARACTERSRUNTIME_API UProceduralCharacterProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UProceduralCharacterProfile();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile") FName ProfileName = TEXT("ProceduralCharacter");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile") EProceduralCharacterArchetype Archetype = EProceduralCharacterArchetype::Zombie;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile") TArray<FProceduralLimbDefinition> Limbs;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile") TArray<FName> SpineBones;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile") TArray<FProceduralTierSettings> TierSettings;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile", meta = (ClampMin = "1", ClampMax = "16")) int32 MaxActiveImpulses = 4;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Significance", meta = (ClampMin = "0.0")) float SignificanceMultiplier = 1.0f;

    const FProceduralTierSettings* FindTierSettings(EProceduralAnimTier Tier) const;
};
