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
    if (InLifetime > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            LifetimeTimer,
            this, &AWTBRNexilWireActor::OnLifetimeExpired,
            InLifetime,
            false);
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
    if (!HasAuthority()) return;
    if (bIsTriggered) return;
    if (!IsValid(OtherActor)) return;

    // TD Fix 2: Self-Stagger check — instigator cannot trip own wire
    if (OtherActor == GetInstigator()) return;

    AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(OtherActor);
    if (!IsValid(HitChar)) return;

    TriggerAndDestroy(OtherActor);
}

void AWTBRNexilWireActor::TriggerAndDestroy(AActor* TriggeredBy)
{
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
    if (!IsValid(TargetChar)) return;
    TargetChar->ApplyStagger(StaggerDuration);
}

void AWTBRNexilWireActor::OnLifetimeExpired()
{
    Destroy();
}

void AWTBRNexilWireActor::OnRep_bIsTriggered()
{
    if (bIsTriggered)
        OnWireTriggeredVFX();
}
