// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Actors/WTBRNexilWireActor.h"
#include "WTBRValidationLog.h"
#include "Trigger/WTBRNexilTrigger.h"
#include "WTBRCharacter.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"

AWTBRNexilWireActor::AWTBRNexilWireActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    WireMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("WireMesh"));
    RootComponent = WireMesh;
    WireMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    WireOverlap = CreateDefaultSubobject<UBoxComponent>(
        TEXT("WireOverlap"));
    WireOverlap->SetupAttachment(RootComponent);
    WireOverlap->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WireOverlap->SetCollisionResponseToAllChannels(ECR_Ignore);
    WireOverlap->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    // Projectiles use ECC_WorldDynamic (see AWTBRProjectileBase's BoxCollision) —
    // Overlap (not Block) so gunfire can cut the wire without the wire acting
    // like a physical wall that stops bullets in flight.
    WireOverlap->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    WireOverlap->SetGenerateOverlapEvents(true);
}

void AWTBRNexilWireActor::BeginPlay()
{
    Super::BeginPlay();
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Wire BeginPlay | Wire=%s | HasAuthority=%s | Replicates=%s | OverlapEnabled=%d | GenerateOverlap=%s | Location=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        GetIsReplicated() ? TEXT("true") : TEXT("false"),
        IsValid(WireOverlap) ? static_cast<int32>(WireOverlap->GetCollisionEnabled()) : -1,
        IsValid(WireOverlap) && WireOverlap->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"),
        *GetActorLocation().ToString());
    if (HasAuthority())
    {
        WireOverlap->OnComponentBeginOverlap.AddDynamic(
            this, &AWTBRNexilWireActor::OnWireOverlapBegin);
    }
}

void AWTBRNexilWireActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWTBRNexilWireActor, bIsTriggered);
    DOREPLIFETIME(AWTBRNexilWireActor, WireHP);
    DOREPLIFETIME(AWTBRNexilWireActor, MaxWireHP);
}

void AWTBRNexilWireActor::InitializeWire(
    float InLifetime,
    float InStaggerDuration,
    float InWireLength,
    int32 InWireHP,
    UWTBRNexilTrigger* InOwnerTrigger)
{
    ensure(HasAuthority());
    StaggerDuration = InStaggerDuration;
    OwnerTrigger    = InOwnerTrigger;
    MaxWireHP       = FMath::Max(1, InWireHP);
    WireHP          = MaxWireHP;
    WireOverlap->SetBoxExtent(
        FVector(10.0f, InWireLength * 0.5f, 30.0f));
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Wire Initialized | Wire=%s | Lifetime=%.1f | Stagger=%.2f | Length=%.1f | WireHP=%d | Extent=%s | OwnerTrigger=%s"),
        *GetNameSafe(this),
        InLifetime,
        InStaggerDuration,
        InWireLength,
        WireHP,
        *WireOverlap->GetScaledBoxExtent().ToString(),
        *GetNameSafe(InOwnerTrigger));
    if (InLifetime > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            LifetimeTimer,
            this, &AWTBRNexilWireActor::OnLifetimeExpired,
            InLifetime,
            false);
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] LifetimeTimerStart | Wire=%s | Lifetime=%.1f"),
            *GetNameSafe(this),
            InLifetime);
    }
}

