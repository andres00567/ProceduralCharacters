#pragma once

#include "CoreMinimal.h"
#include "ProceduralCharacterTypes.generated.h"

UENUM(BlueprintType)
enum class EProceduralAnimTier : uint8
{
    Hero UMETA(DisplayName = "P0 Hero"),
    Combat UMETA(DisplayName = "P1 Combat"),
    NearbyCrowd UMETA(DisplayName = "P2 Nearby Crowd"),
    Background UMETA(DisplayName = "P3 Background"),
    Dormant UMETA(DisplayName = "P4 Dormant")
};

// Blueprint enums are uint8-only in UE, while this mask intentionally has 18 bits.
// The reflected enum still drives BitmaskEnum pickers on the int32 properties.
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EProceduralFeature : uint32
{
    None = 0,
    MovementLean = 1 << 0,
    TurnLean = 1 << 1,
    LookAt = 1 << 2,
    Aim = 1 << 3,
    FootIK = 1 << 4,
    PelvisAdjustment = 1 << 5,
    HandIK = 1 << 6,
    WeaponHandling = 1 << 7,
    HitReactions = 1 << 8,
    LandingResponse = 1 << 9,
    AttackReach = 1 << 10,
    SecondaryMotion = 1 << 11,
    CreatureSpine = 1 << 12,
    MultiLegContacts = 1 << 13,
    FingerPlacement = 1 << 14,
    ControlRigPass = 1 << 15,
    EnvironmentContacts = 1 << 16,
    ViewmodelMotion = 1 << 17
};
ENUM_CLASS_FLAGS(EProceduralFeature);

USTRUCT(BlueprintType)
struct PROCEDURALCHARACTERSRUNTIME_API FProceduralAnimLODState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Procedural Animation")
    EProceduralAnimTier Tier = EProceduralAnimTier::Dormant;

    UPROPERTY(BlueprintReadOnly, Category = "Procedural Animation")
    float QualityMultiplier = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Procedural Animation")
    float PoseUpdateRateHz = 5.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Procedural Animation")
    float ProceduralUpdateRateHz = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Procedural Animation")
    float EnvironmentQueryRateHz = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Procedural Animation", meta = (Bitmask, BitmaskEnum = "/Script/ProceduralCharactersRuntime.EProceduralFeature"))
    int32 EnabledFeatureMask = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Procedural Animation")
    bool bInterpolateSkippedFrames = true;

    UPROPERTY(BlueprintReadOnly, Category = "Procedural Animation")
    bool bRecentlyRendered = false;
};

USTRUCT(BlueprintType)
struct PROCEDURALCHARACTERSRUNTIME_API FProceduralCharacterState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Movement") FVector WorldVelocity = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Movement") FVector LocalVelocity = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Movement") FVector WorldAcceleration = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Movement") FVector LocalAcceleration = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Movement") FVector AngularVelocity = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Movement") FVector DesiredMoveDirection = FVector::ForwardVector;
    UPROPERTY(BlueprintReadOnly, Category = "Movement") float GroundSpeed = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Movement") float VerticalSpeed = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Movement") float MovementDirectionDegrees = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Grounding") bool bGrounded = false;
    UPROPERTY(BlueprintReadOnly, Category = "Grounding") FVector FloorNormal = FVector::UpVector;
    UPROPERTY(BlueprintReadOnly, Category = "Grounding") float FloorDistance = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Grounding") float SlopeAngleDegrees = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Intent") FVector AimDirection = FVector::ForwardVector;
    UPROPERTY(BlueprintReadOnly, Category = "Intent") FVector LookTargetWorld = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Intent") FVector AttackTargetWorld = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Intent") bool bHasLookTarget = false;
    UPROPERTY(BlueprintReadOnly, Category = "Intent") bool bHasAttackTarget = false;
    UPROPERTY(BlueprintReadOnly, Category = "State") float GaitAlpha = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "State") float CrouchAlpha = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "State") float InjuryAlpha = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "State") float StaggerAlpha = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "State") float LandingStrength = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "State") float TurnStrength = 0.0f;
    /** Aggregated fixed-buffer impulse values. No arrays cross onto the animation worker. */
    UPROPERTY(BlueprintReadOnly, Category = "Reactions") FVector ProceduralLinearImpulse = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Reactions") FVector ProceduralAngularImpulse = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Reactions") float ProceduralImpulseAlpha = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Reactions", meta = (Bitmask, BitmaskEnum = "/Script/ProceduralCharactersRuntime.EProceduralFeature"))
    int32 ActiveImpulseFeatureMask = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Budget") FProceduralAnimLODState LOD;
};

