#include "Subsystems/ProceduralAnimationBudgetSubsystem.h"

#include "Components/ProceduralCharacterComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Data/ProceduralAnimationBudgetSettings.h"
#include "Data/ProceduralCharacterProfile.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ProceduralCharactersRuntime.h"

namespace ProceduralAnimationCVars
{
    static TAutoConsoleVariable<int32> Debug(
        TEXT("pc.Anim.Debug"), 0, TEXT("Draw procedural animation budget state."), ECVF_Cheat);
    static TAutoConsoleVariable<int32> ShowTiers(
        TEXT("pc.Anim.ShowTiers"), 0, TEXT("Draw assigned procedural tiers."), ECVF_Cheat);
    static TAutoConsoleVariable<int32> ShowSignificance(
        TEXT("pc.Anim.ShowSignificance"), 0, TEXT("Draw procedural significance values."), ECVF_Cheat);
    static TAutoConsoleVariable<int32> ForceTier(
        TEXT("pc.Anim.ForceTier"), -1, TEXT("Force tier 0-4; -1 restores budgeting."), ECVF_Cheat);
    static TAutoConsoleVariable<int32> FreezeBudget(
        TEXT("pc.Anim.FreezeBudget"), 0, TEXT("Freeze tier assignments while state scheduling continues."), ECVF_Cheat);
    static TAutoConsoleVariable<int32> HeroCap(
        TEXT("pc.Anim.HeroCap"), -1, TEXT("Override P0 cap; -1 uses project settings."), ECVF_Default);
    static TAutoConsoleVariable<int32> CombatCap(
        TEXT("pc.Anim.CombatCap"), -1, TEXT("Override P1 cap; -1 uses project settings."), ECVF_Default);
    static TAutoConsoleVariable<int32> NearbyCap(
        TEXT("pc.Anim.NearbyCap"), -1, TEXT("Override P2 cap; -1 uses project settings."), ECVF_Default);
    static TAutoConsoleVariable<int32> BackgroundCap(
        TEXT("pc.Anim.BackgroundCap"), -1, TEXT("Override P3 cap; -1 uses project settings."), ECVF_Default);

    static void DumpStats(const TArray<FString>&, UWorld* World)
    {
        const UProceduralAnimationBudgetSubsystem* Budget =
            World ? World->GetSubsystem<UProceduralAnimationBudgetSubsystem>() : nullptr;
        if (!Budget)
        {
            UE_LOG(LogProceduralCharacters, Display, TEXT("No procedural animation budget subsystem in this world."));
            return;
        }
        UE_LOG(LogProceduralCharacters, Display,
            TEXT("Registered=%d P0=%d P1=%d P2=%d P3=%d P4=%d Queries=%d/%d/%d/%d Rejected=%d"),
            Budget->GetRegisteredCharacterCount(),
            Budget->GetTierPopulation(EProceduralAnimTier::Hero),
            Budget->GetTierPopulation(EProceduralAnimTier::Combat),
            Budget->GetTierPopulation(EProceduralAnimTier::NearbyCrowd),
            Budget->GetTierPopulation(EProceduralAnimTier::Background),
            Budget->GetTierPopulation(EProceduralAnimTier::Dormant),
            Budget->GetQueriesConsumedThisFrame(EProceduralQueryType::Ground),
            Budget->GetQueriesConsumedThisFrame(EProceduralQueryType::Interaction),
            Budget->GetQueriesConsumedThisFrame(EProceduralQueryType::Climb),
            Budget->GetQueriesConsumedThisFrame(EProceduralQueryType::AttackReach),
            Budget->GetQueriesRejectedThisFrame());
    }

    static FAutoConsoleCommandWithWorldAndArgs DumpStatsCommand(
        TEXT("pc.Anim.DumpStats"), TEXT("Log procedural character counts by tier."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DumpStats));
}

bool UProceduralAnimationBudgetSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UProceduralAnimationBudgetSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UProceduralAnimationBudgetSubsystem, STATGROUP_Tickables);
}

void UProceduralAnimationBudgetSubsystem::RegisterCharacter(UProceduralCharacterComponent* Character)
{
    if (!IsValid(Character) || RegisteredCharacters.ContainsByPredicate(
        [Character](const FRegisteredCharacter& Entry) { return Entry.Component == Character; }))
    {
        return;
    }
    FRegisteredCharacter& Entry = RegisteredCharacters.AddDefaulted_GetRef();
    Entry.Component = Character;
    Entry.StableId = NextStableId++;
    Entry.LastStateUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    Entry.NextStateUpdateTime = Entry.LastStateUpdateTime +
        FMath::Fmod(Entry.StableId * 0.61803398875, 1.0) * 0.1;
    Character->SetStableRegistrationId(Entry.StableId);
    Character->GatherScheduledState(1.0f / 60.0f);
    LastSignificanceUpdateTime = -BIG_NUMBER;
}

