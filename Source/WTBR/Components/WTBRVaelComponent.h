// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WTBRVaelComponent.generated.h"

// EVaelReleaseEvent — fired when Vael energy physically leaves the character capsule.
// Drives the Action Ping system on the radar.  Rule: ฟัน/กาง = no ping; projectile/wave = ping.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVaelReleased);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVaelChanged, float, NewVael, float, MaxVael);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOverheatChanged, bool, bIsOverheated);

class UWTBRCoreStatsDataAsset;

UCLASS(ClassGroup=(WTBR), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRVaelComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRVaelComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
    TObjectPtr<UWTBRCoreStatsDataAsset> StatsData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vael")
    float MaxVael = 1000.f;

    // Broadcast when Vael energy leaves the character capsule → radar gets a ping
    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnVaelReleased OnVaelReleased;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnVaelChanged OnVaelChanged;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnOverheatChanged OnOverheatChanged;

    UFUNCTION(BlueprintCallable, Category="Vael")
    bool TryConsumeVael(float Amount);

    // Call this from a Trigger/Projectile the moment the Vael actor exits the capsule
    UFUNCTION(BlueprintCallable, Category="Vael")
    void NotifyVaelLeftCharacterBounds();

    UFUNCTION(BlueprintPure, Category="Vael")
    float GetCurrentVael() const { return CurrentVael; }

    UFUNCTION(BlueprintPure, Category="Vael")
    bool IsOverheated() const { return bOverheated; }

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(Replicated)
    float CurrentVael;

    UPROPERTY(Replicated)
    bool bOverheated = false;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
