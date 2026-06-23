// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRMeleeTrigger.h"
#include "WTBRFeryxTrigger.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRFeryxTrigger : public UWTBRMeleeTrigger
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Feryx | VFX")
    void OnFeryxSwing(bool bIsDualWield, bool bDidHit);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Feryx | VFX")
    void OnFeryxHit(const FHitResult& Hit, float DamageDealt);

protected:
    virtual void PerformSingleSweep(TArray<FHitResult>& OutHits) override;
    virtual void PerformDualSweep(TArray<FHitResult>& OutHits) override;
};