void UProceduralAnimationBudgetSubsystem::UnregisterCharacter(UProceduralCharacterComponent* Character)
{
    RegisteredCharacters.RemoveAllSwap([Character](const FRegisteredCharacter& Entry)
    {
        return !Entry.Component.IsValid() || Entry.Component == Character;
    }, EAllowShrinking::No);
    if (Character)
    {
        Character->SetStableRegistrationId(INDEX_NONE);
    }
}

int32 UProceduralAnimationBudgetSubsystem::GetRegisteredCharacterCount() const
{
    int32 Count = 0;
    for (const FRegisteredCharacter& Entry : RegisteredCharacters)
    {
        Count += Entry.Component.IsValid() ? 1 : 0;
    }
    return Count;
}

int32 UProceduralAnimationBudgetSubsystem::GetTierPopulation(EProceduralAnimTier Tier) const
{
    return TierPopulations[FMath::Clamp(static_cast<int32>(Tier), 0, 4)];
}

void UProceduralAnimationBudgetSubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    const double Now = World->GetTimeSeconds();
    FMemory::Memzero(QueriesConsumed, sizeof(QueriesConsumed));
    QueriesRejectedThisFrame = 0;
    const UProceduralAnimationBudgetSettings* Settings = GetDefault<UProceduralAnimationBudgetSettings>();
    if (!ProceduralAnimationCVars::FreezeBudget.GetValueOnGameThread() &&
        Now - LastSignificanceUpdateTime >= Settings->SignificanceUpdateInterval)
    {
        RecalculateSignificanceAndTiers(Now);
        LastSignificanceUpdateTime = Now;
    }

    for (FRegisteredCharacter& Entry : RegisteredCharacters)
    {
        UProceduralCharacterComponent* Character = Entry.Component.Get();
        if (!Character)
        {
            continue;
        }
        const float Rate = Character->GetGameThreadState().LOD.ProceduralUpdateRateHz;
        if (Rate <= 0.0f)
        {
            continue;
        }
        const double Interval = 1.0 / Rate;
        if (Now >= Entry.NextStateUpdateTime)
        {
            Character->GatherScheduledState(static_cast<float>(Now - Entry.LastStateUpdateTime));
            Entry.LastStateUpdateTime = Now;
            Entry.NextStateUpdateTime = Now + Interval;
        }
    }

    if (ProceduralAnimationCVars::Debug.GetValueOnGameThread() ||
        ProceduralAnimationCVars::ShowTiers.GetValueOnGameThread() ||
        ProceduralAnimationCVars::ShowSignificance.GetValueOnGameThread())
    {
        DrawDebugState();
    }
    if ((GFrameCounter & 127) == 0)
    {
        CompactInvalidRegistrations();
    }
}

bool UProceduralAnimationBudgetSubsystem::TryConsumeQueryBudget(
    const UProceduralCharacterComponent* Character, EProceduralQueryType QueryType)
{
    if (!Character || Character->GetAssignedTier() > EProceduralAnimTier::NearbyCrowd ||
        Character->GetGameThreadState().LOD.EnvironmentQueryRateHz <= 0.0f)
    {
        ++QueriesRejectedThisFrame;
        return false;
    }
    const FProceduralQueryBudget& Budget =
        GetDefault<UProceduralAnimationBudgetSettings>()->QueryBudget;
    const int32 Limits[] = {
        Budget.MaxGroundQueriesPerFrame,
        Budget.MaxInteractionQueriesPerFrame,
        Budget.MaxClimbQueriesPerFrame,
        Budget.MaxAttackReachQueriesPerFrame
    };
    const int32 Index = FMath::Clamp(static_cast<int32>(QueryType), 0, 3);
    if (QueriesConsumed[Index] >= Limits[Index])
    {
        ++QueriesRejectedThisFrame;
        return false;
    }
    ++QueriesConsumed[Index];
    return true;
}

int32 UProceduralAnimationBudgetSubsystem::GetQueriesConsumedThisFrame(EProceduralQueryType QueryType) const
{
    return QueriesConsumed[FMath::Clamp(static_cast<int32>(QueryType), 0, 3)];
}

