// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "WTBRProjectileVFXTypes.generated.h"

class UNiagaraSystem;

// One optional override for a physical surface. If no entry matches, the
// projectile uses DefaultImpactEffect. Physical surface names are configured in
// Project Settings > Physics > Physical Surfaces.
USTRUCT(BlueprintType)
struct FWTBRSurfaceImpactVFX
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Surface")
    TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Surface")
    TObjectPtr<UNiagaraSystem> Effect = nullptr;
};

// Visual configuration lives with the Trigger Data Asset, so a weapon's VFX is
// selected with its gameplay definition and does not need Blueprint graph wiring.
USTRUCT(BlueprintType)
struct FWTBRProjectileVFXConfig
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
    TObjectPtr<UNiagaraSystem> TrailEffect = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
    TObjectPtr<UNiagaraSystem> DefaultImpactEffect = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
    TArray<FWTBRSurfaceImpactVFX> SurfaceImpactOverrides;

    // Enables C++ spawning for this projectile. When false, the existing
    // OnProjectileHitVFX Blueprint event remains the active implementation.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
    bool bUseBuiltInImpactVFX = false;

    // Cosmetic cull distance in cm. Zero keeps the effect visible at all
    // distances; clients beyond this distance do not spawn the impact system.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Performance",
        meta = (ClampMin = "0.0"))
    float MaxImpactVFXDistance = 20000.0f;
};
