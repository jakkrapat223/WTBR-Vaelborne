// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Trigger/WTBRCompositeBehaviorBase.h"
#include "Trigger/WTBRTriggerDataAsset.h"

#include "WTBRCompositeRegistryDataAsset.generated.h"

class AWTBRProjectileBase;

UENUM(BlueprintType)
enum class EWTBRCompositeMovementInterruptPolicy : uint8
{
    None UMETA(DisplayName = "None"),
    RootTranslationCameraAimFree UMETA(DisplayName = "Root Translation, Camera/Aim Free"),
};

/** Generic homing settings for a composite definition. All values are PLAYTEST PENDING. */
USTRUCT(BlueprintType)
struct FWTBRCompositeHomingParams
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING: disabled by default until a definition opts in.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Homing")
    bool bEnableHoming = false;

    // ⚠ PLAYTEST PENDING: zero means no target acquisition range is configured.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Homing", meta = (ClampMin = "0.0"))
    float TargetAcquisitionRange = 0.0f;

    // ⚠ PLAYTEST PENDING: zero means no homing acceleration is configured.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Homing", meta = (ClampMin = "0.0"))
    float HomingAcceleration = 0.0f;

    // ⚠ PLAYTEST PENDING: zero means no maximum homing speed is configured.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Homing", meta = (ClampMin = "0.0"))
    float MaximumHomingSpeed = 0.0f;
};

/** Generic explosion settings for a composite definition. All values are PLAYTEST PENDING. */
USTRUCT(BlueprintType)
struct FWTBRCompositeExplosionParams
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING: disabled by default until a definition opts in.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion")
    bool bExplodes = false;

    // ⚠ PLAYTEST PENDING: zero means no explosion radius is configured.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion", meta = (ClampMin = "0.0"))
    float ExplosionRadius = 0.0f;

    // ⚠ PLAYTEST PENDING: neutral falloff shape for a future configured explosion.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion", meta = (ClampMin = "0.0"))
    float DamageFalloffExponent = 1.0f;
};

/** One DataAsset-authored composite recipe. Values are intentionally unbalanced placeholders. */
USTRUCT(BlueprintType)
struct FWTBRCompositeDefinition
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING: identifies the composite produced by this definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Definition")
    EWTBRCompositeBulletType CompositeType = EWTBRCompositeBulletType::None;

    // ⚠ PLAYTEST PENDING: the first source archetype is authored per definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Definition")
    EWTBRBulletArchetype RequiredArchetypeA = EWTBRBulletArchetype::None;

    // ⚠ PLAYTEST PENDING: the second source archetype is authored per definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Definition")
    EWTBRBulletArchetype RequiredArchetypeB = EWTBRBulletArchetype::None;

    // ⚠ PLAYTEST PENDING: no projectile is assigned until an owner authors a registry asset.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Definition")
    TSubclassOf<AWTBRProjectileBase> ProjectileClass = nullptr;

    // ⚠ PLAYTEST PENDING: selected concrete behavior is authored by the registry asset.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Definition")
    TSubclassOf<UWTBRCompositeBehaviorBase> BehaviorClass = nullptr;

    // ⚠ PLAYTEST PENDING: balance is authored per composite definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Balance", meta = (ClampMin = "0.0"))
    float TotalDamageBudget = 0.0f;

    // ⚠ PLAYTEST PENDING: balance is authored per composite definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Balance", meta = (ClampMin = "0.0"))
    float VaelCost = 0.0f;

    // ⚠ PLAYTEST PENDING: balance is authored per composite definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Timing", meta = (ClampMin = "0.0"))
    float MergeTime = 0.0f;

    // ⚠ PLAYTEST PENDING: balance is authored per composite definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Timing", meta = (ClampMin = "0.0"))
    float CompositeCooldown = 0.0f;

    // ⚠ PLAYTEST PENDING: no movement restriction is applied until a definition opts in.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Movement")
    EWTBRCompositeMovementInterruptPolicy MovementInterruptPolicy =
        EWTBRCompositeMovementInterruptPolicy::None;

    // PathPreset is intentionally omitted until Step 14 creates FWTBRPathPreset/FWTBRPathLane.

    // ⚠ PLAYTEST PENDING: generic homing values are authored per definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Homing")
    FWTBRCompositeHomingParams HomingParams;

    // ⚠ PLAYTEST PENDING: generic explosion values are authored per definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion")
    FWTBRCompositeExplosionParams ExplosionParams;
};

/** Central, DataAsset-authored source of composite definitions. */
UCLASS(BlueprintType)
class WTBR_API UWTBRCompositeRegistryDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Empty by default; real definitions are authored after the vertical slice validates this shape.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Registry")
    TArray<FWTBRCompositeDefinition> Definitions;

    /** Resolves an unordered pair, or None when the pair is invalid or unregistered. */
    UFUNCTION(BlueprintPure, Category = "Composite | Registry")
    EWTBRCompositeBulletType ResolveCompositeType(
        EWTBRBulletArchetype ArchetypeA,
        EWTBRBulletArchetype ArchetypeB) const;

    /** Finds the full definition for an already-resolved Type. False if Type is None or unregistered. */
    UFUNCTION(BlueprintPure, Category = "Composite | Registry")
    bool FindDefinition(EWTBRCompositeBulletType Type, FWTBRCompositeDefinition& OutDefinition) const;
};