float UProceduralAnimationBudgetSubsystem::CalculateSignificance(const FProceduralSignificanceInput& Input)
{
    float Score =
        Input.ScreenCoverageScore * 0.30f +
        Input.DistanceScore * 0.20f +
        Input.ViewCenterScore * 0.15f +
        Input.VisibilityScore * 0.10f +
        Input.ThreatScore * 0.15f +
        Input.RecentEventScore * 0.10f;
    if (!Input.bRecentlyRendered)
    {
        Score *= 0.20f;
    }
    if (Input.bAttackingPlayer) Score = FMath::Max(Score, 0.85f);
    if (Input.bTargetedByPlayer) Score = FMath::Max(Score, 0.70f);
    if (Input.bSpecialEnemy) Score = FMath::Max(Score, 0.75f);
    if (Input.bBoss || Input.bLocallyControlled) Score = 1.0f;
    return FMath::Clamp(Score, 0.0f, 1.0f);
}

void UProceduralAnimationBudgetSubsystem::AssignTiersByScore(
    const TArray<float>& Scores, const FProceduralTierCaps& Caps,
    TArray<EProceduralAnimTier>& OutTiers)
{
    TArray<int32> Order;
    Order.Reserve(Scores.Num());
    for (int32 Index = 0; Index < Scores.Num(); ++Index) Order.Add(Index);
    Order.Sort([&Scores](int32 A, int32 B)
    {
        if (!FMath::IsNearlyEqual(Scores[A], Scores[B])) return Scores[A] > Scores[B];
        return A < B;
    });
    OutTiers.Init(EProceduralAnimTier::Dormant, Scores.Num());
    int32 Cursor = 0;
    const auto Assign = [&Order, &OutTiers, &Cursor](EProceduralAnimTier Tier, int32 Count)
    {
        const int32 End = FMath::Min(Cursor + FMath::Max(0, Count), Order.Num());
        while (Cursor < End) OutTiers[Order[Cursor++]] = Tier;
    };
    Assign(EProceduralAnimTier::Hero, Caps.Hero);
    Assign(EProceduralAnimTier::Combat, Caps.Combat);
    Assign(EProceduralAnimTier::NearbyCrowd, Caps.NearbyCrowd);
    Assign(EProceduralAnimTier::Background, Caps.Background);
}

void UProceduralAnimationBudgetSubsystem::RecalculateSignificanceAndTiers(double NowSeconds)
{
    TArray<FViewpoint, TInlineAllocator<4>> Viewpoints;
    GatherViewpoints(Viewpoints);
    TArray<float> RankingScores;
    TArray<bool> RecentlyRendered;
    RankingScores.Reserve(RegisteredCharacters.Num());
    RecentlyRendered.Reserve(RegisteredCharacters.Num());
    const UProceduralAnimationBudgetSettings* Settings = GetDefault<UProceduralAnimationBudgetSettings>();

    for (FRegisteredCharacter& Entry : RegisteredCharacters)
    {
        UProceduralCharacterComponent* Character = Entry.Component.Get();
        if (!Character)
        {
            RankingScores.Add(-1.0f);
            RecentlyRendered.Add(false);
            continue;
        }
        const FProceduralSignificanceInput Input = BuildSignificanceInput(*Character, Viewpoints, NowSeconds);
        Entry.Significance = CalculateSignificance(Input);
        if (UProceduralCharacterComponent::IsPriorityBoostActive(Character->GetPriorityBoost(), NowSeconds))
        {
            Entry.Significance = FMath::Max(Entry.Significance, Character->GetPriorityBoost().MinimumSignificance);
        }
        if (const UProceduralCharacterProfile* Profile = Character->GetProfile())
        {
            Entry.Significance = FMath::Clamp(Entry.Significance * Profile->SignificanceMultiplier, 0.0f, 1.0f);
        }
        float RankingScore = Entry.Significance;
        if (Character->GetAssignedTier() != EProceduralAnimTier::Dormant)
        {
            RankingScore += Settings->DemotionHysteresisBonus;
        }
        RankingScores.Add(RankingScore);
        RecentlyRendered.Add(Input.bRecentlyRendered);
    }

    TArray<EProceduralAnimTier> Tiers;
    AssignTiersByScore(RankingScores, GetEffectiveCaps(), Tiers);
    const int32 ForcedTier = ProceduralAnimationCVars::ForceTier.GetValueOnGameThread();
    FMemory::Memzero(TierPopulations, sizeof(TierPopulations));
    for (int32 Index = 0; Index < RegisteredCharacters.Num(); ++Index)
    {
        if (UProceduralCharacterComponent* Character = RegisteredCharacters[Index].Component.Get())
        {
            const EProceduralAnimTier Tier = ForcedTier >= 0 && ForcedTier <= 4
                ? static_cast<EProceduralAnimTier>(ForcedTier) : Tiers[Index];
            ++TierPopulations[static_cast<int32>(Tier)];
            Character->ApplyBudgetResult(
                BuildLODState(*Character, Tier, RecentlyRendered[Index]),
                RegisteredCharacters[Index].Significance);
        }
    }
}

