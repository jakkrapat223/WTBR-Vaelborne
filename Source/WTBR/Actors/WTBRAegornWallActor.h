// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WTBRAegornWallActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class AWTBRCharacter;

// bExpiredNaturally: true = lifetime/Duration ran out on its own; false = HP
// was driven to zero by damage (or a deliberate self-damage collapse, e.g.
// UWTBRAegornTrigger::CancelShield). Lets listeners tell "ran its course"
// apart from "got beaten."
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWallDestroyed, bool, bExpiredNaturally);

UCLASS()
class WTBR_API AWTBRAegornWallActor : public AActor
{
    GENERATED_BODY()

public:
    AWTBRAegornWallActor();

    UPROPERTY(BlueprintAssignable, Category = "WTBR | Aegorn Wall | Events")
    FOnWallDestroyed OnWallDestroyed;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "WTBR | Aegorn Wall | Components")
    TObjectPtr<UStaticMeshComponent> WallMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "WTBR | Aegorn Wall | Components")
    TObjectPtr<UBoxComponent> WallCollision;

    UPROPERTY(ReplicatedUsing = OnRep_WallHP, BlueprintReadOnly,
        Category = "WTBR | Aegorn Wall | State")
    float WallHP = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly,
        Category = "WTBR | Aegorn Wall | State")
    float MaxWallHP = 0.0f;

    // InDuration > 0 = auto-despawn after that many seconds; 0 = no lifetime limit.
    UFUNCTION(BlueprintCallable, Category = "WTBR | Aegorn Wall")
    void InitializeWall(float InMaxHP, float InDuration = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "WTBR | Aegorn Wall")
    void TakeDamageFromProjectile(float DamageAmount);

    // Escudo-only: begin an eruption animation from behind the anchoring
    // surface out to InFinalLocation over InBuildTime seconds (server-only,
    // matches canon "sprouts from the surface"). While erupting, every
    // AWTBRCharacter and physics-simulated prop newly swept by the rising
    // collision is displaced once, at InEruptionImpulse — caster, ally and
    // enemy alike, all thrown along the wall's grow axis. InClearanceRatio
    // scales the sideways nudge that keeps a straight-up launch from dropping
    // the target back onto the finished wall.
    // Not used by Aegorn Shield, which has its own hold-tracking tick on the
    // Trigger side (UWTBRAegornTrigger::TickHeldShield).
    void BeginEscudoEruption(const FVector& InFinalLocation, float InBuildTime,
        float InEruptionImpulse, float InClearanceRatio);

    UFUNCTION(BlueprintCallable, Category = "WTBR | Aegorn Wall | Status")
    void ApplyBrittle(float DamageMultiplier, float Duration);

    UFUNCTION(BlueprintPure, Category = "WTBR | Aegorn Wall")
    float GetWallHPPercent() const;

    // Half-extents WallCollision should have for THIS class's WallMesh
    // (static mesh + Blueprint-authored component scale), independent of
    // whether BeginPlay has ever run. Safe to call on a Class Default Object
    // — mesh/scale are Blueprint-construction-time data, not runtime state.
    // This exists so pre-placement code that only ever has a CDO (Escudo's
    // ValidatePanelPlacement anti-trap check, the ghost-preview footprint)
    // can compute the exact same size BeginPlay will end up applying to a
    // real spawned instance's WallCollision, instead of the two silently
    // disagreeing (the bug this fixes: BeginPlay's own auto-fit only ever
    // touched an actually-spawned instance's collision, which nothing that
    // reads GetDefaultObject() ever sees).
    UFUNCTION(BlueprintPure, Category = "WTBR | Aegorn Wall")
    FVector ComputeIntendedCollisionExtent() const;

    bool HandleProjectileContact(AActor* OtherActor, const TCHAR* Source);

    UFUNCTION(BlueprintImplementableEvent,
        Category = "WTBR | Aegorn Wall | VFX")
    void OnWallDamaged(float NewHP, float DamageAmount);

    UFUNCTION(BlueprintImplementableEvent,
        Category = "WTBR | Aegorn Wall | VFX")
    void OnWallCollapsed();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_WallHP();

    UFUNCTION()
    void OnWallHit(UPrimitiveComponent* HitComponent,
                   AActor* OtherActor,
                   UPrimitiveComponent* OtherComp,
                   FVector NormalImpulse,
                   const FHitResult& Hit);

    UFUNCTION()
    void OnWallOverlap(UPrimitiveComponent* OverlappedComponent,
                       AActor* OtherActor,
                       UPrimitiveComponent* OtherComp,
                       int32 OtherBodyIndex,
                       bool bFromSweep,
                       const FHitResult& SweepResult);

private:
    void DestroyWall(bool bExpiredNaturally);
    void ClearBrittle();

    void TickEscudoEruption(float DeltaTime);
    void ApplyEruptionDisplacement(const FVector& SweepFrom, const FVector& SweepTo);

    UFUNCTION()
    void OnLifetimeExpired();

    UPROPERTY()
    TArray<TObjectPtr<AActor>> DamagedProjectiles;

    FTimerHandle LifetimeTimer;
    FTimerHandle BrittleExpiryTimer;
    float BrittleDamageMultiplier = 1.0f;

    bool bIsErupting = false;
    FVector EruptStartLocation = FVector::ZeroVector;
    FVector EruptFinalLocation = FVector::ZeroVector;
    float EruptElapsed = 0.0f;
    float EruptDuration = 0.0f;
    float EruptImpulse = 0.0f;
    float EruptClearanceRatio = 0.0f;

    UPROPERTY()
    TSet<TObjectPtr<AWTBRCharacter>> EruptDisplacedCharacters;

    UPROPERTY()
    TSet<TObjectPtr<UPrimitiveComponent>> EruptDisplacedProps;
};
