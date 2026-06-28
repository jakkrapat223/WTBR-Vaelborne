// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRDefenseTrigger.h"
#include "WTBRAegornTrigger.generated.h"

class UBoxComponent;
class AWTBRAegornWallActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAegornShieldChanged, bool, bIsActive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAegornWallPlaced);

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRAegornTrigger : public UWTBRDefenseTrigger
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "WTBR | Aegorn | Events")
    FOnAegornShieldChanged OnShieldChanged;

    UPROPERTY(BlueprintAssignable, Category = "WTBR | Aegorn | Events")
    FOnAegornWallPlaced OnWallPlaced;

    UPROPERTY(ReplicatedUsing = OnRep_bShieldActive, BlueprintReadOnly,
        Category = "WTBR | Aegorn | State")
    bool bShieldActive = false;

    UPROPERTY(Replicated, BlueprintReadOnly,
        Category = "WTBR | Aegorn | State")
    bool bIsDualShield = false;

    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual void OnReleased_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintCallable, Category = "WTBR | Aegorn | Wall")
    void PlaceWall();

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

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Aegorn | VFX")
    void OnAegornWallSpawned(AWTBRAegornWallActor* WallActor);

protected:
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_bShieldActive();

private:
    void RaiseShield(bool bDualWield);
    void LowerShield();
    void UpdateShieldCollisionSize(bool bDualWield);
    void StartVaelDrain();
    void StopVaelDrain();

    UFUNCTION()
    void TickVaelDrain();

    UPROPERTY()
    TObjectPtr<UBoxComponent> ShieldCollisionBox;

    UPROPERTY(EditDefaultsOnly, Category = "WTBR | Aegorn | Wall")
    TSoftClassPtr<AWTBRAegornWallActor> WallActorClass;

    FTimerHandle VaelDrainTimer;
    float VaelDrainPerTick = 0.0f;

    static constexpr float DRAIN_INTERVAL = 0.1f;
};
