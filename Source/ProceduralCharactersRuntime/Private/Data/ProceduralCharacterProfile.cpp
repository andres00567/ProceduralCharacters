#include "Data/ProceduralCharacterProfile.h"

UProceduralCharacterProfile::UProceduralCharacterProfile()
{
    const int32 HeroFeatures = static_cast<int32>(
        EProceduralFeature::MovementLean | EProceduralFeature::TurnLean |
        EProceduralFeature::LookAt | EProceduralFeature::Aim |
        EProceduralFeature::FootIK | EProceduralFeature::PelvisAdjustment |
        EProceduralFeature::HandIK | EProceduralFeature::WeaponHandling |
        EProceduralFeature::HitReactions | EProceduralFeature::LandingResponse |
        EProceduralFeature::AttackReach | EProceduralFeature::SecondaryMotion |
        EProceduralFeature::EnvironmentContacts);
    TierSettings = {
        { EProceduralAnimTier::Hero, 60.0f, 60.0f, 30.0f, HeroFeatures, 4, 5, 8, true },
        { EProceduralAnimTier::Combat, 30.0f, 30.0f, 15.0f,
            static_cast<int32>(EProceduralFeature::MovementLean | EProceduralFeature::TurnLean |
                EProceduralFeature::LookAt | EProceduralFeature::HitReactions |
                EProceduralFeature::FootIK | EProceduralFeature::PelvisAdjustment), 2, 3, 0, true },
        { EProceduralAnimTier::NearbyCrowd, 15.0f, 10.0f, 0.0f,
            static_cast<int32>(EProceduralFeature::MovementLean | EProceduralFeature::LookAt |
                EProceduralFeature::HitReactions), 1, 2, 0, true },
        { EProceduralAnimTier::Background, 7.5f, 3.0f, 0.0f, 0, 0, 0, 0, true },
        { EProceduralAnimTier::Dormant, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0, false }
    };
}

const FProceduralTierSettings* UProceduralCharacterProfile::FindTierSettings(EProceduralAnimTier Tier) const
{
    return TierSettings.FindByPredicate([Tier](const FProceduralTierSettings& Settings)
    {
        return Settings.Tier == Tier;
    });
}
