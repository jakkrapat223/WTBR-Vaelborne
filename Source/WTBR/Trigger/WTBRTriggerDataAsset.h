// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

class USkeletalMesh;
class UAnimMontage;

#include "WTBRTriggerDataAsset.generated.h"


UENUM(BlueprintType)
enum class ETriggerCategory : uint8
{
    None         UMETA(DisplayName = "None"),
    Melee        UMETA(DisplayName = "Melee"),


    Gunner       UMETA(DisplayName = "Gunner"),


    SniperBullet UMETA(DisplayName = "Sniper Bullet"),


    Defense      UMETA(DisplayName = "Defense"),


    Movement     UMETA(DisplayName = "Movement"),


    Trap         UMETA(DisplayName = "Trap"),


    BlackTrigger UMETA(DisplayName = "Black Trigger"),

};

UENUM(BlueprintType)
enum class ETriggerSlotConstraint : uint8
{
    MainOnly UMETA(DisplayName="Main Slot Only"),
    SubOnly  UMETA(DisplayName="Sub Slot Only"),
    Any      UMETA(DisplayName="Any Slot"),
};

USTRUCT(BlueprintType)
struct FWTBRDualWieldStats
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dual Wield", meta=(ClampMin="0.1", ClampMax="5.0"))
    float DamageMultiplier = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dual Wield", meta=(ClampMin="0.1", ClampMax="1.0"))
    float SpeedMultiplier = 1.0f;

    // Multiplicative with StaminaCostPerUse when dual wielding (GDD §3.4 Lock: 1.5x)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dual Wield", meta=(ClampMin="1.0", ClampMax="3.0"))
    float StaminaCostMultiplier = 1.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dual Wield")
    bool bHasDualWieldMontage = false;
};

UCLASS(BlueprintType)
class WTBR_API UWTBRTriggerDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ─── Identity ────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Identity")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Identity")
    ETriggerCategory Category = ETriggerCategory::None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Identity")
    ETriggerSlotConstraint SlotConstraint = ETriggerSlotConstraint::Any;

    // ─── Resources ───────────────────────────────────────────────────────────

    // Vael cost per activation — validated server-side before any effect (⚠ Playtest)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Resources", meta=(ClampMin="0"))
    float VaelCostPerUse = 10.0f;

    // Stamina cost per activation — base before dual-wield multiplier (⚠ Playtest)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Resources", meta=(ClampMin="0"))
    float StaminaCostPerUse = 0.0f;

    // ─── Combat ──────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Combat", meta=(ClampMin="0"))
    float BaseDamage = 0.0f;

    // [0,1] — fraction of damage that bypasses Defense triggers (GDD §2.5)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Combat", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ShieldPenetrationFactor = 0.0f;

    // ─── Dual Wield ──────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Dual Wield")
    bool bSupportsDualWield = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Dual Wield", meta=(EditCondition="bSupportsDualWield"))
    FWTBRDualWieldStats DualWieldStats;

    // ─── Action Ping ─────────────────────────────────────────────────────────

    // True when Vael exits the character capsule on activation → radar ping (GDD §3.5)
    // False for melee, shield gank, etc.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Action Ping")
    bool bDispatchesActionPing = false;

    // ─── Visuals ─────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Visuals")
    TSoftObjectPtr<USkeletalMesh> TriggerMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Visuals")
    TSoftClassPtr<AActor> ProjectileClass;

    // ─── Animation ───────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Animation")
    TSoftObjectPtr<UAnimMontage> ActivationMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Animation", meta=(EditCondition="bSupportsDualWield"))
    TSoftObjectPtr<UAnimMontage> DualWieldActivationMontage;

    // ─── Asset Manager ───────────────────────────────────────────────────────

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId("WTBRTrigger", GetFName());
    }
};
