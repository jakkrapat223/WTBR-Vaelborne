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
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority() || bSpent) return;

    if (CurrentMode == EWTBRFeryxMode::Throw)
    {
        ThrowBladeStars();
        return;
    }

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
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority() || !GetWorld() || bSpent) return;
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
        // A hold is resolved by the owning client's radial wheel through
        // ResolveFormSelection. Releasing without a selection is a no-op.
        return;
    }
    ResolveTap(bPendingDualWield);
}

void UWTBRFeryxTrigger::OnEquipped()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReconjureTimer);
    }
    bSpent = false;
    CurrentMode = EWTBRFeryxMode::ShortBlade;
}

bool UWTBRFeryxTrigger::ResolveFormSelection(
    EWTBRFeryxMode RequestedMode, bool bHasSelection)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;

    if (!bHoldPending || !GetWorld() || !IsValid(DataAsset)) return false;

    const float HeldFor = GetWorld()->GetTimeSeconds() - HoldStartTime;
    bHoldPending = false;
    if (!bHasSelection || bSpent || !IsSupportedMode(RequestedMode)
        || HeldFor < DataAsset->FeryxParams.HoldThreshold)
    {
        return false;
    }

    CurrentMode = RequestedMode;
    return true;
}

bool UWTBRFeryxTrigger::IsSupportedMode(EWTBRFeryxMode Mode)
{
    return Mode == EWTBRFeryxMode::ShortBlade
        || Mode == EWTBRFeryxMode::Throw;
}

void UWTBRFeryxTrigger::ThrowBladeStars()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority() || !GetWorld() || !IsValid(DataAsset)
        || bSpent) return;

    const FWTBRFeryxParams& Params = DataAsset->FeryxParams;
    FVector EyeLocation;
    FRotator AimRotation;
    OwnerCharacter->GetActorEyesViewPoint(EyeLocation, AimRotation);
    const TSubclassOf<AWTBRProjectileBase> ProjectileClass = Params.StarProjectileClass
        ? Params.StarProjectileClass : AWTBRProjectileBase::StaticClass();

    // One large blade-star per throw — not a multi-projectile fan spread.
    const FVector SpawnLocation = EyeLocation + AimRotation.Vector() * 80.0f;
    AWTBRProjectileBase* Star = GetWorld()->SpawnActorDeferred<AWTBRProjectileBase>(
        ProjectileClass, FTransform(AimRotation, SpawnLocation), OwnerCharacter.Get(), OwnerCharacter.Get(),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (IsValid(Star))
    {
        Star->MaxRange = Params.StarRange;
        Star->InitializeProjectile(Params.StarDamage, Params.StarSpeed,
            ETriggerCategory::Melee, false, false, 0.0f);
        Star->ConfigureOnHitEffects(Params.BleedDamagePerTick, Params.BleedDuration,
            Params.BrittleDamageMultiplier, Params.BrittleDuration);
        Star->FinishSpawning(FTransform(AimRotation, SpawnLocation));
        Star->Launch(AimRotation.Vector(), OwnerCharacter.Get());
    }
    else
    {
        return;
    }

    bSpent = true;
    const float ReconjureSeconds = FMath::Max(Params.ReconjureSeconds, 0.0f);
    if (ReconjureSeconds <= 0.0f)
    {
        CompleteReconjure();
        return;
    }
    GetWorld()->GetTimerManager().SetTimer(
        ReconjureTimer, this, &UWTBRFeryxTrigger::CompleteReconjure, ReconjureSeconds, false);
}

void UWTBRFeryxTrigger::CompleteReconjure()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return;

    // TODO: real conjure system replaces this placeholder
    bSpent = false;
    CurrentMode = EWTBRFeryxMode::ShortBlade;
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
