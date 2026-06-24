// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WTBRKaldrixZone.generated.h"

class AWTBRCharacter;

UENUM(BlueprintType)
enum class EKaldrixState : uint8
{
    Placed   UMETA(DisplayName = "Placed"),
    Armed    UMETA(DisplayName = "Armed"),
    Exploded UMETA(DisplayName = "Exploded"),
};

// Timed explosion zone placed by Kaldrix (Black Trigger — Kazan).
// Arms after KaldrixArmTime seconds, then sweeps for pawns and detonates.
UCLASS(BlueprintType, Blueprintable)
class WTBR_API AWTBRKaldrixZone : public AActor
{
    GENERATED_BODY()

public:
    AWTBRKaldrixZone();

    UPROPERTY(ReplicatedUsing = OnRep_KaldrixState, BlueprintReadOnly,
        Category = "WTBR | Kaldrix | State")
    EKaldrixState KaldrixState = EKaldrixState::Placed;

    // Call via SpawnActorDeferred before FinishSpawning
    void InitializeZone(float InDamage, float InRadius, float InArmTime,
        float InStaggerDuration, AWTBRCharacter* InInstigator);

    // ── VFX Hooks ─────────────────────────────────────────────────────────────
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Kaldrix | VFX")
    void OnKaldrixPlaced();

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Kaldrix | VFX")
    void OnKaldrixArmed();

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Kaldrix | VFX")
    void OnKaldrixExplode();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_KaldrixState();

private:
    float KaldrixDamage          = 0.0f;
    float KaldrixRadius          = 500.0f;
    float KaldrixArmTime         = 3.0f;
    float KaldrixStaggerDuration = 1.5f;
    TWeakObjectPtr<AWTBRCharacter> InstigatorChar;
    FTimerHandle ArmTimer;

    void Arm();
    void Explode();
};