void AWTBRNexilWireActor::OnWireOverlapBegin(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] WireOverlap | Wire=%s | Other=%s | OtherClass=%s | HasAuthority=%s | Triggered=%s | IsInstigator=%s"),
        *GetNameSafe(this),
        *GetNameSafe(OtherActor),
        IsValid(OtherActor) ? *GetNameSafe(OtherActor->GetClass()) : TEXT("None"),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        bIsTriggered ? TEXT("true") : TEXT("false"),
        OtherActor == GetInstigator() ? TEXT("true") : TEXT("false"));

    WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] NexilOverlap | Wire=%s | OverlapActor=%s | ActorClass=%s | HasAuthority=%s | OwnerIgnored=%s | AlreadyTriggered=%s | EnemyValid=%s"),
        *GetNameSafe(this),
        *GetNameSafe(OtherActor),
        IsValid(OtherActor) ? *GetNameSafe(OtherActor->GetClass()) : TEXT("None"),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        OtherActor == GetInstigator() ? TEXT("true") : TEXT("false"),
        bIsTriggered ? TEXT("true") : TEXT("false"),
        IsValid(Cast<AWTBRCharacter>(OtherActor)) && OtherActor != GetInstigator() ? TEXT("true") : TEXT("false"));

    if (!HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] NexilRejected | Wire=%s | OverlapActor=%s | Reason=NoAuthority"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor));
        return;
    }
    if (!IsValid(OtherActor))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] NexilRejected | Wire=%s | OverlapActor=%s | Reason=InvalidActor"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor));
        return;
    }

    // Gunfire/explosion contact cuts the wire (GDD: "ฟันหรือยิงขาดได้") regardless
    // of team or trip-state — checked before bIsTriggered/instigator/team gates
    // below, which are trip-mechanic-only concerns.
    if (AWTBRProjectileBase* Projectile = Cast<AWTBRProjectileBase>(OtherActor))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] WireHitByProjectile | Wire=%s | Projectile=%s | WireHPBefore=%d"),
            *GetNameSafe(this),
            *GetNameSafe(Projectile),
            WireHP);
        TakeHit();
        return;
    }

    if (bIsTriggered)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] NexilRejected | Wire=%s | OverlapActor=%s | Reason=AlreadyTriggered"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor));
        return;
    }

    // TD Fix 2: Self-Stagger check — instigator cannot trip own wire
    if (OtherActor == GetInstigator())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] NexilRejected | Wire=%s | OverlapActor=%s | Reason=OwnerIgnored"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor));
        return;
    }

    AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(OtherActor);
    if (!IsValid(HitChar))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=OverlapNotCharacter | Other=%s"),
            *GetNameSafe(OtherActor));
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] NexilRejected | Wire=%s | OverlapActor=%s | Reason=OverlapNotCharacter"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor));
        return;
    }

    // Friendly-fire fix: teammates of the wire's owner must not trip it.
    AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetInstigator());
    if (IsValid(OwnerChar) && OwnerChar->IsSameTeamAs(HitChar))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] NexilRejected | Wire=%s | OverlapActor=%s | Reason=SameTeam"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor));
        return;
    }

    TriggerAndDestroy(OtherActor);
}

void AWTBRNexilWireActor::TakeHit()
{
    if (!HasAuthority()) return;
    if (bIsTriggered) return; // already gone via trip path
    if (WireHP <= 0) return;

    const int32 OldHP = WireHP;
    WireHP = FMath::Max(0, WireHP - 1);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] WireHPDamage | Wire=%s | OldHP=%d | NewHP=%d"),
        *GetNameSafe(this),
        OldHP,
        WireHP);

    if (WireHP <= 0)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] WireDestroyedByDamage | Wire=%s"),
            *GetNameSafe(this));
        OnWireDestroyedByDamageVFX();
        GetWorld()->GetTimerManager().ClearTimer(LifetimeTimer);
        Destroy();
    }
}

float AWTBRNexilWireActor::GetWireHPPercent() const
{
    if (MaxWireHP <= 0) return 0.0f;
    return static_cast<float>(WireHP) / static_cast<float>(MaxWireHP);
}

void AWTBRNexilWireActor::TriggerAndDestroy(AActor* TriggeredBy)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Wire Triggered | Wire=%s | TriggeredBy=%s"),
        *GetNameSafe(this),
        *GetNameSafe(TriggeredBy));
    WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] NexilTriggered | Wire=%s | TriggeredBy=%s | CleanupReason=TriggeredOverlap"),
        *GetNameSafe(this),
        *GetNameSafe(TriggeredBy));
    bIsTriggered = true;
    ApplyStaggerToCharacter(TriggeredBy);
    if (OwnerTrigger.IsValid())
        OwnerTrigger->NotifyWireTriggered(this);
    GetWorld()->GetTimerManager().ClearTimer(LifetimeTimer);
    Destroy();
}

void AWTBRNexilWireActor::ApplyStaggerToCharacter(AActor* TargetActor)
{
    AWTBRCharacter* TargetChar = Cast<AWTBRCharacter>(TargetActor);
    if (!IsValid(TargetChar))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=StaggerTargetInvalid | Target=%s"),
            *GetNameSafe(TargetActor));
        return;
    }
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] StaggerApplied | Target=%s | Duration=%.2f"),
        *GetNameSafe(TargetChar),
        StaggerDuration);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] NexilStaggerApplied | Target=%s | Duration=%.2f | StaggerApplied=true"),
        *GetNameSafe(TargetChar),
        StaggerDuration);
    TargetChar->ApplyStagger(StaggerDuration);
}

void AWTBRNexilWireActor::OnLifetimeExpired()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] LifetimeExpired | Wire=%s"),
        *GetNameSafe(this));
    Destroy();
}

void AWTBRNexilWireActor::OnRep_bIsTriggered()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] OnRep_bIsTriggered | Wire=%s | Triggered=%s"),
        *GetNameSafe(this),
        bIsTriggered ? TEXT("true") : TEXT("false"));
    if (bIsTriggered)
        OnWireTriggeredVFX();
}

void AWTBRNexilWireActor::OnRep_WireHP()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] OnRep_WireHP | Wire=%s | WireHP=%d | MaxWireHP=%d"),
        *GetNameSafe(this),
        WireHP,
        MaxWireHP);
}
