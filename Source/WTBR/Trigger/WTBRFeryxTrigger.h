// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRMeleeTrigger.h"
#include "WTBRFeryxTrigger.generated.h"

UENUM(BlueprintType)
enum class EWTBRFeryxMode : uint8
{
    ShortBlade UMETA(DisplayName = "Short Blade"),
    Throw UMETA(DisplayName = "Throw")
};

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRFeryxTrigger : public UWTBRMeleeTrigger
{
    GENERATED_BODY()

public:
    // Used by the Feryx+Feryx Mantorn gesture path when a press resolves as a
    // normal tap rather than a completed two-button transform.
    void ResolveTap(bool bIsDualWield);

    virtual bool Activate_Implementation(const FInputActionValue& InputValue, bool bIsDualWield) override;
    virtual void OnTriggerActivated_Implementation(AActor* OwnerActor, bool bIsMain) override;
    virtual void OnTriggerDeactivated_Implementation(AActor* OwnerActor, bool bIsMain) override;
    virtual void OnEquipped() override;

    UFUNCTION(BlueprintPure, Category = "WTBR | Feryx | State")
    EWTBRFeryxMode GetCurrentMode() const { return CurrentMode; }

    UFUNCTION(BlueprintPure, Category = "WTBR | Feryx | State")
    bool IsSpent() const { return bSpent; }

    // Completes a hold-to-select request on the server. bHasSelection=false
    // cancels the pending hold without changing form (wheel dead zone / cancel).
    // Returns true only when a valid, available form was selected.
    bool ResolveFormSelection(EWTBRFeryxMode RequestedMode, bool bHasSelection);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Feryx | VFX")
    void OnFeryxSwing(bool bIsDualWield, bool bDidHit);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Feryx | VFX")
    void OnFeryxHit(const FHitResult& Hit, float DamageDealt);

    // Throw-mode tap: throws one large blade-star (canon: Team Yuma vs Team
    // Ninomiya) and consumes this Feryx until it reconjures. Public so automation
    // can drive the path directly, matching other trigger test patterns.
    void ThrowBladeStars();

protected:
    virtual void PerformSingleSweep(TArray<FHitResult>& OutHits) override;
    virtual void PerformDualSweep(TArray<FHitResult>& OutHits) override;

private:
    void CompleteReconjure();
    static bool IsSupportedMode(EWTBRFeryxMode Mode);

    float HoldStartTime = 0.0f;
    bool bHoldPending = false;
    bool bPendingDualWield = false;
    bool bSpent = false;
    EWTBRFeryxMode CurrentMode = EWTBRFeryxMode::ShortBlade;
    FTimerHandle ReconjureTimer;
};
