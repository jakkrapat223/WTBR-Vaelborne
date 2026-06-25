// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRSniperTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Actors/WTBRProjectileBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

FVector UWTBRSniperTrigger::GetFireDirection() const
{
    return OwnerCharacter->GetController()
        ? OwnerCharacter->GetController()->GetControlRotation().Vector()
        : OwnerCharacter->GetActorForwardVector();
}

FVector UWTBRSniperTrigger::GetMuzzleLocation(const FVector& AimDirection) const
{
    FVector EyeLoc;
    FRotator EyeRot;
    OwnerCharacter->GetActorEyesViewPoint(EyeLoc, EyeRot);
    return EyeLoc + (AimDirection * 100.0f);
}

AWTBRProjectileBase* UWTBRSniperTrigger::FireSniper(
    TSubclassOf<AWTBRProjectileBase> ProjClass,
    float Damage, float Speed, float Range,
    bool bCanPenetrate, int32 CubeSplitCount)
{
    if (!IsValid(ProjClass)) return nullptr;

    FVector Dir = GetFireDirection();
    FVector Loc = GetMuzzleLocation(Dir);

    AWTBRProjectileBase* Proj = GetWorld()->SpawnActorDeferred<AWTBRProjectileBase>(
        ProjClass,
        FTransform(Dir.Rotation(), Loc),
        OwnerCharacter.Get(), OwnerCharacter.Get(),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Proj)) return nullptr;

    Proj->MaxRange      = Range;
    Proj->CubeSplitCount = CubeSplitCount;
    Proj->InitializeProjectile(Damage, Speed,
        ETriggerCategory::SniperBullet,
        true,    // bSniper = true
        false,   // bExplode = false
        0.0f);
    Proj->bCanPenetrate = bCanPenetrate;
    UGameplayStatics::FinishSpawningActor(Proj, FTransform(Dir.Rotation(), Loc));
    Proj->Launch(Dir, OwnerCharacter.Get());
    OnSniperFired(Dir);
    return Proj;
}

void UWTBRSniperTrigger::StartCooldown()
{
    bIsOnCooldown = true;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            CooldownTimer,
            this, &UWTBRSniperTrigger::OnCooldownExpired,
            GetCooldownDuration(),
            false);
    }
}

void UWTBRSniperTrigger::OnCooldownExpired()
{
    bIsOnCooldown = false;
}
