// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "WTBRVaelComponent.generated.h"

// EVaelReleaseEvent — fired when Vael energy physically leaves the character capsule.
// Drives the Action Ping system on the radar.  Rule: ฟัน/กาง = no ping; projectile/wave = ping.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVaelReleased);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVaelChanged, float, NewVael, float, MaxVael);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOverheatChanged, bool, bIsOverheated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDesperationActiveChanged, bool, bIsActive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDesperationCooldownChanged, bool, bIsOnCooldown);

UCLASS(ClassGroup=(WTBR), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRVaelComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRVaelComponent();

    // ── DataAsset Reference ──────────────────────────────────────────────────
    // Single source of truth for all tunable stats.
    // Set this reference in BP_WTBRCharacter component defaults (Details panel).
    // All gameplay values are read from this asset at BeginPlay — never hardcoded.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Config")
    TSoftObjectPtr<UWTBRCoreStatsDataAsset> CoreStatsAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vael")
    float MaxVael = 1000.f;

    // Broadcast when Vael energy leaves the character capsule → radar gets a ping
    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnVaelReleased OnVaelReleased;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnVaelChanged OnVaelChanged;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnOverheatChanged OnOverheatChanged;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnDesperationActiveChanged OnDesperationActiveChanged;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnDesperationCooldownChanged OnDesperationCooldownChanged;

    UFUNCTION(BlueprintCallable, Category="Vael")
    bool TryConsumeVael(float Amount);

    UFUNCTION(BlueprintCallable, Category="Vael")
    bool GrantVael(float Amount);

    UFUNCTION(BlueprintCallable, Category="Vael")
    void ResetDesperationState();

    // Call this from a Trigger/Projectile the moment the Vael actor exits the capsule
    UFUNCTION(BlueprintCallable, Category="Vael")
    void NotifyVaelLeftCharacterBounds();

    UFUNCTION(BlueprintPure, Category="Vael")
    float GetCurrentVael() const { return CurrentVael; }

    UFUNCTION(BlueprintPure, Category="Vael")
    bool IsOverheated() const { return bOverheated; }

    UFUNCTION(BlueprintPure, Category="Vael")
    bool IsDesperationActive() const { return bIsDesperationActive; }

    UFUNCTION(BlueprintPure, Category="Vael")
    bool IsDesperationOnCooldown() const { return bIsDesperationOnCooldown; }

    UFUNCTION(BlueprintPure, Category="Vael")
    float GetVaelCostMultiplier() const;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(ReplicatedUsing=OnRep_CurrentVael)
    float CurrentVael;

    UPROPERTY(ReplicatedUsing=OnRep_bOverheated)
    bool bOverheated = false;

    UPROPERTY(ReplicatedUsing=OnRep_IsDesperationActive)
    bool bIsDesperationActive = false;

    UPROPERTY(ReplicatedUsing=OnRep_IsDesperationOnCooldown)
    bool bIsDesperationOnCooldown = false;

    FTimerHandle DesperationActiveTimerHandle;
    FTimerHandle DesperationCooldownTimerHandle;

    const UWTBRCoreStatsDataAsset* GetStats() const
    {
        ensure(CoreStatsAsset.IsValid());
        return CoreStatsAsset.Get();
    }

    UFUNCTION()
    void OnRep_CurrentVael();

    UFUNCTION()
    void OnRep_bOverheated();

    UFUNCTION()
    void OnRep_IsDesperationActive();

    UFUNCTION()
    void OnRep_IsDesperationOnCooldown();

    void HandleVaelChanged(float OldVael, float NewVael);
    void TriggerDesperationWindow();
    void EndDesperationWindow();
    void EndDesperationCooldown();
    void SetDesperationActive(bool bNewActive);
    void SetDesperationOnCooldown(bool bNewOnCooldown);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
