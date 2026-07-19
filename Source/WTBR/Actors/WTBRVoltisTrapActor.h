// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WTBRVoltisTrapActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UWTBRVoltisLaunchTrigger;
class UNiagaraSystem;
class UNiagaraComponent;

UCLASS()
class WTBR_API AWTBRVoltisTrapActor : public AActor
{
    GENERATED_BODY()

public:
    AWTBRVoltisTrapActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "WTBR | Voltis Trap | Components")
    TObjectPtr<UStaticMeshComponent> TrapMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "WTBR | Voltis Trap | Components")
    TObjectPtr<USphereComponent> TrapOverlap;

    UPROPERTY(ReplicatedUsing = OnRep_bIsTriggered, BlueprintReadOnly,
        Category = "WTBR | Voltis Trap | State")
    bool bIsTriggered = false;

    // One-shot burst played once when the trap is placed. Defaults to a real
    // generated asset (see constructor) so this works with zero Editor/BP setup —
    // a BP subclass can still override it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Voltis Trap | VFX")
    TObjectPtr<UNiagaraSystem> PlacedEffect;

    // One-shot burst played once when the trap launches someone, right before it
    // destroys itself. Same default-asset convention as PlacedEffect.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Voltis Trap | VFX")
    TObjectPtr<UNiagaraSystem> TriggeredEffect;

    UFUNCTION(BlueprintCallable, Category = "WTBR | Voltis Trap")
    void InitializeTrap(
        float InLifetime,
        float InExplosionRadius,
        float InDamage,
        float InVerticalLaunchForce,
        UWTBRVoltisLaunchTrigger* InOwnerTrigger);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Voltis Trap | VFX")
    void OnTrapTriggeredVFX();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayTriggeredVFX();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_bIsTriggered();

    UFUNCTION()
    void OnTrapOverlapBegin(
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
    void OnTrapOverlapBeginForTest(AActor* OtherActor)
    {
        OnTrapOverlapBegin(nullptr, OtherActor, nullptr, 0, false, FHitResult());
    }
#endif

private:
    float ExplosionRadius = 300.0f;
    float TrapDamage = 0.0f;
    float VerticalLaunchForce = 1200.0f;
    TWeakObjectPtr<UWTBRVoltisLaunchTrigger> OwnerTrigger;
    FTimerHandle LifetimeTimer;

    UFUNCTION()
    void OnLifetimeExpired();

    void LaunchCharacterUp(AActor* TargetActor);
    void TriggerAndDestroy(AActor* TriggeredBy);

    // NS_Template_Burst-derived assets loop indefinitely by default (Emitter State
    // Loop Behavior = Infinite, kept that way so the effect is easy to eyeball while
    // editing) — a bare SpawnSystemAtLocation call never completes on its own, so
    // bAutoDestroy never fires and the burst plays forever. This force-stops it after
    // Lifetime instead of relying on natural completion. Real bug found via the
    // owner's own PIE test ("green smoke stays after using Trap").
    // The cube-template system is isotropic by itself.  A thin rectangular
    // scale lets the trap read as a foot-sized ground plate, not a floating cube.
    void PlayOneShotBurst(
        UNiagaraSystem* Effect,
        float Lifetime,
        const FVector& VisualScale = FVector::OneVector);

    void StartPlacedPlate();
    void StopPlacedPlate();

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> PlacedPlateComponent;

    bool bTriggeredVFXAlreadyPlayed = false;
};
