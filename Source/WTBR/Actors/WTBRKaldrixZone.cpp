// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Actors/WTBRKaldrixZone.h"
#include "WTBRCharacter.h"
#include "Net/UnrealNetwork.h"

AWTBRKaldrixZone::AWTBRKaldrixZone()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
}

void AWTBRKaldrixZone::BeginPlay()
{
    Super::BeginPlay();

    // Initial placed VFX fires on both server and client when actor first appears
    OnKaldrixPlaced();

    if (HasAuthority())
    {
        GetWorld()->GetTimerManager().SetTimer(
            ArmTimer, this, &AWTBRKaldrixZone::Arm, KaldrixArmTime, false);
    }
}

void AWTBRKaldrixZone::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWTBRKaldrixZone, KaldrixState);
}

void AWTBRKaldrixZone::InitializeZone(float InDamage, float InRadius,
    float InArmTime, float InStaggerDuration, AWTBRCharacter* InInstigator)
{
    KaldrixDamage          = InDamage;
    KaldrixRadius          = InRadius;
    KaldrixArmTime         = InArmTime;
    KaldrixStaggerDuration = InStaggerDuration;
    InstigatorChar         = InInstigator;
}

void AWTBRKaldrixZone::Arm()
{
    if (!HasAuthority()) return;

    KaldrixState = EKaldrixState::Armed;
    OnKaldrixArmed(); // server VFX — clients get OnRep → OnKaldrixArmed()

    Explode();
}

void AWTBRKaldrixZone::Explode()
{
    if (!HasAuthority()) return;

    KaldrixState = EKaldrixState::Exploded;

    const FVector Center = GetActorLocation();
    TArray<FHitResult> Hits;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    if (InstigatorChar.IsValid())
        QueryParams.AddIgnoredActor(InstigatorChar.Get()); // no self-damage

    GetWorld()->SweepMultiByChannel(
        Hits, Center, Center, FQuat::Identity,
        ECC_Pawn, FCollisionShape::MakeSphere(KaldrixRadius), QueryParams);

    TSet<AWTBRCharacter*> DamagedChars;
    for (const FHitResult& Hit : Hits)
    {
        AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(Hit.GetActor());
        if (!IsValid(HitChar) || DamagedChars.Contains(HitChar)) continue;
        DamagedChars.Add(HitChar);

        if (IsValid(HitChar->HealthComponent))
            HitChar->HealthComponent->ApplyDamage(KaldrixDamage, InstigatorChar.Get());

        HitChar->ApplyStagger(KaldrixStaggerDuration);
    }

    OnKaldrixExplode(); // server VFX — clients get OnRep → OnKaldrixExplode()
    Destroy();
}

void AWTBRKaldrixZone::OnRep_KaldrixState()
{
    switch (KaldrixState)
    {
        case EKaldrixState::Placed:   OnKaldrixPlaced();  break;
        case EKaldrixState::Armed:    OnKaldrixArmed();   break;
        case EKaldrixState::Exploded: OnKaldrixExplode(); break;
    }
}
