// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Actors/WTBRNexilWireActor.h"
#include "Trigger/WTBRNexilTrigger.h"
#include "WTBRCharacter.h"
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
    WireOverlap->SetGenerateOverlapEvents(true);
}

void AWTBRNexilWireActor::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning,
        TEXT("[Nexil Test] Wire BeginPlay | Wire=%s | HasAuthority=%s | Replicates=%s | OverlapEnabled=%d | GenerateOverlap=%s | Location=%s"),
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
}

void AWTBRNexilWireActor::InitializeWire(
    float InLifetime,
    float InStaggerDuration,
    float InWireLength,
    UWTBRNexilTrigger* InOwnerTrigger)
{
    ensure(HasAuthority());
    StaggerDuration = InStaggerDuration;
    OwnerTrigger    = InOwnerTrigger;
    WireOverlap->SetBoxExtent(
        FVector(10.0f, InWireLength * 0.5f, 30.0f));
    UE_LOG(LogTemp, Warning,
        TEXT("[Nexil Test] Wire Initialized | Wire=%s | Lifetime=%.1f | Stagger=%.2f | Length=%.1f | Extent=%s | OwnerTrigger=%s"),
        *GetNameSafe(this),
        InLifetime,
        InStaggerDuration,
        InWireLength,
        *WireOverlap->GetScaledBoxExtent().ToString(),
        *GetNameSafe(InOwnerTrigger));
    if (InLifetime > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            LifetimeTimer,
            this, &AWTBRNexilWireActor::OnLifetimeExpired,
            InLifetime,
            false);
        UE_LOG(LogTemp, Warning,
            TEXT("[Nexil Test] LifetimeTimerStart | Wire=%s | Lifetime=%.1f"),
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
    UE_LOG(LogTemp, Warning,
        TEXT("[Nexil Test] WireOverlap | Wire=%s | Other=%s | OtherClass=%s | HasAuthority=%s | Triggered=%s | IsInstigator=%s"),
        *GetNameSafe(this),
        *GetNameSafe(OtherActor),
        IsValid(OtherActor) ? *GetNameSafe(OtherActor->GetClass()) : TEXT("None"),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        bIsTriggered ? TEXT("true") : TEXT("false"),
        OtherActor == GetInstigator() ? TEXT("true") : TEXT("false"));

    if (!HasAuthority()) return;
    if (bIsTriggered) return;
    if (!IsValid(OtherActor)) return;

    // TD Fix 2: Self-Stagger check — instigator cannot trip own wire
    if (OtherActor == GetInstigator()) return;

    AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(OtherActor);
    if (!IsValid(HitChar))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Nexil Test] Fail | Reason=OverlapNotCharacter | Other=%s"),
            *GetNameSafe(OtherActor));
        return;
    }

    TriggerAndDestroy(OtherActor);
}

void AWTBRNexilWireActor::TriggerAndDestroy(AActor* TriggeredBy)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Nexil Test] Wire Triggered | Wire=%s | TriggeredBy=%s"),
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
        UE_LOG(LogTemp, Warning,
            TEXT("[Nexil Test] Fail | Reason=StaggerTargetInvalid | Target=%s"),
            *GetNameSafe(TargetActor));
        return;
    }
    UE_LOG(LogTemp, Warning,
        TEXT("[Nexil Test] StaggerApplied | Target=%s | Duration=%.2f"),
        *GetNameSafe(TargetChar),
        StaggerDuration);
    TargetChar->ApplyStagger(StaggerDuration);
}

void AWTBRNexilWireActor::OnLifetimeExpired()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Nexil Test] LifetimeExpired | Wire=%s"),
        *GetNameSafe(this));
    Destroy();
}

void AWTBRNexilWireActor::OnRep_bIsTriggered()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Nexil Test] OnRep_bIsTriggered | Wire=%s | Triggered=%s"),
        *GetNameSafe(this),
        bIsTriggered ? TEXT("true") : TEXT("false"));
    if (bIsTriggered)
        OnWireTriggeredVFX();
}
