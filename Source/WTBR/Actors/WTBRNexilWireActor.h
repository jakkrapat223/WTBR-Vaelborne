// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WTBRNexilWireActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UWTBRNexilTrigger;
class AWTBRCharacter;

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
        UWTBRNexilTrigger* InOwnerTrigger,
        float InZiplineLaunchSpeed = 0.0f);

    // Zipline ability (owner + teammates only, not the trip mechanic): a wire
    // can be F-grabbed and later released via Jump, launching the grabber in
    // whatever direction their camera is facing at release. Wire persists
    // afterward — reusable until natural expiry or an enemy cuts it, same as
    // any other contact. Returns false for the wire's own owner (no self-grab
    // restriction was requested, but there's no defined use for it either —
    // revisit if a real need shows up), enemies, or an already-triggered wire.
    UFUNCTION(BlueprintPure, Category = "WTBR | Nexil Wire | Zipline")
    bool CanBeGrabbedBy(const AWTBRCharacter* Character) const;

    UFUNCTION(BlueprintPure, Category = "WTBR | Nexil Wire | Zipline")
    float GetZiplineLaunchSpeed() const { return ZiplineLaunchSpeed; }

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
    float ZiplineLaunchSpeed = 0.0f;
    TWeakObjectPtr<UWTBRNexilTrigger> OwnerTrigger;
    FTimerHandle LifetimeTimer;

    UFUNCTION()
    void OnLifetimeExpired();

    void ApplyStaggerToCharacter(AActor* TargetActor);
    void TriggerAndDestroy(AActor* TriggeredBy);
};
