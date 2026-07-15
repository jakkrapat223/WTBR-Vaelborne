// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"

namespace
{
    UWTBRCompositeRegistryDataAsset* MakeSolgrixRegistry()
    {
        UWTBRCompositeRegistryDataAsset* Registry = NewObject<UWTBRCompositeRegistryDataAsset>();

        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Solux;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Fulgrix;
        Definition.CompositeType = EWTBRCompositeBulletType::Solgrix;

        return Registry;
    }
}

// ── Resolver contract: a composite recipe is an unordered archetype pair ──────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCompositeRegistryResolvesMatchingPairRegardlessOfOrderTest,
    "WTBR.Composite.Registry.ResolvesMatchingPairRegardlessOfOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCompositeRegistryResolvesMatchingPairRegardlessOfOrderTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRCompositeRegistryDataAsset* Registry = MakeSolgrixRegistry();
    if (!Registry) return false;

    TestEqual(TEXT("Solux + Fulgrix resolves to Solgrix"),
        Registry->ResolveCompositeType(EWTBRBulletArchetype::Solux, EWTBRBulletArchetype::Fulgrix),
        EWTBRCompositeBulletType::Solgrix);
    TestEqual(TEXT("Fulgrix + Solux resolves to the same Solgrix definition"),
        Registry->ResolveCompositeType(EWTBRBulletArchetype::Fulgrix, EWTBRBulletArchetype::Solux),
        EWTBRCompositeBulletType::Solgrix);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCompositeRegistryNoMatchReturnsNoneTest,
    "WTBR.Composite.Registry.NoMatchReturnsNone",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCompositeRegistryNoMatchReturnsNoneTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRCompositeRegistryDataAsset* Registry = MakeSolgrixRegistry();
    if (!Registry) return false;

    TestEqual(TEXT("An unregistered Solux + Venyx pair returns None"),
        Registry->ResolveCompositeType(EWTBRBulletArchetype::Solux, EWTBRBulletArchetype::Venyx),
        EWTBRCompositeBulletType::None);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCompositeRegistryNonCombinableOrNoneArchetypeNeverResolvesTest,
    "WTBR.Composite.Registry.NonCombinableOrNoneArchetypeNeverResolves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCompositeRegistryNonCombinableOrNoneArchetypeNeverResolvesTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRCompositeRegistryDataAsset* Registry = MakeSolgrixRegistry();
    if (!Registry) return false;

    TestEqual(TEXT("NonCombinable first archetype is rejected before lookup"),
        Registry->ResolveCompositeType(EWTBRBulletArchetype::NonCombinable, EWTBRBulletArchetype::Fulgrix),
        EWTBRCompositeBulletType::None);
    TestEqual(TEXT("NonCombinable second archetype is rejected before lookup"),
        Registry->ResolveCompositeType(EWTBRBulletArchetype::Solux, EWTBRBulletArchetype::NonCombinable),
        EWTBRCompositeBulletType::None);
    TestEqual(TEXT("None first archetype is rejected before lookup"),
        Registry->ResolveCompositeType(EWTBRBulletArchetype::None, EWTBRBulletArchetype::Fulgrix),
        EWTBRCompositeBulletType::None);
    TestEqual(TEXT("None second archetype is rejected before lookup"),
        Registry->ResolveCompositeType(EWTBRBulletArchetype::Solux, EWTBRBulletArchetype::None),
        EWTBRCompositeBulletType::None);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
