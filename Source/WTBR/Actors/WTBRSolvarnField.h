// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WTBRSolvarnField.generated.h"

class USphereComponent;
class AWTBRCharacter;

// Defense field spawned by Solvarn (Black Trigger — Suiren).
// Destroys any AWTBRProjectileBase that enters its radius.
// Sustained by Vael drain; collapses when Vael is exhausted or duration expires.
UCLASS(BlueprintType, Blueprintable)
class WTBR_API AWTBRSolvarnField : public AActor
{
    GENERATED_BODY()

public:
    AWTBRSolvarnField();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WTBR | Solvarn | Components")
    TObjectPtr<USphereComponent> SphereCollision;

    // Call via SpawnActorDeferred before FinishSpawning
    void InitializeField(AWTBRCharacter* InOwner, float InRadius,
        float InDuration, float InVaelDrainPerSec);

    // ── VFX Hooks ─────────────────────────────────────────────────────────────
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Solvarn | VFX")
    void OnSolvarnActivated();

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Solvarn | VFX")
    void OnSolvarnDeactivated();

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Solvarn | VFX")
    void OnSolvarnBlockedProjectile();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

private:
    TWeakObjectPtr<AWTBRCharacter> OwnerChar;
    float VaelDrainPerSec = 0.0f;
    FTimerHandle DrainTimer;

    void DrainVael();
};
