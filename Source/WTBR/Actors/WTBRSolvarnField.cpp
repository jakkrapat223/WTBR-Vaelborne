// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Actors/WTBRSolvarnField.h"
#include "Components/SphereComponent.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Actors/WTBRProjectileBase.h"

AWTBRSolvarnField::AWTBRSolvarnField()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
    RootComponent = SphereCollision;
    SphereCollision->SetSphereRadius(400.0f);
    SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SphereCollision->SetCollisionObjectType(ECC_WorldDynamic);
    SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    SphereCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    SphereCollision->SetGenerateOverlapEvents(true);
}

void AWTBRSolvarnField::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        SphereCollision->OnComponentBeginOverlap.AddDynamic(
            this, &AWTBRSolvarnField::OnOverlapBegin);

        if (VaelDrainPerSec > 0.0f)
        {
            GetWorld()->GetTimerManager().SetTimer(
                DrainTimer, this, &AWTBRSolvarnField::DrainVael, 1.0f, true);
        }
    }
    OnSolvarnActivated();
}

void AWTBRSolvarnField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
        GetWorld()->GetTimerManager().ClearTimer(DrainTimer);

    OnSolvarnDeactivated();
    Super::EndPlay(EndPlayReason);
}

void AWTBRSolvarnField::InitializeField(AWTBRCharacter* InOwner, float InRadius,
    float InDuration, float InVaelDrainPerSec)
{
    OwnerChar       = InOwner;
    VaelDrainPerSec = InVaelDrainPerSec;

    if (InRadius > 0.0f)
        SphereCollision->SetSphereRadius(InRadius);

    if (InDuration > 0.0f)
        SetLifeSpan(InDuration);
}

void AWTBRSolvarnField::DrainVael()
{
    if (!HasAuthority()) return;
    if (!OwnerChar.IsValid()) { Destroy(); return; }

    UWTBRVaelComponent* Vael = OwnerChar->VaelComponent;
    if (!IsValid(Vael) || !Vael->TryConsumeVael(VaelDrainPerSec))
        Destroy(); // Vael exhausted — field collapses
}

void AWTBRSolvarnField::OnOverlapBegin(UPrimitiveComponent* /*OverlappedComp*/,
    AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/,
    int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
    if (!HasAuthority()) return;
    if (!IsValid(OtherActor)) return;

    if (AWTBRProjectileBase* Proj = Cast<AWTBRProjectileBase>(OtherActor))
    {
        OnSolvarnBlockedProjectile();
        Proj->Destroy();
    }
}
