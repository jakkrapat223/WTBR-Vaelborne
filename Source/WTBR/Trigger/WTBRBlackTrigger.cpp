// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRBlackTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Actors/WTBRProjectileBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

FVector UWTBRBlackTrigger::GetFireDirection() const
{
    return OwnerCharacter->GetController()
        ? OwnerCharacter->GetController()->GetControlRotation().Vector()
        : OwnerCharacter->GetActorForwardVector();
}

FVector UWTBRBlackTrigger::GetMuzzleLocation(const FVector& AimDirection) const
{
    FVector EyeLoc;
    FRotator EyeRot;
    OwnerCharacter->GetActorEyesViewPoint(EyeLoc, EyeRot);
    return EyeLoc + (AimDirection * 100.0f);
}

AWTBRProjectileBase* UWTBRBlackTrigger::FireBlackProjectile(
    TSubclassOf<AWTBRProjectileBase> ProjClass,
    float Damage,
    float Speed,
    float KnockbackForce,
    int32 CubeSplitCount)
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

    Proj->CubeSplitCount  = CubeSplitCount;
    Proj->KnockbackForce  = KnockbackForce;
    Proj->InitializeProjectile(Damage, Speed,
        ETriggerCategory::BlackTrigger,
        false,   // bIsSniper = false
        false,   // bExplode = false
        0.0f);
    UGameplayStatics::FinishSpawningActor(Proj, FTransform(Dir.Rotation(), Loc));
    Proj->Launch(Dir, OwnerCharacter.Get());
    OnBlackTriggerFired(Dir);
    return Proj;
}

void UWTBRBlackTrigger::StartCooldown()
{
    bIsOnCooldown = true;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            CooldownTimer,
            this, &UWTBRBlackTrigger::OnCooldownEnd,
            GetCooldownDuration(),
            false);
    }
}

void UWTBRBlackTrigger::OnCooldownEnd()
{
    bIsOnCooldown = false;
}
