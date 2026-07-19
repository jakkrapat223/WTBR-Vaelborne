// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRMovementTrigger.h"
#include "WTBRVoltisLaunchTrigger.generated.h"

class AWTBRVoltisTrapActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoltisLaunch, bool, bIsDualWield);
// bWasHardLanding param removed with the ceiling-obstruction check it reported
// (blocked ANY Voltis dash toward a nearby wall or enemy Pawn — WorldStatic
// trace channel blocks Pawns by default — not just an actual low ceiling).
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVoltisLand);

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRVoltisLaunchTrigger : public UWTBRMovementTrigger
{
    GENERATED_BODY()

public:
    UWTBRVoltisLaunchTrigger();

    UPROPERTY(BlueprintAssignable, Category = "WTBR | Voltis | Events")
    FOnVoltisLaunch OnVoltisLaunch;

    UPROPERTY(BlueprintAssignable, Category = "WTBR | Voltis | Events")
    FOnVoltisLand OnVoltisLand;

    UPROPERTY(ReplicatedUsing = OnRep_bIsStaggered, BlueprintReadOnly,
        Category = "WTBR | Voltis | State")
    bool bIsStaggered = false;

    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual void OnReleased_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual void Deactivate_Implementation() override;

    UFUNCTION()
    void OnCharacterLanded(const FHitResult& Hit);

    UFUNCTION(BlueprintPure, Category = "WTBR | Voltis | State")
    bool IsStaggered() const { return bIsStaggered; }

    UFUNCTION(BlueprintPure, Category = "WTBR | Voltis | State")
    int32 GetRemainingLaunches() const { return RemainingLaunches; }

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Voltis | VFX")
    void OnVoltisLaunched(bool bIsDualWield);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Voltis | VFX")
    void OnVoltisHardLanding();

    // Hold — teammate direct-apply (no object spawned).
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Voltis | VFX")
    void OnVoltisFriendlyApplied(AWTBRCharacter* Teammate);

    // Hold — ground/environment Trap placed.
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Voltis | VFX")
    void OnVoltisTrapPlaced(AWTBRVoltisTrapActor* Trap);

    virtual void InitializeTrigger(
        AWTBRCharacter* InOwnerCharacter,
        UWTBRTriggerDataAsset* InDataAsset) override;

    // Called by AWTBRVoltisTrapActor when it triggers or expires, so this Trigger's
    // FIFO-capped ActiveTraps bookkeeping stays accurate (mirrors UWTBRNexilTrigger's
    // NotifyWireTriggered/NotifyWireDestroyed pattern).
    UFUNCTION()
    void NotifyTrapConsumed(AWTBRVoltisTrapActor* ConsumedTrap);

    UFUNCTION(BlueprintPure, Category = "WTBR | Voltis | State")
    int32 GetActiveTrapCount() const;

#if WITH_DEV_AUTOMATION_TESTS
    // Test-only: inject the trap actor class (the constructor already defaults this to
    // the native AWTBRVoltisTrapActor::StaticClass(), but a BP subclass of this Trigger
    // can still override it) so headless tests can exercise Hold placement/FIFO/cleanup
    // with an explicit, test-controlled class regardless of that default.
    void SetTrapActorClassForTest(TSoftClassPtr<AWTBRVoltisTrapActor> InClass) { TrapActorClass = InClass; }
    void OnHoldThresholdReachedForTest() { OnHoldThresholdReached(); }
#endif

protected:
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_bIsStaggered();

private:
    void PerformLaunch(bool bIsDualWield, const FVector& LaunchDir);
    void StartStagger();

    UFUNCTION()
    void OnStaggerExpired();

    // Tap (dash) — existing behavior, now gated behind Vael like every other Trigger
    // instead of unconditionally free. Only runs if released before HOLD_THRESHOLD.
    void PerformTap(const FInputActionValue& InputValue, bool bIsDualWield);

    UFUNCTION()
    void OnHoldThresholdReached();

    // Branches on what's aimed at: teammate in range -> direct apply; otherwise ->
    // aim-trace + surface-snap-validated Trap placement. Resolves once, immediately,
    // at the hold threshold — no continuous tracking like Aegorn's held shield.
    void ResolveHold();
    void PerformDirectApply(AWTBRCharacter* Teammate);
    void PerformTrapPlacement();
    void RemoveOldestTrap();

    bool bWaitingForHoldDecision = false;
    FTimerHandle HoldThresholdTimer;

    // Captured at PRESS time (Activate_Implementation) and used by PerformTap instead
    // of OnReleased_Implementation's own InputValue. AWTBRCharacter::ExecuteServerTriggerInput
    // sends a FRESH ClientMoveInputDir snapshot on every press AND every release RPC
    // separately — reading OnReleased's copy for tap direction meant a fast tap used
    // whatever (often near-zero) direction the client happened to be holding at the
    // release instant, not the press instant, which then fell through to the Velocity2D
    // fallback and reused the PREVIOUS dash's leftover momentum direction instead of the
    // new press's intended direction. Real bug found via the owner's own testing.
    FInputActionValue CachedPressInputValue;

    UPROPERTY(EditDefaultsOnly, Category = "WTBR | Voltis | Trap")
    TSoftClassPtr<AWTBRVoltisTrapActor> TrapActorClass;

    UPROPERTY()
    TArray<TWeakObjectPtr<AWTBRVoltisTrapActor>> ActiveTraps;

    int32 RemainingLaunches = 0;
    FTimerHandle StaggerTimer;

    // ⚠ PLAYTEST PENDING — จะย้ายไป DataAsset ใน Phase 5
    static constexpr float STAGGER_DURATION = 0.4f;
    static constexpr float HOLD_THRESHOLD = 0.2f;
};
