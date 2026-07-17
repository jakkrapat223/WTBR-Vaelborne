// Copyright Vaelborne: Dominion. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "NiagaraSystem.h"
#include "VFX/WTBRProjectileVFXTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVFXSurfaceRoutingTest,
    "WTBR.VFX.SurfaceRouting.UsesMatchingEffectAndFallsBackToDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVFXSurfaceRoutingTest::RunTest(const FString& /*Parameters*/)
{
    UNiagaraSystem* DefaultEffect = NewObject<UNiagaraSystem>(GetTransientPackage());
    UNiagaraSystem* MetalEffect = NewObject<UNiagaraSystem>(GetTransientPackage());
    UNiagaraSystem* ShieldEffect = NewObject<UNiagaraSystem>(GetTransientPackage());

    TArray<FWTBRSurfaceImpactVFX> Overrides;
    FWTBRSurfaceImpactVFX& Metal = Overrides.AddDefaulted_GetRef();
    Metal.SurfaceType = SurfaceType1;
    Metal.Effect = MetalEffect;
    FWTBRSurfaceImpactVFX& Shield = Overrides.AddDefaulted_GetRef();
    Shield.SurfaceType = SurfaceType4;
    Shield.Effect = ShieldEffect;

    TestEqual(TEXT("Metal resolves to its configured effect"),
        WTBRResolveSurfaceImpactEffect(DefaultEffect, Overrides, SurfaceType1), MetalEffect);
    TestEqual(TEXT("Shield resolves to its configured effect"),
        WTBRResolveSurfaceImpactEffect(DefaultEffect, Overrides, SurfaceType4), ShieldEffect);
    TestEqual(TEXT("Unmapped surface falls back to the weapon default"),
        WTBRResolveSurfaceImpactEffect(DefaultEffect, Overrides, SurfaceType2), DefaultEffect);

    // A stale/null override must never suppress the weapon's default impact.
    Overrides[0].Effect = nullptr;
    TestEqual(TEXT("Null mapped override falls back to the default"),
        WTBRResolveSurfaceImpactEffect(DefaultEffect, Overrides, SurfaceType1), DefaultEffect);
    return true;
}

#endif
