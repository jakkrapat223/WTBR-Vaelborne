// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRGameState.h"
#include "UI/WTBRHUDViewTypes.h"
#include "WTBRHUDViewModelComponent.generated.h"

class AWTBRCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWTBRHUDSnapshotChanged);

UCLASS(ClassGroup=(WTBR), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRHUDViewModelComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRHUDViewModelComponent();

    UPROPERTY(BlueprintAssignable, Category="WTBR | HUD")
    FWTBRHUDSnapshotChanged OnHUDSnapshotChanged;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FWTBRHUDSnapshot GetHUDSnapshot() const;

    UFUNCTION(BlueprintCallable, Category="WTBR | HUD")
    void RefreshHUDSnapshot();

    UFUNCTION(BlueprintCallable, Category="WTBR | HUD | Requests")
    bool RequestUseDisplayedQuickItem();

    UFUNCTION(BlueprintCallable, Category="WTBR | HUD | Requests")
    bool RequestPickupFocusedTarget();

    UFUNCTION(BlueprintCallable, Category="WTBR | HUD | Requests")
    bool RequestCancelCurrentUIOrAction();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FWTBRHUDSnapshot CachedSnapshot;
    int32 CachedQuickItemSlotIndex = INDEX_NONE;

    FWTBRHUDSnapshot BuildDefaultSnapshot() const;
    FWTBRHUDSnapshot BuildLiveSnapshot();
    FWTBRHUDVitalsSnapshot BuildVitalsSnapshot(const AWTBRCharacter* Character) const;
    FWTBRHUDTriggerCardSnapshot BuildTriggerCardSnapshot(const AWTBRCharacter* Character, bool bIsMain) const;
    FWTBRHUDQuickItemSnapshot BuildQuickItemSnapshot(const AWTBRCharacter* Character, int32& OutSlotIndex) const;
    FWTBRHUDPickupPromptSnapshot BuildPickupPromptSnapshot(const AWTBRCharacter* Character) const;
    FWTBRHUDMatchSnapshot BuildMatchSnapshot() const;
    static int32 ResolveQuickItemSlotIndex(const AWTBRCharacter* Character);
    void BindOwnerDelegates();
    void UnbindOwnerDelegates();

    UFUNCTION()
    void OnHPChangedHandler(float NewHP, float MaxHP);

    UFUNCTION()
    void OnVaelChangedHandler(float NewVael, float MaxVael);

    UFUNCTION()
    void OnInventoryChangedHandler();

    UFUNCTION()
    void OnActiveTriggerChangedHandler(ETriggerSlot NewSlot);

    UFUNCTION()
    void OnMatchPhaseChangedHandler(EWTBRMatchPhase MatchPhase);
};
