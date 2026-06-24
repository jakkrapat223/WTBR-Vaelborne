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

    UFUNCTION(BlueprintCallable, Category = "WTBR | Nexil Wire")
    void InitializeWire(
        float InLifetime,
        float InStaggerDuration,
        float InWireLength,
        UWTBRNexilTrigger* InOwnerTrigger);

    UFUNCTION(BlueprintImplementableEvent,
        Category = "WTBR | Nexil Wire | VFX")
    void OnWireTriggeredVFX();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_bIsTriggered();

    UFUNCTION()
    void OnWireOverlapBegin(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

private:
    float StaggerDuration = 1.5f;
    TWeakObjectPtr<UWTBRNexilTrigger> OwnerTrigger;
    FTimerHandle LifetimeTimer;

    UFUNCTION()
    void OnLifetimeExpired();

    void ApplyStaggerToCharacter(AActor* TargetActor);
    void TriggerAndDestroy(AActor* TriggeredBy);
};
