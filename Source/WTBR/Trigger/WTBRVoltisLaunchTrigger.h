// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRMovementTrigger.h"
#include "WTBRVoltisLaunchTrigger.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoltisLaunch, bool, bIsDualWield);
// bWasHardLanding param removed with the ceiling-obstruction check it reported
// (blocked ANY Voltis dash toward a nearby wall or enemy Pawn — WorldStatic
// trace channel blocks Pawns by default — not just an actual low ceiling).
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVoltisLand);

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRVoltisLaunchTrigger : public UWTBRMovementTrigger
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "WTBR | Voltis | Events")
    FOnVoltisLaunch OnVoltisLaunch;

    UPROPERTY(BlueprintAssignable, Category = "WTBR | Voltis | Events")
    FOnVoltisLand OnVoltisLand;

    UPROPERTY(ReplicatedUsing = OnRep_bIsStaggered, BlueprintReadOnly,
        Category = "WTBR | Voltis | State")
    bool bIsStaggered = false;

    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual void OnReleased_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual void Deactivate_Implementation() override;

    UFUNCTION()
    void OnCharacterLanded(const FHitResult& Hit);

    UFUNCTION(BlueprintPure, Category = "WTBR | Voltis | State")
    bool IsStaggered() const { return bIsStaggered; }

    UFUNCTION(BlueprintPure, Category = "WTBR | Voltis | State")
    int32 GetRemainingLaunches() const { return RemainingLaunches; }

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Voltis | VFX")
    void OnVoltisLaunched(bool bIsDualWield);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Voltis | VFX")
    void OnVoltisHardLanding();

    virtual void InitializeTrigger(
        AWTBRCharacter* InOwnerCharacter,
        UWTBRTriggerDataAsset* InDataAsset) override;

protected:
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_bIsStaggered();

private:
    void PerformLaunch(bool bIsDualWield, const FVector& LaunchDir);
    void StartStagger();

    UFUNCTION()
    void OnStaggerExpired();

    int32 RemainingLaunches = 0;
    FTimerHandle StaggerTimer;

    // ⚠ PLAYTEST PENDING — จะย้ายไป DataAsset ใน Phase 5
    static constexpr float STAGGER_DURATION = 0.4f;
};
