// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Actors/WTBRAegornWallActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

AWTBRAegornWallActor::AWTBRAegornWallActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("WallMesh"));
    RootComponent = WallMesh;

    WallCollision = CreateDefaultSubobject<UBoxComponent>(
        TEXT("WallCollision"));
    WallCollision->SetupAttachment(RootComponent);
    WallCollision->SetBoxExtent(FVector(10.0f, 150.0f, 150.0f));
    WallCollision->SetCollisionEnabled(
        ECollisionEnabled::QueryAndPhysics);
    WallCollision->SetCollisionObjectType(ECC_WorldStatic);
    WallCollision->SetCollisionResponseToAllChannels(ECR_Block);
}

void AWTBRAegornWallActor::BeginPlay()
{
    Super::BeginPlay();
    Tags.Add(TEXT("AegornWall"));
    if (HasAuthority())
    {
        WallCollision->OnComponentHit.AddDynamic(
            this, &AWTBRAegornWallActor::OnWallHit);
    }
}

void AWTBRAegornWallActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWTBRAegornWallActor, WallHP);
    DOREPLIFETIME(AWTBRAegornWallActor, MaxWallHP);
}

void AWTBRAegornWallActor::InitializeWall(float InMaxHP)
{
    ensure(HasAuthority());
    MaxWallHP = InMaxHP;
    WallHP    = InMaxHP;
}

void AWTBRAegornWallActor::TakeDamageFromProjectile(float DamageAmount)
{
    if (!HasAuthority()) return;
    if (WallHP <= 0.0f) return;
    WallHP = FMath::Max(0.0f, WallHP - DamageAmount);
    OnWallDamaged(WallHP, DamageAmount);
    if (WallHP <= 0.0f)
        DestroyWall();
}

float AWTBRAegornWallActor::GetWallHPPercent() const
{
    if (MaxWallHP <= 0.0f) return 0.0f;
    return WallHP / MaxWallHP;
}

void AWTBRAegornWallActor::OnWallHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (!HasAuthority()) return;
    if (!IsValid(OtherActor)) return;
    if (!OtherActor->ActorHasTag(TEXT("WTBRProjectile"))) return;

    // TODO Phase 4: ดึง Damage จาก Projectile DataAsset
    const float ProjectileDamage = 50.0f;
    TakeDamageFromProjectile(ProjectileDamage);
    OtherActor->Destroy();
}

void AWTBRAegornWallActor::DestroyWall()
{
    OnWallDestroyed.Broadcast();
    OnWallCollapsed();
    Destroy();
}

void AWTBRAegornWallActor::OnRep_WallHP()
{
    OnWallDamaged(WallHP, 0.0f);
}