void UProceduralAnimationBudgetSubsystem::GatherViewpoints(
    TArray<FViewpoint, TInlineAllocator<4>>& OutViewpoints) const
{
    const UWorld* World = GetWorld();
    if (!World) return;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* Controller = It->Get();
        if (!Controller || !Controller->IsLocalController()) continue;
        FRotator Rotation;
        FViewpoint& Viewpoint = OutViewpoints.AddDefaulted_GetRef();
        Controller->GetPlayerViewPoint(Viewpoint.Location, Rotation);
        Viewpoint.Forward = Rotation.Vector();
    }
}

FProceduralSignificanceInput UProceduralAnimationBudgetSubsystem::BuildSignificanceInput(
    const UProceduralCharacterComponent& Character, TConstArrayView<FViewpoint> Viewpoints,
    double NowSeconds) const
{
    FProceduralSignificanceInput Input;
    const AActor* Owner = Character.GetOwner();
    if (!Owner) return Input;
    Input.bLocallyControlled = Character.IsLocallyControlledForBudget();
    Input.bSpecialEnemy = Character.IsSpecialEnemyForBudget();
    Input.bBoss = Character.IsBossForBudget();
    Input.ThreatScore = Character.GetGameplayThreat();
    Input.bAttackingPlayer = Character.GetGameThreadState().bHasAttackTarget;
    Input.RecentEventScore = UProceduralCharacterComponent::IsPriorityBoostActive(
        Character.GetPriorityBoost(), NowSeconds) ? Character.GetPriorityBoost().MinimumSignificance : 0.0f;
    if (const UPrimitiveComponent* Primitive = Owner->FindComponentByClass<UPrimitiveComponent>())
    {
        Input.bRecentlyRendered = Primitive->WasRecentlyRendered(0.25f);
    }
    Input.VisibilityScore = Input.bRecentlyRendered ? 1.0f : 0.0f;
    if (Viewpoints.IsEmpty())
    {
        return Input;
    }
    const float MaxDistance = GetDefault<UProceduralAnimationBudgetSettings>()->MaximumSignificanceDistance;
    FVector Origin;
    FVector Extents;
    Owner->GetActorBounds(false, Origin, Extents);
    const float Radius = FMath::Max(Extents.Size(), 25.0f);
    for (const FViewpoint& Viewpoint : Viewpoints)
    {
        const FVector ToActor = Origin - Viewpoint.Location;
        const float Distance = ToActor.Size();
        const float DistanceScore = 1.0f - FMath::Clamp(Distance / MaxDistance, 0.0f, 1.0f);
        const float CenterScore = FMath::Square(FMath::Max(0.0f, FVector::DotProduct(
            ToActor.GetSafeNormal(), Viewpoint.Forward)));
        const float CoverageScore = FMath::Clamp((Radius / FMath::Max(Distance, Radius)) * 8.0f, 0.0f, 1.0f);
        Input.DistanceScore = FMath::Max(Input.DistanceScore, DistanceScore);
        Input.ViewCenterScore = FMath::Max(Input.ViewCenterScore, CenterScore);
        Input.ScreenCoverageScore = FMath::Max(Input.ScreenCoverageScore, CoverageScore);
    }
    return Input;
}

