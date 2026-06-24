// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "WTBRProjectileBase.generated.h"

class UBoxComponent;
class UProjectileMovementComponent;
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

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    TObjectPtr<AActor> OwnerInstigator;

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

    // ── VFX Hooks ──────────────────────────────────────────────────────────────
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnProjectileLaunched();

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnProjectileHitVFX(FVector ImpactPoint);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnExplosionVFX(FVector Center, float Radius);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnCubeSplitVFX();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_ProjectileState();

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

private:
    float CachedSpeed   = 0.0f;
    bool  bHandledClash = false; // prevents double-processing when both bullets' overlaps fire

    void TriggerExplosion();
    void OnBulletClash(AWTBRProjectileBase* OtherBullet);
    void SpawnCubeSplits();
};
