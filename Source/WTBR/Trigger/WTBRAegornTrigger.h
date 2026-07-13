// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRDefenseTrigger.h"
#include "WTBRAegornTrigger.generated.h"

class AWTBRAegornWallActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAegornShieldChanged, bool, bIsActive);

// Aegorn = Shield only (canon: movable, transparent, shapeable barrier).
// Wall placement (canon: Escudo — fixed-position, opaque, far more durable)
// lives on UWTBREscudoTrigger as its own equippable Trigger, not a mode of
// this one.
//
// Tap (quick press-release, < HOLD_THRESHOLD) = one-shot shield planted in
// front, same as before.
// Hold (held past HOLD_THRESHOLD) = a carried shield that tracks the
// player's aim every tick until release. If Aegorn is equipped in BOTH Main
// and Sub and BOTH are held simultaneously, the Sub instance automatically
// flips to face the opposite direction — canon Full Guard (front+back
// coverage), no extra input or dialog needed.
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRAegornTrigger : public UWTBRDefenseTrigger
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "WTBR | Aegorn | Events")
    FOnAegornShieldChanged OnShieldChanged;

    UPROPERTY(ReplicatedUsing = OnRep_bShieldActive, BlueprintReadOnly,
        Category = "WTBR | Aegorn | State")
    bool bShieldActive = false;

    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual void OnTriggerActivated_Implementation(
        AActor* OwnerActor, bool bIsMain) override;

    virtual void OnReleased_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    bool CancelShield();

    virtual void InitializeTrigger(
        AWTBRCharacter* InOwnerCharacter,
        UWTBRTriggerDataAsset* InDataAsset) override;

    virtual void Deactivate_Implementation() override;

    UFUNCTION(BlueprintPure, Category = "WTBR | Aegorn | State")
    bool IsShieldActive() const { return bShieldActive; }

    UFUNCTION(BlueprintPure, Category = "WTBR | Aegorn | State")
    bool IsHeldModeActive() const { return bIsHeldMode; }

    UFUNCTION(BlueprintPure, Category = "WTBR | Aegorn | State")
    bool IsOnCooldown() const { return bIsOnCooldown; }

    // bWasHeld: false = tap (instant plant), true = carried/held raise —
    // lets BP distinguish a "snap into place" VFX from a "forming" one.
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Aegorn | VFX")
    void OnAegornShieldRaised(bool bWasHeld);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Aegorn | VFX")
    void OnAegornShieldLowered();

protected:
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_bShieldActive();

private:
    void PerformTap();
    void EnterHeldMode();
    void ExitHeldMode();
    bool IsSiblingAegornHeld() const;
    FVector GetAimDirection() const;

    // Shared spawn path for both tap and hold — consumes Vael, loads
    // ShieldActorClass, spawns + initializes it, binds the destroy callback.
    // Returns nullptr (no Vael spent) on any failure; LogTag distinguishes
    // tap/hold in the validation log.
    AWTBRAegornWallActor* SpawnShieldActor(
        const FVector& Loc, const FRotator& Rot, const TCHAR* LogTag);

    UFUNCTION()
    void NotifyShieldDestroyed(bool bExpiredNaturally);

    UFUNCTION()
    void OnHoldThresholdReached();

    UFUNCTION()
    void TickHeldShield();

    UFUNCTION()
    void OnCooldownExpired();

    void StartCooldown();

    UPROPERTY(EditDefaultsOnly, Category = "WTBR | Aegorn | Shield")
    TSoftClassPtr<AWTBRAegornWallActor> ShieldActorClass;

    UPROPERTY()
    TWeakObjectPtr<AWTBRAegornWallActor> ActiveShieldActor;

    // Cached from OnTriggerActivated(bIsMain) — lets a held shield know
    // whether it's the Main or Sub instance for the Full Guard back-flip.
    bool bIsMainSlot = true;
    bool bWaitingForHoldDecision = false;
    bool bIsHeldMode = false;
    bool bIsOnCooldown = false;

    // Set right before CancelShield's self-damage collapse so
    // NotifyShieldDestroyed can tell "player chose to end this" (normal
    // hold release, slot switch, explicit cancel — no cooldown) apart from
    // "an enemy actually broke it" (bExpiredNaturally=false with this still
    // clear — cooldown applies).
    bool bPendingManualCancel = false;

    FTimerHandle HoldThresholdTimer;
    FTimerHandle HoldTrackingTimer;
    FTimerHandle CooldownTimer;

    static constexpr float HOLD_THRESHOLD = 0.2f;
    static constexpr float HOLD_TRACK_INTERVAL = 0.05f;
    static constexpr float SPAWN_FORWARD_OFFSET = 150.0f;
};
