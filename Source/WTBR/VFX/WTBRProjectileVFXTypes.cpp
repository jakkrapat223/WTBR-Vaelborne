// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "VFX/WTBRProjectileVFXTypes.h"
#include "NiagaraSystem.h"

UNiagaraSystem* WTBRResolveSurfaceImpactEffect(
    UNiagaraSystem* DefaultEffect,
    const TArray<FWTBRSurfaceImpactVFX>& SurfaceOverrides,
    uint8 SurfaceType)
{
    for (const FWTBRSurfaceImpactVFX& Override : SurfaceOverrides)
    {
        UNiagaraSystem* Effect = Override.Effect.Get();
        if (static_cast<uint8>(Override.SurfaceType.GetValue()) == SurfaceType
            && IsValid(Effect))
        {
            return Effect;
        }
    }
    return DefaultEffect;
}
