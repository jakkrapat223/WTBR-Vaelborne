// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WTBRActionPingSubsystem.generated.h"

// Receives short-lived action markers from VaelComponent::NotifyVaelLeftCharacterBounds.
// These markers are presentation/intelligence events; they never add, remove, or
// suppress normal Radar contacts. Canon radar sees every non-cloaked trion body.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionPing, AActor*, Source);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnActionPingNative, AActor*);

UCLASS()
class WTBR_API UWTBRActionPingSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnActionPing OnActionPing;

    // Native mirror for C++ consumers and automation tests. Keep the dynamic
    // delegate above for Blueprint marker presentation.
    FOnActionPingNative OnActionPingNative;

    UFUNCTION(BlueprintCallable, Category="ActionPing")
    void RegisterActionPing(AActor* Source);

protected:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
};
