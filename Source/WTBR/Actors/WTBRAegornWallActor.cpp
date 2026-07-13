// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Actors/WTBRAegornWallActor.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRProjectileBase.h"
#include "WTBRCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AWTBRAegornWallActor::AWTBRAegornWallActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    // A held Aegorn shield is repositioned ~20x/sec on the server
    // (UWTBRAegornTrigger::TickHeldShield); replicate that movement and update
    // often so clients see it track the caster's aim instead of lagging behind
    // at a stale/crooked transform.
    SetReplicateMovement(true);
    NetUpdateFrequency = 60.0f;
    MinNetUpdateFrequency = 30.0f;

    WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("WallMesh"));
    RootComponent = WallMesh;

    WallCollision = CreateDefaultSubobject<UBoxComponent>(
        TEXT("WallCollision"));
    WallCollision->SetupAttachment(RootComponent);
    WallCollision->SetBoxExtent(FVector(10.0f, 150.0f, 150.0f));
    WallCollision->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly);
    WallCollision->SetCollisionObjectType(ECC_WorldStatic);
    WallCollision->SetCollisionResponseToAllChannels(ECR_Block);
    WallCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    WallCollision->SetGenerateOverlapEvents(true);

    WallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WallMesh->SetGenerateOverlapEvents(false);
}

void AWTBRAegornWallActor::BeginPlay()
{
    Super::BeginPlay();
    Tags.Add(TEXT("AegornWall"));
    if (IsValid(WallMesh))
    {
        WallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WallMesh->SetGenerateOverlapEvents(false);
    }
    if (IsValid(WallCollision))
    {
        WallCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        WallCollision->SetCollisionObjectType(ECC_WorldStatic);
        WallCollision->SetCollisionResponseToAllChannels(ECR_Block);
        WallCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        WallCollision->SetGenerateOverlapEvents(true);
        FVector WallExtent = WallCollision->GetUnscaledBoxExtent();
        if (WallExtent.X < 20.0f)
        {
            WallExtent.X = 20.0f;
            WallCollision->SetBoxExtent(WallExtent);
        }

        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] CollisionConfig | Wall=%s | HasAuthority=%s | CollisionEnabled=%d | ObjectType=%d | GenerateOverlap=%s | RespWorldDynamic=%d | RespWorldStatic=%d | RespPawn=%d | Extent=%s | Location=%s"),
            *GetNameSafe(this),
            HasAuthority() ? TEXT("true") : TEXT("false"),
            static_cast<int32>(WallCollision->GetCollisionEnabled()),
            static_cast<int32>(WallCollision->GetCollisionObjectType()),
            WallCollision->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"),
            static_cast<int32>(WallCollision->GetCollisionResponseToChannel(ECC_WorldDynamic)),
            static_cast<int32>(WallCollision->GetCollisionResponseToChannel(ECC_WorldStatic)),
            static_cast<int32>(WallCollision->GetCollisionResponseToChannel(ECC_Pawn)),
            *WallCollision->GetScaledBoxExtent().ToString(),
            *WallCollision->GetComponentLocation().ToString());
    }
    if (HasAuthority())
    {
        WallCollision->OnComponentHit.AddDynamic(
            this, &AWTBRAegornWallActor::OnWallHit);
        WallCollision->OnComponentBeginOverlap.AddDynamic(
            this, &AWTBRAegornWallActor::OnWallOverlap);
    }
}

void AWTBRAegornWallActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWTBRAegornWallActor, WallHP);
    DOREPLIFETIME(AWTBRAegornWallActor, MaxWallHP);
}

void AWTBRAegornWallActor::InitializeWall(float InMaxHP, float InDuration)
{
    ensure(HasAuthority());
    MaxWallHP = InMaxHP;
    WallHP    = InMaxHP;
    if (InDuration > 0.0f)
    {
        GetWorldTimerManager().SetTimer(
            LifetimeTimer,
            this, &AWTBRAegornWallActor::OnLifetimeExpired,
            InDuration,
            false);
    }
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] InitializeWall | Wall=%s | HasAuthority=%s | WallHP=%.1f | MaxWallHP=%.1f | Duration=%.1f | Replicates=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        WallHP,
        MaxWallHP,
        InDuration,
        GetIsReplicated() ? TEXT("true") : TEXT("false"));
}

void AWTBRAegornWallActor::OnLifetimeExpired()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] LifetimeExpired | Wall=%s"),
        *GetNameSafe(this));
    DestroyWall(/*bExpiredNaturally=*/true);
}

