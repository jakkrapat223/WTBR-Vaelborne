// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRSniperTrigger.h"
#include "WTBRValidationLog.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Actors/WTBRProjectileBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

bool UWTBRSniperTrigger::Activate_Implementation(
    const FInputActionValue& InputValue, bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown()) return false;

    // Don't fire yet — just start aiming. ExecuteFire runs on release.
    bIsAiming = true;
    return true;
}

void UWTBRSniperTrigger::OnReleased_Implementation(
    const FInputActionValue& InputValue, bool bIsDualWield)
{
    if (!bIsAiming) return;
    bIsAiming = false;

    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return;

    if (ExecuteFire())
    {
        StartCooldown();
    }
}

void UWTBRSniperTrigger::Deactivate_Implementation()
{
    bIsAiming = false;
    Super::Deactivate_Implementation();
}

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
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=OwnerInvalid"));
        return nullptr;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=NoAuthority"));
        return nullptr;
    }
    if (!IsValid(ProjClass))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=ProjectileClassNull"));
        return nullptr;
    }
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=WorldInvalid"));
        return nullptr;
    }

    FVector Dir = GetFireDirection();
    FVector Loc = GetMuzzleLocation(Dir);
    const FRotator SpawnRot = Dir.Rotation();

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] ProjectileSpawnAttempt | Class=%s | Location=%s | Rotation=%s | Speed=%.1f | Damage=%.1f"),
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
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=SpawnFailed"));
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
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] ProjectileSpawnSuccess | Projectile=%s"),
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
