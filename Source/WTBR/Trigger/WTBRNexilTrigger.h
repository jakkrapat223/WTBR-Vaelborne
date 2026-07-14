// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRNexilTrigger.generated.h"

class AWTBRNexilWireActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNexilWirePlaced);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnNexilWireTriggered, AWTBRNexilWireActor*, TriggeredWire);

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRNexilTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "WTBR | Nexil | Events")
    FOnNexilWirePlaced OnWirePlaced;

    UPROPERTY(BlueprintAssignable, Category = "WTBR | Nexil | Events")
    FOnNexilWireTriggered OnWireTriggered;

    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION()
    void NotifyWireTriggered(AWTBRNexilWireActor* TriggeredWire);

    UFUNCTION()
    void NotifyWireDestroyed(AActor* DestroyedActor);

    UFUNCTION(BlueprintPure, Category = "WTBR | Nexil | State")
    int32 GetActiveWireCount() const;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Nexil | VFX")
    void OnNexilWirePlaced(AWTBRNexilWireActor* WireActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Nexil | VFX")
    void OnNexilWireTriggeredVFX(AWTBRNexilWireActor* WireActor);

    virtual void InitializeTrigger(
        AWTBRCharacter* InOwnerCharacter,
        UWTBRTriggerDataAsset* InDataAsset) override;

    virtual void Deactivate_Implementation() override;

#if WITH_DEV_AUTOMATION_TESTS
    // Test-only: inject the wire actor class (normally set on BP_WTBRNexilTrigger)
    // so headless tests can exercise placement/FIFO/cleanup without the Blueprint.
    void SetWireActorClassForTest(TSoftClassPtr<AWTBRNexilWireActor> InClass) { WireActorClass = InClass; }
#endif

private:
    // TD Fix 1: TWeakObjectPtr prevents crash when Wire destroyed early
    TArray<TWeakObjectPtr<AWTBRNexilWireActor>> ActiveWires;

    UPROPERTY(EditDefaultsOnly, Category = "WTBR | Nexil | Wire")
    TSoftClassPtr<AWTBRNexilWireActor> WireActorClass;

    void PlaceWire();
    void RemoveOldestWire();
};
