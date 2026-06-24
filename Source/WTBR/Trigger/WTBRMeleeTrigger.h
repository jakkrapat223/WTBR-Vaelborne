// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRMeleeTrigger.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnMeleeSwing,
    bool, bIsDualWield
);

UCLASS(Abstract, BlueprintType, Blueprintable)
class WTBR_API UWTBRMeleeTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "WTBR | Melee | Events")
    FOnMeleeSwing OnMeleeSwing;

    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintPure, Category = "WTBR | Melee | State")
    bool IsOnCooldown() const { return bIsOnCooldown; }

protected:
    UFUNCTION(BlueprintCallable, Category = "WTBR | Melee | Sweep")
    virtual void PerformSingleSweep(TArray<FHitResult>& OutHits);

    UFUNCTION(BlueprintCallable, Category = "WTBR | Melee | Sweep")
    virtual void PerformDualSweep(TArray<FHitResult>& OutHits);

    void ApplyDamageToHits(
        const TArray<FHitResult>& Hits,
        float DamageMultiplier);

    const FWTBRMeleeHitboxParams& GetHitboxParams() const;

    void SweepCapsuleAt(
        const FVector& Center,
        const FQuat& Rotation,
        float Radius,
        float HalfHeight,
        TArray<FHitResult>& OutHits);

    // Anti-tunneling sweep: ลาก Capsule จาก Start ไป End
    // ใช้เมื่อ blade เคลื่อนที่ระหว่าง ticks — ป้องกัน enemy
    // ที่อยู่กลางเส้นทางรอดตัวไปเพราะ position jump ระหว่าง tick
    void SweepCapsuleFromTo(
        const FVector& Start,
        const FVector& End,
        const FQuat& Rotation,
        float Radius,
        float HalfHeight,
        TArray<FHitResult>& OutHits);

    void FilterHits(
        TArray<FHitResult>& InOutHits,
        const AActor* InstigatorActor);

    // Cooldown — moved to protected so subclasses (Lacern etc.)
    // can call StartCooldown() directly without hacks
    bool bIsOnCooldown = false;
    FTimerHandle CooldownTimer;
    void StartCooldown();

    UFUNCTION()
    void OnCooldownExpired();
};