FProceduralAnimLODState UProceduralAnimationBudgetSubsystem::BuildLODState(
    const UProceduralCharacterComponent& Character, EProceduralAnimTier Tier,
    bool bRecentlyRendered) const
{
    FProceduralAnimLODState LOD;
    LOD.Tier = Tier;
    LOD.bRecentlyRendered = bRecentlyRendered;
    const UProceduralAnimationBudgetSettings* BudgetSettings = GetDefault<UProceduralAnimationBudgetSettings>();
    static constexpr float Quality[] = { 1.0f, 0.8f, 0.5f, 0.2f, 0.0f };
    LOD.QualityMultiplier = Quality[static_cast<int32>(Tier)] * BudgetSettings->RuntimeBudgetScale;
    const UProceduralCharacterProfile* Profile = Character.GetProfile();
    const FProceduralTierSettings* Settings = Profile ? Profile->FindTierSettings(Tier) : nullptr;
    if (Settings)
    {
        LOD.PoseUpdateRateHz = Settings->PoseUpdateRateHz;
        LOD.ProceduralUpdateRateHz = Settings->ProceduralUpdateRateHz;
        LOD.EnvironmentQueryRateHz = Settings->EnvironmentQueryRateHz;
        LOD.EnabledFeatureMask = Settings->EnabledFeatureMask;
        LOD.bInterpolateSkippedFrames = Settings->bInterpolateSkippedFrames;
    }
    else
    {
        static constexpr float PoseRates[] = { 60.0f, 30.0f, 15.0f, 7.5f, 0.0f };
        static constexpr float ProceduralRates[] = { 60.0f, 30.0f, 10.0f, 3.0f, 0.0f };
        LOD.PoseUpdateRateHz = PoseRates[static_cast<int32>(Tier)];
        LOD.ProceduralUpdateRateHz = ProceduralRates[static_cast<int32>(Tier)];
        LOD.EnvironmentQueryRateHz = 0.0f;
        static constexpr int32 FeatureMasks[] = {
            static_cast<int32>(EProceduralFeature::MovementLean | EProceduralFeature::TurnLean |
                EProceduralFeature::LookAt | EProceduralFeature::Aim |
                EProceduralFeature::HitReactions | EProceduralFeature::LandingResponse |
                EProceduralFeature::WeaponHandling),
            static_cast<int32>(EProceduralFeature::MovementLean | EProceduralFeature::TurnLean |
                EProceduralFeature::LookAt | EProceduralFeature::HitReactions |
                EProceduralFeature::LandingResponse),
            static_cast<int32>(EProceduralFeature::MovementLean | EProceduralFeature::LookAt |
                EProceduralFeature::HitReactions),
            0,
            0
        };
        LOD.EnabledFeatureMask = FeatureMasks[static_cast<int32>(Tier)];
    }
    return LOD;
}

FProceduralTierCaps UProceduralAnimationBudgetSubsystem::GetEffectiveCaps() const
{
    FProceduralTierCaps Caps = GetDefault<UProceduralAnimationBudgetSettings>()->TierCaps;
    const auto Override = [](int32 Value, int32 Configured) { return Value >= 0 ? Value : Configured; };
    Caps.Hero = Override(ProceduralAnimationCVars::HeroCap.GetValueOnGameThread(), Caps.Hero);
    Caps.Combat = Override(ProceduralAnimationCVars::CombatCap.GetValueOnGameThread(), Caps.Combat);
    Caps.NearbyCrowd = Override(ProceduralAnimationCVars::NearbyCap.GetValueOnGameThread(), Caps.NearbyCrowd);
    Caps.Background = Override(ProceduralAnimationCVars::BackgroundCap.GetValueOnGameThread(), Caps.Background);
    return Caps;
}

void UProceduralAnimationBudgetSubsystem::DrawDebugState() const
{
    const bool bShowTier = ProceduralAnimationCVars::Debug.GetValueOnGameThread() ||
        ProceduralAnimationCVars::ShowTiers.GetValueOnGameThread();
    const bool bShowScore = ProceduralAnimationCVars::Debug.GetValueOnGameThread() ||
        ProceduralAnimationCVars::ShowSignificance.GetValueOnGameThread();
    for (const FRegisteredCharacter& Entry : RegisteredCharacters)
    {
        const UProceduralCharacterComponent* Character = Entry.Component.Get();
        const AActor* Owner = Character ? Character->GetOwner() : nullptr;
        if (!Owner) continue;
        const EProceduralAnimTier Tier = Character->GetAssignedTier();
        const TCHAR* TierName[] = { TEXT("P0 Hero"), TEXT("P1 Combat"), TEXT("P2 Nearby"), TEXT("P3 Background"), TEXT("P4 Dormant") };
        const FColor TierColor[] = { FColor::Cyan, FColor::Red, FColor::Yellow, FColor::Green, FColor::Silver };
        FString Label;
        if (bShowTier) Label += TierName[static_cast<int32>(Tier)];
        if (bShowScore) Label += FString::Printf(TEXT("  %.3f"), Character->GetSignificance());
        if (Character->GetProfile()) Label += FString::Printf(TEXT("  %s"), *Character->GetProfile()->ProfileName.ToString());
        DrawDebugString(GetWorld(), Owner->GetActorLocation() + FVector(0, 0, 120), Label,
            nullptr, TierColor[static_cast<int32>(Tier)], 0.0f, true, 1.0f);
    }
}

void UProceduralAnimationBudgetSubsystem::CompactInvalidRegistrations()
{
    RegisteredCharacters.RemoveAllSwap([](const FRegisteredCharacter& Entry)
    {
        return !Entry.Component.IsValid();
    }, EAllowShrinking::No);
}
