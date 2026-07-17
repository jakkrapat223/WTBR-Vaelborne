// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "WTBRProjectileVFXTypes.generated.h"

class UCameraShakeBase;
class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;

// A template-defined Niagara User parameter that receives an existing content
// asset at spawn time. The same mechanism supports Material, Texture, Curve,
// Static Mesh, and Ribbon-profile assets as long as the Niagara template uses
// a compatible exposed UObject parameter.
USTRUCT(BlueprintType)
struct FWTBRNiagaraAssetParameter
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Parameters")
    FName ParameterName = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Parameters")
    TObjectPtr<UObject> Asset = nullptr;
};

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Parameters")
    TArray<FWTBRNiagaraAssetParameter> ImpactAssetParameters;

    // Enables C++ spawning for this projectile. When false, the existing
    // OnProjectileHitVFX Blueprint event remains the active implementation.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
    bool bUseBuiltInImpactVFX = false;

    // Cosmetic cull distance in cm. Zero keeps the effect visible at all
    // distances; clients beyond this distance do not spawn the impact system.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Performance",
        meta = (ClampMin = "0.0"))
    float MaxImpactVFXDistance = 20000.0f;

    // All fields below are consumed by UWTBRVFXManagerSubsystem. Keeping them
    // in the gameplay asset makes cosmetic feedback consistent across clients
    // without putting Spawn nodes in every projectile Blueprint.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Impact")
    TObjectPtr<USoundBase> ImpactSound = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Impact")
    TObjectPtr<UMaterialInterface> ImpactDecalMaterial = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Impact",
        meta = (ClampMin = "0.0"))
    FVector ImpactDecalSize = FVector(12.0f, 12.0f, 12.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Impact",
        meta = (ClampMin = "0.0"))
    float ImpactDecalLifeSpan = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Impact")
    TSubclassOf<UCameraShakeBase> ImpactCameraShake;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX | Debug")
    bool bDrawImpactDebug = false;
};

// Shared by runtime projectiles and automation tests so surface routing has a
// single, testable implementation.
WTBR_API UNiagaraSystem* WTBRResolveSurfaceImpactEffect(
    UNiagaraSystem* DefaultEffect,
    const TArray<FWTBRSurfaceImpactVFX>& SurfaceOverrides,
    uint8 SurfaceType);
