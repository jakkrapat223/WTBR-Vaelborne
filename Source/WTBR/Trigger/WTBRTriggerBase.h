// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "WTBRTriggerBase.generated.h"

class AWTBRCharacter;

UCLASS(Abstract, BlueprintType, Blueprintable)
class WTBR_API UWTBRTriggerBase : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trigger")
    TObjectPtr<UWTBRTriggerDataAsset> DataAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trigger")
    ETriggerCategory Category = ETriggerCategory::Melee;

    // Called by TriggerSetComponent after spawn to bind owner and data.
    virtual void InitializeTrigger(AWTBRCharacter* InOwnerCharacter, UWTBRTriggerDataAsset* InDataAsset);

    // Main activation entry point — called by TriggerSetComponent on button press.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Trigger")
    bool Activate(const FInputActionValue& InputValue, bool bIsDualWield);
    virtual bool Activate_Implementation(const FInputActionValue& InputValue, bool bIsDualWield);

    // Called by TriggerSetComponent when LMB/RMB is pressed (bIsMain=true→LMB, false→RMB)
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Trigger")
    void OnTriggerActivated(AActor* OwnerActor, bool bIsMain);
    virtual void OnTriggerActivated_Implementation(AActor* OwnerActor, bool bIsMain);

    // Called on button release
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Trigger")
    void OnTriggerDeactivated(AActor* OwnerActor, bool bIsMain);
    virtual void OnTriggerDeactivated_Implementation(AActor* OwnerActor, bool bIsMain);

    // Called when Vael energy physically leaves the character hitbox.
    // Triggers EVaelReleaseEvent dispatch to UWTBRActionPingSubsystem.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Trigger")
    void OnVaelLeftCharacterBounds(AActor* OwnerActor);
    virtual void OnVaelLeftCharacterBounds_Implementation(AActor* OwnerActor);

    // Called by TriggerSetComponent on button release (hold-style triggers like Aegorn)
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Trigger")
    void OnReleased(const FInputActionValue& InputValue, bool bIsDualWield);
    virtual void OnReleased_Implementation(const FInputActionValue& InputValue, bool bIsDualWield);

    // Called when trigger is force-deactivated (death, stun, loadout swap)
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Trigger")
    void Deactivate();
    virtual void Deactivate_Implementation();

    // Called by TriggerSetComponent when this trigger becomes the active Sub-Trigger
    virtual void OnEquipped() {}

    // Called by TriggerSetComponent when this trigger is replaced as the active Sub-Trigger
    virtual void OnUnequipped() {}

protected:
    // Set by TriggerSetComponent after spawn — weak to avoid cycle.
    UPROPERTY()
    TWeakObjectPtr<AWTBRCharacter> OwnerCharacter;
};
