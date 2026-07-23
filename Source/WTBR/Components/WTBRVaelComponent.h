// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "Misc/Guid.h"
#include "WTBRVaelComponent.generated.h"

// EVaelReleaseEvent — fired when an action releases Vael beyond the character capsule.
// This is a short-lived action MARKER only; it never controls baseline Radar visibility.
// Canon radar sees every non-Bagworm trion body, including an agent merely holding Kogetsu.
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

    // Broadcast an action marker when Vael energy leaves the character capsule.
    // Radar contact visibility is instead owned by AWTBRCharacter::bRadarCloaked.
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
    bool TryReserveVael(float Amount, FGuid& OutReservationHandle);

    UFUNCTION(BlueprintCallable, Category="Vael")
    bool CommitReservation(const FGuid& ReservationHandle);

    UFUNCTION(BlueprintCallable, Category="Vael")
    bool ReleaseReservation(const FGuid& ReservationHandle);

    UFUNCTION(BlueprintPure, Category="Vael")
    float GetAvailableVael() const;

    UFUNCTION(BlueprintCallable, Category="Vael")
    void ResetDesperationState();

    UFUNCTION(BlueprintCallable, Category="WTBR | Debug")
    void DebugSetCurrentVaelDirect(float NewVael);

    // Call this from a Trigger/Projectile the moment the Vael actor exits the capsule
    // and should create an action marker. Do not call it for equipping, holding, or
    // ordinary Kogetsu/Lacern swings.
    UFUNCTION(BlueprintCallable, Category="Vael")
    void NotifyVaelLeftCharacterBounds();

    UFUNCTION(BlueprintPure, Category="Vael")
    float GetCurrentVael() const { return CurrentVael; }

    UFUNCTION(BlueprintPure, Category="Vael")
    float GetMaxVael() const;

    UFUNCTION(BlueprintPure, Category="Vael")
    bool IsOverheated() const { return bOverheated; }

    UFUNCTION(BlueprintPure, Category="Vael")
    bool IsDesperationActive() const { return bIsDesperationActive; }

    UFUNCTION(BlueprintPure, Category="Vael")
    bool IsDesperationOnCooldown() const { return bIsDesperationOnCooldown; }

    UFUNCTION(BlueprintPure, Category="Vael")
    float GetVaelCostMultiplier() const;

    UFUNCTION(BlueprintPure, Category="Vael")
    float GetLowVaelThreshold() const;

    UFUNCTION(BlueprintPure, Category="WTBR | Debug")
    bool DebugIsDesperationActiveTimerActive() const;

    UFUNCTION(BlueprintPure, Category="WTBR | Debug")
    bool DebugIsDesperationCooldownTimerActive() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TMap<FGuid, float> ActiveVaelReservations;
    float GetTotalReservedVael() const;

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
    FTimerHandle PassiveRegenTimerHandle;

    const UWTBRCoreStatsDataAsset* GetStats() const
    {
        if (CoreStatsAsset.IsNull())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Vael CoreStatsCheck] Owner=%s | Component=%s | CoreStatsAsset=None | CoreStatsPath=None | Loaded=false"),
                *GetNameSafe(GetOwner()),
                *GetNameSafe(this));
            return nullptr;
        }

        return CoreStatsAsset.LoadSynchronous();
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
    void StartPassiveRegenTimer();
    void StopPassiveRegenTimer();
    void ApplyPassiveRegenTick();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