UENUM(BlueprintType)
enum class EProceduralImpulseType : uint8
{
    WeaponRecoil, Landing, JumpLaunch, HitReaction, Stagger, Explosion,
    SuddenStop, MeleeImpact, DeathImpact, Custom
};

USTRUCT(BlueprintType)
struct PROCEDURALCHARACTERSRUNTIME_API FProceduralImpulse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impulse") EProceduralImpulseType Type = EProceduralImpulseType::Custom;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impulse") FVector LinearImpulse = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impulse") FVector AngularImpulse = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impulse") FVector WorldOrigin = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impulse") FName BoneName = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impulse") float Strength = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impulse", meta = (ClampMin = "0.0")) float Duration = 0.25f;
    UPROPERTY(BlueprintReadOnly, Category = "Impulse") float StartTimeSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct PROCEDURALCHARACTERSRUNTIME_API FProceduralSignificanceInput
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Significance") float ScreenCoverageScore = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Significance") float DistanceScore = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Significance") float ViewCenterScore = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Significance") float VisibilityScore = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Significance") float ThreatScore = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Significance") float RecentEventScore = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Significance") float SpecialEnemyScore = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Significance") bool bRecentlyRendered = false;
    UPROPERTY(BlueprintReadWrite, Category = "Significance") bool bAttackingPlayer = false;
    UPROPERTY(BlueprintReadWrite, Category = "Significance") bool bTargetedByPlayer = false;
    UPROPERTY(BlueprintReadWrite, Category = "Significance") bool bSpecialEnemy = false;
    UPROPERTY(BlueprintReadWrite, Category = "Significance") bool bBoss = false;
    UPROPERTY(BlueprintReadWrite, Category = "Significance") bool bLocallyControlled = false;
};

USTRUCT(BlueprintType)
struct PROCEDURALCHARACTERSRUNTIME_API FProceduralTierCaps
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Caps", meta = (ClampMin = "0")) int32 Hero = 8;
    UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Caps", meta = (ClampMin = "0")) int32 Combat = 24;
    UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Caps", meta = (ClampMin = "0")) int32 NearbyCrowd = 80;
    UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Caps", meta = (ClampMin = "0")) int32 Background = 160;
};

UENUM(BlueprintType)
enum class EProceduralQueryType : uint8
{
    Ground,
    Interaction,
    Climb,
    AttackReach
};

USTRUCT(BlueprintType)
struct PROCEDURALCHARACTERSRUNTIME_API FProceduralQueryBudget
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Queries", meta = (ClampMin = "0")) int32 MaxGroundQueriesPerFrame = 16;
    UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Queries", meta = (ClampMin = "0")) int32 MaxInteractionQueriesPerFrame = 8;
    UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Queries", meta = (ClampMin = "0")) int32 MaxClimbQueriesPerFrame = 4;
    UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Queries", meta = (ClampMin = "0")) int32 MaxAttackReachQueriesPerFrame = 8;
};

USTRUCT()
struct PROCEDURALCHARACTERSRUNTIME_API FProceduralPriorityBoost
{
    GENERATED_BODY()

    float MinimumSignificance = 0.0f;
    double ExpirationTimeSeconds = 0.0;
    FName Reason = NAME_None;
};

FORCEINLINE bool IsProceduralFeatureEnabled(const FProceduralAnimLODState& LOD, EProceduralFeature Feature)
{
    return (static_cast<uint32>(LOD.EnabledFeatureMask) & static_cast<uint32>(Feature)) != 0;
}