void AWTBRAegornWallActor::TakeDamageFromProjectile(float DamageAmount)
{
    if (!HasAuthority()) return;
    if (WallHP <= 0.0f) return;

    const float OldHP = WallHP;
    WallHP = FMath::Max(0.0f, WallHP - DamageAmount);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] WallDamage | OldHP=%.1f | Damage=%.1f | NewHP=%.1f"),
        OldHP,
        DamageAmount,
        WallHP);

    OnWallDamaged(WallHP, DamageAmount);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] OnWallDamaged Fired | Wall=%s"),
        *GetNameSafe(this));

    if (WallHP <= 0.0f)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] WallDestroyed | Wall=%s"),
            *GetNameSafe(this));
        DestroyWall(/*bExpiredNaturally=*/false);
    }
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
    if (OtherActor && OtherActor->IsA(AWTBRCharacter::StaticClass()))
    {
        return;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] Hit | Wall=%s | Other=%s | OtherClass=%s | HasAuthority=%s | Source=Hit"),
        *GetNameSafe(this),
        *GetNameSafe(OtherActor),
        IsValid(OtherActor) ? *GetNameSafe(OtherActor->GetClass()) : TEXT("None"),
        HasAuthority() ? TEXT("true") : TEXT("false"));

    if (HandleProjectileContact(OtherActor, TEXT("Hit")))
    {
        return;
    }
    return;

    if (!HasAuthority()) return;
    if (!IsValid(OtherActor)) return;
    if (!OtherActor->ActorHasTag(TEXT("WTBRProjectile"))) return;

    // TODO Phase 4: ดึง Damage จาก Projectile DataAsset
    const float ProjectileDamage = 50.0f;
    TakeDamageFromProjectile(ProjectileDamage);
    OtherActor->Destroy();
}

void AWTBRAegornWallActor::OnWallOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->IsA(AWTBRCharacter::StaticClass()))
    {
        return;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] Hit | Wall=%s | Other=%s | OtherClass=%s | HasAuthority=%s | Source=Overlap"),
        *GetNameSafe(this),
        *GetNameSafe(OtherActor),
        IsValid(OtherActor) ? *GetNameSafe(OtherActor->GetClass()) : TEXT("None"),
        HasAuthority() ? TEXT("true") : TEXT("false"));

    HandleProjectileContact(OtherActor, TEXT("Overlap"));
}

bool AWTBRAegornWallActor::HandleProjectileContact(AActor* OtherActor, const TCHAR* Source)
{
    if (!HasAuthority()) return false;
    if (!IsValid(OtherActor)) return false;
    if (DamagedProjectiles.Contains(OtherActor)) return false;

    if (AWTBRProjectileBase* Projectile = Cast<AWTBRProjectileBase>(OtherActor))
    {
        const float ProjectileDamage = Projectile->BaseDamage;
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] ProjectileDetected | Method=Cast | Projectile=%s | Damage=%.1f | Source=%s"),
            *GetNameSafe(Projectile),
            ProjectileDamage,
            Source);

        DamagedProjectiles.Add(OtherActor);
        TakeDamageFromProjectile(ProjectileDamage);
        OtherActor->Destroy();
        return true;
    }

    if (OtherActor->ActorHasTag(TEXT("WTBRProjectile")))
    {
        const float ProjectileDamage = 50.0f;
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] ProjectileDetected | Method=Tag | Projectile=%s | Damage=%.1f | Source=%s"),
            *GetNameSafe(OtherActor),
            ProjectileDamage,
            Source);

        DamagedProjectiles.Add(OtherActor);
        TakeDamageFromProjectile(ProjectileDamage);
        OtherActor->Destroy();
        return true;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] ProjectileRejected | Other=%s | Source=%s | HasWTBRProjectileTag=%s"),
        *GetNameSafe(OtherActor),
        Source,
        OtherActor->ActorHasTag(TEXT("WTBRProjectile")) ? TEXT("true") : TEXT("false"));
    return false;
}

void AWTBRAegornWallActor::DestroyWall(bool bExpiredNaturally)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] DestroyWall | Wall=%s | HasAuthority=%s | ExpiredNaturally=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        bExpiredNaturally ? TEXT("true") : TEXT("false"));
    OnWallDestroyed.Broadcast(bExpiredNaturally);
    OnWallCollapsed();
    Destroy();
}

void AWTBRAegornWallActor::OnRep_WallHP()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] OnRep_WallHP | Wall=%s | WallHP=%.1f | MaxWallHP=%.1f"),
        *GetNameSafe(this),
        WallHP,
        MaxWallHP);
    OnWallDamaged(WallHP, 0.0f);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] OnWallDamaged Fired | Wall=%s | Source=OnRep_WallHP"),
        *GetNameSafe(this));
}
