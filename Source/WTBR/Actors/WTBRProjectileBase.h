// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "WTBRProjectileBase.generated.h"

class UBoxComponent;
class UProjectileMovementComponent;
class UInterpToMovementComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EProjectileState : uint8
{
    Active    UMETA(DisplayName = "Active"),
    Exploded  UMETA(DisplayName = "Exploded"),
    Destroyed UMETA(DisplayName = "Destroyed"),
};

UCLASS(BlueprintType, Blueprintable)
class WTBR_API AWTBRProjectileBase : public AActor
{
    GENERATED_BODY()

public:
    AWTBRProjectileBase();

    // ── Components ─────────────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "WTBR | Projectile | Components")
    TObjectPtr<UBoxComponent> BoxCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "WTBR | Projectile | Components")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "WTBR | Projectile | Components")
    TObjectPtr<UInterpToMovementComponent> InterpMovement;

    // ── Config (set via InitializeProjectile) ──────────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float BaseDamage = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float MaxRange = 3000.0f;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    ETriggerCategory OwnerCategory = ETriggerCategory::None;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    bool bIsCubeSplit = false;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    int32 CubeSplitCount = 4;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    bool bIsSniper = false;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    bool bExplodeOnImpact = false;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float ExplosionRadius = 0.0f;

    // When true the bullet passes through characters but stops at geometry (Sniper only)
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    bool bCanPenetrate = false;

    // Ventryx (Black Trigger) — non-zero launches hit character away from impact point
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float KnockbackForce = 0.0f;

    float BleedDamagePerTick = 0.0f;
    float BleedDuration = 0.0f;
    float ShieldBrittleDamageMultiplier = 1.0f;
    float ShieldBrittleDuration = 0.0f;

    void ConfigureOnHitEffects(float InBleedDamagePerTick, float InBleedDuration,
        float InShieldBrittleDamageMultiplier, float InShieldBrittleDuration);

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    TObjectPtr<AActor> OwnerInstigator;

    // Actors already damaged this flight — prevents re-hit on re-entry (penetration)
    UPROPERTY()
    TArray<TObjectPtr<AActor>> DamagedActors;

    // ── State (Replicated) ─────────────────────────────────────────────────────
    UPROPERTY(ReplicatedUsing = OnRep_ProjectileState, BlueprintReadOnly,
        Category = "WTBR | Projectile | State")
    EProjectileState ProjectileState = EProjectileState::Active;

    // ── Core Interface ─────────────────────────────────────────────────────────
    // Called by spawning Trigger immediately after SpawnActorDeferred
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void InitializeProjectile(float InDamage, float InSpeed,
        ETriggerCategory InCategory, bool bSniper,
        bool bExplode, float ExplodeRadius);

    // Called after FinishSpawning — sets velocity and lifespan
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void Launch(FVector Direction, AActor* InInstigator);

    // Enable homing after Launch() — call only on authority
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void EnableHoming(USceneComponent* Target, float Accel);

    // Serpveil curved path — deactivates ProjectileMovement, sets InterpToMovement control points
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void InitializePathMovement(const TArray<FVector>& Points, float Speed, AActor* InInstigator);

    // ── VFX Hooks ──────────────────────────────────────────────────────────────
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnProjectileLaunched();

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnProjectileHitVFX(FVector ImpactPoint);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnExplosionVFX(FVector Center, float Radius);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnCubeSplitVFX();

    // Spawn sub-bullets for a cube-split burst; called immediately by Labyrn FireComposite.
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void SpawnCubeSplits();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_ProjectileState();

    UFUNCTION()
    void OnInterpMovementEnd(const FHitResult& ImpactResult, float Time);

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnProjectileHit(UPrimitiveComponent* HitComponent,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        FVector NormalImpulse, const FHitResult& Hit);

private:
    float CachedSpeed   = 0.0f;
    bool  bHandledClash = false; // prevents double-processing when both bullets' overlaps fire

    void TriggerExplosion();
    void OnBulletClash(AWTBRProjectileBase* OtherBullet);
};
