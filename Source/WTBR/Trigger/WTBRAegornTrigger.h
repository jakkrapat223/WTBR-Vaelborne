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

    bool CancelShield();

    virtual void InitializeTrigger(
        AWTBRCharacter* InOwnerCharacter,
        UWTBRTriggerDataAsset* InDataAsset) override;

    virtual void Deactivate_Implementation() override;

    UFUNCTION(BlueprintPure, Category = "WTBR | Aegorn | State")
    bool IsShieldActive() const { return bShieldActive; }

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Aegorn | VFX")
    void OnAegornShieldRaised(bool bDualWield);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Aegorn | VFX")
    void OnAegornShieldLowered();

protected:
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_bShieldActive();

private:
    UFUNCTION()
    void NotifyShieldDestroyed();

    UPROPERTY(EditDefaultsOnly, Category = "WTBR | Aegorn | Shield")
    TSoftClassPtr<AWTBRAegornWallActor> ShieldActorClass;

    UPROPERTY()
    TWeakObjectPtr<AWTBRAegornWallActor> ActiveShieldActor;
};
