// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WTBRAegornWallActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWallDestroyed);

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

    UFUNCTION(BlueprintCallable, Category = "WTBR | Aegorn Wall")
    void InitializeWall(float InMaxHP);

    UFUNCTION(BlueprintCallable, Category = "WTBR | Aegorn Wall")
    void TakeDamageFromProjectile(float DamageAmount);

    UFUNCTION(BlueprintPure, Category = "WTBR | Aegorn Wall")
    float GetWallHPPercent() const;

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

private:
    void DestroyWall();
};
