// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRFeryxTrigger.h"
#include "WTBRValidationLog.h"
#include "WTBRCharacter.h"
#include "Actors/WTBRProjectileBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UWTBRFeryxTrigger::ResolveTap(bool bIsDualWield)
{
    bHoldPending = false;
    UWTBRMeleeTrigger::Activate_Implementation(FInputActionValue(), bIsDualWield);
}

bool UWTBRFeryxTrigger::Activate_Implementation(const FInputActionValue& /*InputValue*/, bool bIsDualWield)
{
    // Resolve on release so a single button can distinguish a tap from a hold.
    bPendingDualWield = bIsDualWield;
    return true;
}

void UWTBRFeryxTrigger::OnTriggerActivated_Implementation(AActor* /*OwnerActor*/, bool /*bIsMain*/)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority() || !GetWorld()) return;
    HoldStartTime = GetWorld()->GetTimeSeconds();
    bHoldPending = true;
}

void UWTBRFeryxTrigger::OnTriggerDeactivated_Implementation(AActor* /*OwnerActor*/, bool /*bIsMain*/)
{
    if (!bHoldPending || !OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()
        || !GetWorld() || !IsValid(DataAsset)) return;

    bHoldPending = false;
    const float HeldFor = GetWorld()->GetTimeSeconds() - HoldStartTime;
    if (HeldFor >= DataAsset->FeryxParams.HoldThreshold)
    {
        ThrowBladeStars();
        return;
    }
    ResolveTap(bPendingDualWield);
}

void UWTBRFeryxTrigger::ThrowBladeStars()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority() || !GetWorld() || !IsValid(DataAsset)) return;

    const FWTBRFeryxParams& Params = DataAsset->FeryxParams;
    const int32 Count = Params.StarCount;
    if (Count <= 0 || bStarOnCooldown) return;

    FVector EyeLocation;
    FRotator AimRotation;
    OwnerCharacter->GetActorEyesViewPoint(EyeLocation, AimRotation);
    const float Step = Count > 1 ? Params.StarSpreadDegrees / (Count - 1) : 0.0f;
    const TSubclassOf<AWTBRProjectileBase> ProjectileClass = Params.StarProjectileClass
        ? Params.StarProjectileClass : AWTBRProjectileBase::StaticClass();

    for (int32 Index = 0; Index < Count; ++Index)
    {
        FRotator StarRotation = AimRotation;
        StarRotation.Yaw += -Params.StarSpreadDegrees * 0.5f + Step * Index;
        const FVector SpawnLocation = EyeLocation + StarRotation.Vector() * 80.0f;
        AWTBRProjectileBase* Star = GetWorld()->SpawnActorDeferred<AWTBRProjectileBase>(
            ProjectileClass, FTransform(StarRotation, SpawnLocation), OwnerCharacter.Get(), OwnerCharacter.Get(),
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!IsValid(Star)) continue;

        Star->MaxRange = Params.StarRange;
        Star->InitializeProjectile(Params.StarDamage, Params.StarSpeed,
            ETriggerCategory::Melee, false, false, 0.0f);
        Star->ConfigureOnHitEffects(Params.BleedDamagePerTick, Params.BleedDuration,
            Params.BrittleDamageMultiplier, Params.BrittleDuration);
        Star->FinishSpawning(FTransform(StarRotation, SpawnLocation));
        Star->Launch(StarRotation.Vector(), OwnerCharacter.Get());
    }

    bStarOnCooldown = true;
    GetWorld()->GetTimerManager().SetTimer(StarCooldownTimer, this,
        &UWTBRFeryxTrigger::ClearStarCooldown,
        FMath::Max(DataAsset->MeleeHitbox.SwingCooldown, 0.05f), false);
}

void UWTBRFeryxTrigger::ClearStarCooldown()
{
    bStarOnCooldown = false;
}

void UWTBRFeryxTrigger::PerformSingleSweep(TArray<FHitResult>& OutHits)
{
    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->HasAuthority())
    {
        OutHits.Reset();
        UE_LOG(LogTemp, Warning,
            TEXT("[Feryx Sweep] Owner=%s | Auth=false | Dual=false | RawHits=0"),
            *GetNameSafe(OwnerCharacter.Get()));
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Feryx Test] Sweep | Owner=%s | Auth=false | Dual=false | RawHits=0 | Result=Rejected"),
            *GetNameSafe(OwnerCharacter.Get()));
        OnFeryxSwing(false, false);
        return;
    }

    Super::PerformSingleSweep(OutHits);
    UE_LOG(LogTemp, Warning,
        TEXT("[Feryx Sweep] Owner=%s | Auth=true | Dual=false | RawHits=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        OutHits.Num());
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Feryx Test] Sweep | Owner=%s | Auth=true | Dual=false | RawHits=%d | Result=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        OutHits.Num(),
        OutHits.Num() > 0 ? TEXT("Hit") : TEXT("Miss"));
    OnFeryxSwing(false, OutHits.Num() > 0);
    for (const FHitResult& Hit : OutHits)
    {
        if (IsValid(DataAsset))
        {
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Feryx Test] HitAttempt | Owner=%s | Target=%s | Damage=%.1f | Dual=false"),
                *GetNameSafe(OwnerCharacter.Get()),
                *GetNameSafe(Hit.GetActor()),
                DataAsset->BaseDamage);
            OnFeryxHit(Hit, DataAsset->BaseDamage);
        }
    }
}

void UWTBRFeryxTrigger::PerformDualSweep(TArray<FHitResult>& OutHits)
{
    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->HasAuthority())
    {
        OutHits.Reset();
        UE_LOG(LogTemp, Warning,
            TEXT("[Feryx Sweep] Owner=%s | Auth=false | Dual=true | RawHits=0"),
            *GetNameSafe(OwnerCharacter.Get()));
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Feryx Test] Sweep | Owner=%s | Auth=false | Dual=true | RawHits=0 | Result=Rejected"),
            *GetNameSafe(OwnerCharacter.Get()));
        OnFeryxSwing(true, false);
        return;
    }

    Super::PerformDualSweep(OutHits);
    UE_LOG(LogTemp, Warning,
        TEXT("[Feryx Sweep] Owner=%s | Auth=true | Dual=true | RawHits=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        OutHits.Num());
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Feryx Test] Sweep | Owner=%s | Auth=true | Dual=true | RawHits=%d | Result=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        OutHits.Num(),
        OutHits.Num() > 0 ? TEXT("Hit") : TEXT("Miss"));
    OnFeryxSwing(true, OutHits.Num() > 0);
    for (const FHitResult& Hit : OutHits)
    {
        if (IsValid(DataAsset))
        {
            const float DualDmg = DataAsset->BaseDamage
                * DataAsset->DualWieldStats.DamageMultiplier;
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Feryx Test] HitAttempt | Owner=%s | Target=%s | Damage=%.1f | Dual=true"),
                *GetNameSafe(OwnerCharacter.Get()),
                *GetNameSafe(Hit.GetActor()),
                DualDmg);
            OnFeryxHit(Hit, DualDmg);
        }
    }
}
