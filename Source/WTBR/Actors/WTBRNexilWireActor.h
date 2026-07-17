// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WTBRNexilWireActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UWTBRNexilTrigger;

UCLASS()
class WTBR_API AWTBRNexilWireActor : public AActor
{
    GENERATED_BODY()

public:
    AWTBRNexilWireActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "WTBR | Nexil Wire | Components")
    TObjectPtr<UStaticMeshComponent> WireMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "WTBR | Nexil Wire | Components")
    TObjectPtr<UBoxComponent> WireOverlap;

    UPROPERTY(ReplicatedUsing = OnRep_bIsTriggered, BlueprintReadOnly,
        Category = "WTBR | Nexil Wire | State")
    bool bIsTriggered = false;

    // Hit-count, not a damage-amount pool — see FWTBRNexilParams::WireHP comment.
    UPROPERTY(ReplicatedUsing = OnRep_WireHP, BlueprintReadOnly,
        Category = "WTBR | Nexil Wire | State")
    int32 WireHP = 1;

    UPROPERTY(Replicated, BlueprintReadOnly,
        Category = "WTBR | Nexil Wire | State")
    int32 MaxWireHP = 1;

    UFUNCTION(BlueprintCallable, Category = "WTBR | Nexil Wire")
    void InitializeWire(
        float InLifetime,
        float InStaggerDuration,
        float InWireLength,
        int32 InWireHP,
        UWTBRNexilTrigger* InOwnerTrigger);

    // Removes exactly 1 WireHP regardless of the hitting weapon's own damage
    // number (GDD: "ฟันหรือยิงขาดได้" — any qualifying hit cuts it). Destroys the
    // wire silently (no trip/stagger/Action Ping) once WireHP reaches 0.
    UFUNCTION(BlueprintCallable, Category = "WTBR | Nexil Wire")
    void TakeHit();

    UFUNCTION(BlueprintPure, Category = "WTBR | Nexil Wire")
    float GetWireHPPercent() const;

    UFUNCTION(BlueprintImplementableEvent,
        Category = "WTBR | Nexil Wire | VFX")
    void OnWireTriggeredVFX();

    // Distinct from OnWireTriggeredVFX — fires when gunfire/explosion cuts the
    // wire down before anyone walked into it.
    UFUNCTION(BlueprintImplementableEvent,
        Category = "WTBR | Nexil Wire | VFX")
    void OnWireDestroyedByDamageVFX();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_bIsTriggered();

    UFUNCTION()
    void OnRep_WireHP();

    UFUNCTION()
    void OnWireOverlapBegin(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

#if WITH_DEV_AUTOMATION_TESTS
public:
    // Test-only: headless fixtures have no initialized physics scene, so
    // OnComponentBeginOverlap never fires on its own — forward straight to the
    // protected handler the way real overlap delivery would.
    void OnWireOverlapBeginForTest(AActor* OtherActor)
    {
        OnWireOverlapBegin(nullptr, OtherActor, nullptr, 0, false, FHitResult());
    }
#endif

private:
    float StaggerDuration = 1.5f;
    TWeakObjectPtr<UWTBRNexilTrigger> OwnerTrigger;
    FTimerHandle LifetimeTimer;

    UFUNCTION()
    void OnLifetimeExpired();

    void ApplyStaggerToCharacter(AActor* TargetActor);
    void TriggerAndDestroy(AActor* TriggeredBy);
};
