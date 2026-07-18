// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VFX/WTBRProjectileVFXTypes.h"
#include "WTBRVFXManagerSubsystem.generated.h"

class UCameraShakeBase;
class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;

// Runtime quality policy derived from the user's Effects scalability setting.
// The primary impact remains visible; optional feedback is trimmed first.
USTRUCT(BlueprintType)
struct FWTBRVFXQualityProfile
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WTBR | VFX | Quality")
    float DistanceMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WTBR | VFX | Quality")
    bool bSpawnDecals = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WTBR | VFX | Quality")
    bool bPlayCameraShake = true;
};

// A complete cosmetic request emitted by replicated gameplay. It is processed
// locally on every client, never on a dedicated server.
USTRUCT(BlueprintType)
struct FWTBRImpactVFXRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | VFX")
    TObjectPtr<UNiagaraSystem> Effect = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | VFX")
    TObjectPtr<USoundBase> Sound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | VFX")
    TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | VFX")
    TArray<FWTBRNiagaraAssetParameter> AssetParameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | VFX")
    TSubclassOf<UCameraShakeBase> CameraShake;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | VFX")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | VFX")
    FVector Normal = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | VFX")
    FVector DecalSize = FVector(12.0f, 12.0f, 12.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | VFX",
        meta = (ClampMin = "0.0"))
    float DecalLifeSpan = 8.0f;

    // Zero means no distance culling. This is evaluated against each local
    // player's current view target, so multiplayer cosmetics remain local.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | VFX",
        meta = (ClampMin = "0.0"))
    float MaxDistance = 20000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | VFX")
    uint8 SurfaceType = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | VFX | Debug")
    bool bDrawDebug = false;
};

// Centralizes pooled Niagara, sound, decals, local camera shake, distance
// culling, and opt-in debug drawing for replicated gameplay VFX.
UCLASS()
class WTBR_API UWTBRVFXManagerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:

    // Maps Unreal's sg.EffectsQuality (0..3) to the project's cosmetic policy.
    // This overload is deterministic so it can be used by automation and UI.
    UFUNCTION(BlueprintPure, Category = "WTBR | VFX | Quality")
    static FWTBRVFXQualityProfile GetQualityProfileForEffectsQuality(int32 EffectsQuality);

    UFUNCTION(BlueprintPure, Category = "WTBR | VFX | Quality")
    static FWTBRVFXQualityProfile GetActiveQualityProfile();

    UFUNCTION(BlueprintCallable, Category = "WTBR | VFX")
    bool SpawnImpact(const FWTBRImpactVFXRequest& Request);

    UFUNCTION(BlueprintPure, Category = "WTBR | VFX")
    bool IsRequestVisibleToLocalPlayer(const FWTBRImpactVFXRequest& Request) const;
};
