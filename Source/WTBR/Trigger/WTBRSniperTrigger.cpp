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
    if (!OwnerCharacter.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Fulgris Test] Fail | Reason=OwnerInvalid"));
        return nullptr;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Fulgris Test] Fail | Reason=NoAuthority"));
        return nullptr;
    }
    if (!IsValid(ProjClass))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Fulgris Test] Fail | Reason=ProjectileClassNull"));
        return nullptr;
    }
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Fulgris Test] Fail | Reason=WorldInvalid"));
        return nullptr;
    }

    FVector Dir = GetFireDirection();
    FVector Loc = GetMuzzleLocation(Dir);
    const FRotator SpawnRot = Dir.Rotation();

    UE_LOG(LogTemp, Warning,
        TEXT("[Fulgris Test] ProjectileSpawnAttempt | Class=%s | Location=%s | Rotation=%s | Speed=%.1f | Damage=%.1f"),
        *GetNameSafe(ProjClass.Get()),
        *Loc.ToString(),
        *SpawnRot.ToString(),
        Speed,
        Damage);

    AWTBRProjectileBase* Proj = World->SpawnActorDeferred<AWTBRProjectileBase>(
        ProjClass,
        FTransform(SpawnRot, Loc),
        OwnerCharacter.Get(), OwnerCharacter.Get(),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Proj))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Fulgris Test] Fail | Reason=SpawnFailed"));
        return nullptr;
    }

    Proj->MaxRange      = Range;
    Proj->CubeSplitCount = CubeSplitCount;
    Proj->InitializeProjectile(Damage, Speed,
        ETriggerCategory::SniperBullet,
        true,    // bSniper = true
        false,   // bExplode = false
        0.0f);
    Proj->bCanPenetrate = bCanPenetrate;
    UGameplayStatics::FinishSpawningActor(Proj, FTransform(SpawnRot, Loc));
    UE_LOG(LogTemp, Warning,
        TEXT("[Fulgris Test] ProjectileSpawnSuccess | Projectile=%s"),
        *GetNameSafe(Proj));
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
