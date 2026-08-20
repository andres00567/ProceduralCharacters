#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/ProceduralCharacterComponent.h"
#include "Components/ProceduralViewmodelComponent.h"
#include "Subsystems/ProceduralAnimationBudgetSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralTierCapsTest,
    "ProceduralCharacters.Budget.HardTierCaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProceduralTierCapsTest::RunTest(const FString& Parameters)
{
    TArray<float> Scores;
    for (int32 Index = 0; Index < 500; ++Index)
    {
        Scores.Add(static_cast<float>(500 - Index) / 500.0f);
    }
    FProceduralTierCaps Caps;
    Caps.Hero = 8;
    Caps.Combat = 24;
    Caps.NearbyCrowd = 80;
    Caps.Background = 160;
    TArray<EProceduralAnimTier> Tiers;
    UProceduralAnimationBudgetSubsystem::AssignTiersByScore(Scores, Caps, Tiers);
    const auto CountTier = [&Tiers](EProceduralAnimTier Tier)
    {
        int32 Count = 0;
        for (EProceduralAnimTier Assigned : Tiers) Count += Assigned == Tier ? 1 : 0;
        return Count;
    };
    TestEqual(TEXT("All 500 candidates assigned"), Tiers.Num(), 500);
    TestEqual(TEXT("Hero cap"), CountTier(EProceduralAnimTier::Hero), Caps.Hero);
    TestEqual(TEXT("Combat cap"), CountTier(EProceduralAnimTier::Combat), Caps.Combat);
    TestEqual(TEXT("Nearby cap"), CountTier(EProceduralAnimTier::NearbyCrowd), Caps.NearbyCrowd);
    TestEqual(TEXT("Background cap"), CountTier(EProceduralAnimTier::Background), Caps.Background);
    TestEqual(TEXT("Remaining dormant"), CountTier(EProceduralAnimTier::Dormant),
        500 - Caps.Hero - Caps.Combat - Caps.NearbyCrowd - Caps.Background);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralPriorityBoostExpirationTest,
    "ProceduralCharacters.Budget.PriorityBoostExpiration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProceduralPriorityBoostExpirationTest::RunTest(const FString& Parameters)
{
    FProceduralPriorityBoost Boost;
    Boost.MinimumSignificance = 0.9f;
    Boost.ExpirationTimeSeconds = 12.0;
    TestTrue(TEXT("Boost active before expiration"),
        UProceduralCharacterComponent::IsPriorityBoostActive(Boost, 11.999));
    TestFalse(TEXT("Boost expires at boundary"),
        UProceduralCharacterComponent::IsPriorityBoostActive(Boost, 12.0));
    TestFalse(TEXT("Boost remains expired"),
        UProceduralCharacterComponent::IsPriorityBoostActive(Boost, 20.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralFeatureMaskTest,
    "ProceduralCharacters.Animation.FeatureMaskEarlyOut",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProceduralFeatureMaskTest::RunTest(const FString& Parameters)
{
    FProceduralAnimLODState LOD;
    LOD.EnabledFeatureMask = static_cast<int32>(
        EProceduralFeature::MovementLean | EProceduralFeature::HitReactions);
    TestTrue(TEXT("Enabled lean is visible to native nodes"),
        IsProceduralFeatureEnabled(LOD, EProceduralFeature::MovementLean));
    TestTrue(TEXT("Enabled hit reactions are visible to native nodes"),
        IsProceduralFeatureEnabled(LOD, EProceduralFeature::HitReactions));
    TestFalse(TEXT("Disabled look chain hard-outs"),
        IsProceduralFeatureEnabled(LOD, EProceduralFeature::LookAt));
    LOD.EnabledFeatureMask = 0;
    TestFalse(TEXT("Zero mask disables all universal work"),
        IsProceduralFeatureEnabled(LOD, EProceduralFeature::MovementLean));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralViewmodelMotionTest,
    "ProceduralCharacters.Viewmodel.LocalMotionAndReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProceduralViewmodelMotionTest::RunTest(const FString& Parameters)
{
    UProceduralViewmodelComponent* Viewmodel =
        NewObject<UProceduralViewmodelComponent>(GetTransientPackage());
    TestNotNull(TEXT("Viewmodel component can be created without an actor tick"), Viewmodel);
    if (!Viewmodel)
    {
        return false;
    }

    Viewmodel->AddRecoilImpulse(4.0f);
    for (int32 Frame = 0; Frame < 12; ++Frame)
    {
        Viewmodel->UpdateViewmodel(
            1.0f / 60.0f, FRotator(2.0f, 8.0f, 0.0f),
            FVector::ZeroVector, FVector::ForwardVector,
            FVector(450.0f, 120.0f, 0.0f),
            FVector(900.0f, -400.0f, 0.0f),
            600.0f, 0.25f, true, 0.0f, false, 1.0f);
    }

    const FProceduralViewmodelState& ActiveState = Viewmodel->GetViewmodelState();
    TestFalse(TEXT("Movement and recoil produce a visible camera-local offset"),
        ActiveState.ViewmodelOffset.Equals(FTransform::Identity, KINDA_SMALL_NUMBER));
    TestEqual(TEXT("Aim alpha is preserved for presentation consumers"),
        ActiveState.AimAlpha, 0.25f);
    TestTrue(TEXT("Recoil exposes a normalized presentation alpha"),
        ActiveState.RecoilAlpha > 0.0f);

    for (int32 Frame = 0; Frame < 30; ++Frame)
    {
        Viewmodel->UpdateViewmodel(
            1.0f / 60.0f, FRotator::ZeroRotator,
            FVector::ZeroVector, FVector::ForwardVector,
            FVector::ZeroVector, FVector::ZeroVector,
            600.0f, 0.0f, true, 0.0f, true, 1.0f);
    }
    const FProceduralViewmodelState& ReloadState = Viewmodel->GetViewmodelState();
    TestTrue(TEXT("Reload produces shared procedural root motion"),
        ReloadState.ReloadAlpha > 0.9f
        && !ReloadState.ReloadOffset.Equals(FTransform::Identity, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Reload exposes a normalized curve clock"),
        FMath::IsNearlyEqual(ReloadState.ReloadProgress, 0.5f, 0.02f));
    TestTrue(TEXT("Animation guidance is enabled by default"),
        Viewmodel->ShouldUseReloadAnimationGuide()
        && FMath::IsNearlyEqual(Viewmodel->GetReloadAnimationGuideStrength(), 1.0f));

    Viewmodel->ResetViewmodel();
    const FProceduralViewmodelState& ResetState = Viewmodel->GetViewmodelState();
    TestTrue(TEXT("Reset restores the identity viewmodel transform"),
        ResetState.ViewmodelOffset.Equals(FTransform::Identity));
    TestEqual(TEXT("Reset clears recoil alpha"), ResetState.RecoilAlpha, 0.0f);
    TestEqual(TEXT("Reset clears reload alpha"), ResetState.ReloadAlpha, 0.0f);
    TestEqual(TEXT("Reset clears reload progress"), ResetState.ReloadProgress, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralPalmAnchorTransformTest,
    "ProceduralCharacters.Animation.PalmAnchorDerivesWrist",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProceduralPalmAnchorTransformTest::RunTest(const FString& Parameters)
{
    const FTransform IncomingHand(FRotator(12.0f, -18.0f, 7.0f), FVector(21.0f, 8.0f, 34.0f));
    // The middle finger can have an arbitrary curl/rotation. Only its position
    // contributes to the stable grip frame; wrist axes supply orientation.
    const FTransform IncomingMiddleFinger(FRotator(80.0f, 42.0f, -63.0f), FVector(29.0f, 5.0f, 31.0f));
    const FTransform IncomingGripFrame(
        IncomingHand.GetRotation(), IncomingMiddleFinger.GetLocation());
    const FTransform GripFrameRelativeToHand = IncomingGripFrame.GetRelativeTransform(IncomingHand);
    const FTransform DesiredPalm(FRotator(-35.0f, 70.0f, 16.0f), FVector(60.0f, -12.0f, 48.0f));

    const FTransform DerivedHand = GripFrameRelativeToHand.Inverse() * DesiredPalm;
    const FTransform ReconstructedPalm = GripFrameRelativeToHand * DerivedHand;
    TestTrue(TEXT("Derived wrist places the middle-palm anchor at the grip socket"),
        ReconstructedPalm.GetLocation().Equals(DesiredPalm.GetLocation(), 0.01f));
    TestTrue(TEXT("Derived wrist aligns the middle-palm anchor to the grip socket axes"),
        ReconstructedPalm.GetRotation().AngularDistance(DesiredPalm.GetRotation()) < 0.001f);
    return true;
}

#endif
