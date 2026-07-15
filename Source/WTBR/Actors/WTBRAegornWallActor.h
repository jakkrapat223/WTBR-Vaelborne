// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WTBRAegornWallActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

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

    UFUNCTION(BlueprintCallable, Category = "WTBR | Aegorn Wall | Status")
    void ApplyBrittle(float DamageMultiplier, float Duration);

    UFUNCTION(BlueprintPure, Category = "WTBR | Aegorn Wall")
    float GetWallHPPercent() const;

    bool HandleProjectileContact(AActor* OtherActor, const TCHAR* Source);

    UFUNCTION(BlueprintImplementableEvent,
        Category = "WTBR | Aegorn Wall | VFX")
    void OnWallDamaged(float NewHP, float DamageAmount);

    UFUNCTION(BlueprintImplementableEvent,
        Category = "WTBR | Aegorn Wall | VFX")
    void OnWallCollapsed();

protected:
    virtual void BeginPlay() override;
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

    UFUNCTION()
    void OnLifetimeExpired();

    UPROPERTY()
    TArray<TObjectPtr<AActor>> DamagedProjectiles;

    FTimerHandle LifetimeTimer;
    FTimerHandle BrittleExpiryTimer;
    float BrittleDamageMultiplier = 1.0f;
};
