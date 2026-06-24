// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WTBRActionPingSubsystem.generated.h"

// Receives pings from VaelComponent::NotifyVaelLeftCharacterBounds.
// Radar UI subscribes to OnActionPing to show directional indicators.
// Rule: only fires when Vael energy physically exits the character capsule (GDD §3.2 Lock).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionPing, AActor*, Source);

UCLASS()
class WTBR_API UWTBRActionPingSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnActionPing OnActionPing;

    UFUNCTION(BlueprintCallable, Category="ActionPing")
    void RegisterActionPing(AActor* Source);

    // Vexorn (Signal Block) — server only
    void RegisterSignalBlock(AActor* Actor);
    void UnregisterSignalBlock(AActor* Actor);
    bool IsSignalBlocked(AActor* Actor) const;

protected:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

private:
    // No UPROPERTY — TWeakObjectPtr prevents GC cycles without UObject bookkeeping
    TSet<TWeakObjectPtr<AActor>> BlockedActors;
};
