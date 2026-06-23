// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "WTBRTriggerBase.generated.h"

UCLASS(Abstract, BlueprintType, Blueprintable)
class WTBR_API UWTBRTriggerBase : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trigger")
    TObjectPtr<UWTBRTriggerDataAsset> TriggerData;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trigger")
    ETriggerCategory Category = ETriggerCategory::Melee;

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
};
