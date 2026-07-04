// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/WTBRHUDViewTypes.h"
#include "WTBRHUDViewModelComponent.generated.h"

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

private:
    FWTBRHUDSnapshot CachedSnapshot;

    FWTBRHUDSnapshot BuildDefaultSnapshot() const;
};
