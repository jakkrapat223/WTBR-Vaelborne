// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRSerpveilTrigger.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "Components/WTBRVaelComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "WTBRCharacter.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"

// ─── Activate (no-op — fire is RPC-driven on release) ────────────────────────

bool UWTBRSerpveilTrigger::Activate_Implementation(
    const FInputActionValue& /*InputValue*/,
    bool /*bIsDualWield*/)
{
    // Serpveil fires on button release via Server_FireSerpveil, not here.
    return true;
}

// ─── OnTriggerActivated — client starts charge tracking ──────────────────────

void UWTBRSerpveilTrigger::OnTriggerActivated_Implementation(
    AActor* /*OwnerActor*/, bool bIsMain)
{
    if (!OwnerCharacter.IsValid()) return;

    bCachedIsMain   = bIsMain;
    bIsCharging     = true;
    ChargeStartTime = OwnerCharacter->GetWorld()->GetTimeSeconds();

    const UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    const float CurrentVael = IsValid(VaelComp) ? VaelComp->GetCurrentVael() : -1.0f;
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Serpveil SpamTest] Pressed | Owner=%s | Main=%s | Time=%.3f | CurrentVael=%.2f"),
        *GetNameSafe(OwnerCharacter.Get()),
        bIsMain ? TEXT("true") : TEXT("false"),
        ChargeStartTime,
        CurrentVael);

    if (OwnerCharacter->HasAuthority())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Serpveil] Server Charge Start | Owner=%s | Main=%s | Time=%.2f"),
            *GetNameSafe(OwnerCharacter.Get()),
            bIsMain ? TEXT("true") : TEXT("false"),
            ChargeStartTime);

        OwnerCharacter->SetSerpveilChargeTelegraphActive(true);
    }

    if (!OwnerCharacter->IsLocallyControlled()) return;

    OwnerCharacter->GetWorld()->GetTimerManager().SetTimer(
        ChargeUpdateTimer,
        this, &UWTBRSerpveilTrigger::UpdateChargeProgress,
        0.05f, /*bLoop=*/true);

    OnSerpveilChargeStart();
}

// ─── OnTriggerDeactivated — client sends charge result to server ──────────────

void UWTBRSerpveilTrigger::OnTriggerDeactivated_Implementation(
    AActor* /*OwnerActor*/, bool bIsMain)
{
    if (!OwnerCharacter.IsValid()) return;

    // Placed unconditionally so every exit path below (no-op/no-charge return,
    // and the main release-fire path) clears the telegraph — idempotent, so
    // ExecuteServerFire's own cleanup call later is a harmless no-op.
    OwnerCharacter->SetSerpveilChargeTelegraphActive(false);

    if (!bIsCharging)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] Resolved | Reason=NoRemainingAction | Trigger=Serpveil | Owner=%s | Main=%s"),
            *GetNameSafe(OwnerCharacter.Get()),
            bIsMain ? TEXT("true") : TEXT("false"));
        if (OwnerCharacter->IsLocallyControlled())
        {
            StopChargeTracking();
        }
        return;
    }

    bIsCharging = false;

    if (OwnerCharacter->IsLocallyControlled())
    {
        StopChargeTracking();
    }

    if (!IsValid(DataAsset)) return;
    const FWTBRSerpveilParams& Params = DataAsset->SerpveilParams;

    const float Elapsed     = OwnerCharacter->GetWorld()->GetTimeSeconds() - ChargeStartTime;
    const float ChargeFrac  = FMath::Clamp(Elapsed / FMath::Max(Params.SerpveilChargeTime, 0.01f), 0.0f, 1.0f);
    const float FinalRange  = FMath::Lerp(Params.SerpveilMinRange, Params.SerpveilMaxRange, ChargeFrac);
    const float CurrentTime = OwnerCharacter->GetWorld()->GetTimeSeconds();
    const UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    const float CurrentVael = IsValid(VaelComp) ? VaelComp->GetCurrentVael() : -1.0f;

    FVector  EyeLoc;
    FRotator EyeRot;
    OwnerCharacter->GetActorEyesViewPoint(EyeLoc, EyeRot);

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Serpveil SpamTest] Released | Owner=%s | Main=%s | Time=%.3f | Elapsed=%.3f | ChargeFrac=%.3f | FinalRange=%.1f | CurrentVael=%.2f"),
        *GetNameSafe(OwnerCharacter.Get()),
        bIsMain ? TEXT("true") : TEXT("false"),
        CurrentTime,
        Elapsed,
        ChargeFrac,
        FinalRange,
        CurrentVael);

    if (OwnerCharacter->HasAuthority())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Serpveil] Server Release | Owner=%s | Main=%s | Elapsed=%.2f | ChargeFrac=%.2f | FinalRange=%.1f | Direction=%s"),
            *GetNameSafe(OwnerCharacter.Get()),
            bIsMain ? TEXT("true") : TEXT("false"),
            Elapsed,
            ChargeFrac,
            FinalRange,
            *EyeRot.ToString());

        ExecuteServerFire(Params.PresetShape, EyeRot, FinalRange);
        return;
    }
}

bool UWTBRSerpveilTrigger::CancelCharge()
{
    if (!OwnerCharacter.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] Blocked | Reason=InvalidOwner | Trigger=Serpveil"));
        return false;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] Blocked | Reason=NoAuthority | Trigger=Serpveil | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }
    if (!bIsCharging)
    {
        return false;
    }

    const float CurrentTime = OwnerCharacter->GetWorld()
        ? OwnerCharacter->GetWorld()->GetTimeSeconds()
        : 0.0f;
    const float Elapsed = CurrentTime - ChargeStartTime;
    bIsCharging = false;
    StopChargeTracking();

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] SerpveilChargeCanceled | Owner=%s | Main=%s | Elapsed=%.3f | NoFire=true | NoVaelConsume=true"),
        *GetNameSafe(OwnerCharacter.Get()),
        bCachedIsMain ? TEXT("true") : TEXT("false"),
        Elapsed);

    OwnerCharacter->SetSerpveilChargeTelegraphActive(false);

    return true;
}

// ─── ExecuteServerFire — server validates Vael and spawns projectile ──────────

void UWTBRSerpveilTrigger::ExecuteServerFire(
    EWTBRSerpveilShape Shape, FRotator Direction, float ChargedRange)
{
    const UWTBRVaelComponent* StartingVaelComp =
        OwnerCharacter.IsValid() ? OwnerCharacter->VaelComponent : nullptr;
    const float StartingVael = IsValid(StartingVaelComp)
        ? StartingVaelComp->GetCurrentVael()
        : -1.0f;
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Serpveil SpamTest] ExecuteServerFire Start | Owner=%s | Shape=%d | ChargedRange=%.1f | CurrentVael=%.2f"),
        *GetNameSafe(OwnerCharacter.Get()),
        (int32)Shape,
        ChargedRange,
        StartingVael);

    if (!OwnerCharacter.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Serpveil] ExecuteServerFire Abort | Reason=Owner invalid"));
        return;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Serpveil] ExecuteServerFire Abort | Owner=%s | Reason=No authority"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }
    if (!IsValid(DataAsset))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Serpveil] ExecuteServerFire Abort | Owner=%s | Reason=DataAsset invalid"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[Serpveil] ExecuteServerFire Start | Owner=%s | Shape=%d | ChargedRange=%.1f | Direction=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        (int32)Shape,
        ChargedRange,
        *Direction.ToString());

    const FWTBRSerpveilParams& Params = DataAsset->SerpveilParams;
    UWorld* World = OwnerCharacter->GetWorld();
    if (!IsValid(World))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Serpveil] ExecuteServerFire Abort | Owner=%s | Reason=World invalid"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();
    const float SerpveilCooldown = FMath::Max(Params.SerpveilFireCooldown, 0.0f);
    const float TimeSinceLastFire = CurrentTime - LastSerpveilFireTime;
    if (TimeSinceLastFire < SerpveilCooldown)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Serpveil Cooldown] Blocked | Owner=%s | Time=%.3f | LastFire=%.3f | TimeSinceLastFire=%.3f | Cooldown=%.3f"),
            *GetNameSafe(OwnerCharacter.Get()),
            CurrentTime,
            LastSerpveilFireTime,
            TimeSinceLastFire,
            SerpveilCooldown);
        return;
    }

    if (!Params.SerpveilProjectileClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Serpveil] ExecuteServerFire Abort | Owner=%s | Reason=ProjectileClass null"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Serpveil] ExecuteServerFire Abort | Owner=%s | Reason=VaelComponent invalid"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }

    // Vael validation: how far can we afford based on remaining Vael?
    const float MaxAffordableRange =
        (VaelComp->GetCurrentVael() / Params.SerpveilVaelPerSecond)
        * (Params.SerpveilMaxRange - Params.SerpveilMinRange)
        + Params.SerpveilMinRange;

    const float ValidatedRange = FMath::Clamp(
        ChargedRange, Params.SerpveilMinRange, MaxAffordableRange);

    // Consume Vael proportional to validated range
    const float RangeSpan = Params.SerpveilMaxRange - Params.SerpveilMinRange;
    if (RangeSpan > 0.0f)
    {
        const float RangeFraction = (ValidatedRange - Params.SerpveilMinRange) / RangeSpan;
        const float VaelToConsume = RangeFraction * Params.SerpveilVaelPerSecond;
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Serpveil SpamTest] ConsumeCheck | ValidatedRange=%.1f | RangeFraction=%.3f | VaelToConsume=%.3f | CurrentVael=%.2f"),
            ValidatedRange,
            RangeFraction,
            VaelToConsume,
            VaelComp->GetCurrentVael());

        if (!VaelComp->TryConsumeVael(VaelToConsume))
        {
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Serpveil SpamTest] ConsumeFail | VaelToConsume=%.3f | CurrentVael=%.2f"),
                VaelToConsume,
                VaelComp->GetCurrentVael());

            UE_LOG(LogTemp, Warning,
                TEXT("[Serpveil] ExecuteServerFire Abort | Owner=%s | Reason=TryConsumeVael failed | VaelToConsume=%.2f | CurrentVael=%.2f | ValidatedRange=%.1f"),
                *GetNameSafe(OwnerCharacter.Get()),
                VaelToConsume,
                VaelComp->GetCurrentVael(),
                ValidatedRange);
            return;
        }

        WTBR_VALIDATION_LOG(Verbose, TEXT("[Serpveil SpamTest] ConsumeSuccess | VaelToConsume=%.3f | NewVael=%.2f"),
            VaelToConsume,
            VaelComp->GetCurrentVael());
    }

    // Build world-space path from the equipped-hand socket when available.
    static const FName HandSocketName(TEXT("hand_r"));
    FVector SpawnOrigin;
    if (const USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
        IsValid(CharacterMesh) && CharacterMesh->DoesSocketExist(HandSocketName))
    {
        SpawnOrigin = CharacterMesh->GetSocketLocation(HandSocketName);
    }
    else
    {
        FVector EyeLoc;
        FRotator EyeRot;
        OwnerCharacter->GetActorEyesViewPoint(EyeLoc, EyeRot);
        SpawnOrigin = EyeLoc + FRotationMatrix(Direction).GetScaledAxis(EAxis::X) * 100.0f;

        UE_LOG(LogTemp, Warning,
            TEXT("[Serpveil] Hand socket fallback | Owner=%s | Socket=%s | Reason=MeshMissingOrSocketAbsent"),
            *GetNameSafe(OwnerCharacter.Get()),
            *HandSocketName.ToString());
    }

    TArray<FVector> Points = BuildPathPoints(Shape, SpawnOrigin, Direction, ValidatedRange);
    if (Points.Num() < 2)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Serpveil] ExecuteServerFire Abort | Owner=%s | Reason=PathPoints < 2 | Points=%d"),
            *GetNameSafe(OwnerCharacter.Get()),
            Points.Num());
        return;
    }

    const FTransform SpawnTF(Direction, SpawnOrigin);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Serpveil SpamTest] SpawnAttempt | Points=%d | ProjectileClass=%s | Speed=%.1f | Damage=%.1f"),
        Points.Num(),
        *GetNameSafe(Params.SerpveilProjectileClass),
        Params.SerpveilSpeed,
        Params.SerpveilDamage);

    AWTBRProjectileBase* Proj = World->SpawnActorDeferred<AWTBRProjectileBase>(
        Params.SerpveilProjectileClass, SpawnTF, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Proj))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Serpveil] ExecuteServerFire Abort | Owner=%s | Reason=SpawnActorDeferred failed | Class=%s"),
            *GetNameSafe(OwnerCharacter.Get()),
            *GetNameSafe(Params.SerpveilProjectileClass));
        return;
    }

    Proj->InitializeProjectile(Params.SerpveilDamage, Params.SerpveilSpeed,
        ETriggerCategory::Gunner, false, false, 0.0f);
    Proj->FinishSpawning(SpawnTF);
    Proj->InitializePathMovement(Points, Params.SerpveilSpeed, OwnerCharacter.Get());

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Serpveil SpamTest] SpawnSuccess | Projectile=%s | Points=%d | Range=%.1f"),
        *GetNameSafe(Proj),
        Points.Num(),
        ValidatedRange);

    UE_LOG(LogTemp, Warning,
        TEXT("[Serpveil] Spawned Path Projectile | Owner=%s | Projectile=%s | Points=%d | Range=%.1f | Damage=%.1f | Speed=%.1f"),
        *GetNameSafe(OwnerCharacter.Get()),
        *GetNameSafe(Proj),
        Points.Num(),
        ValidatedRange,
        Params.SerpveilDamage,
        Params.SerpveilSpeed);

    LastSerpveilFireTime = CurrentTime;

    if (UWTBRActionPingSubsystem* PingSys = World->GetSubsystem<UWTBRActionPingSubsystem>())
        PingSys->RegisterActionPing(OwnerCharacter.Get());

    OnSerpveilFired(Shape);

    OwnerCharacter->SetSerpveilChargeTelegraphActive(false);
}

// ─── GetPreviewPathPoints (client only, for aim preview Blueprint) ────────────

TArray<FVector> UWTBRSerpveilTrigger::GetPreviewPathPoints(
    FVector StartLocation, FRotator Direction, float Range) const
{
    if (!IsValid(DataAsset)) return {};
    return BuildPathPoints(
        DataAsset->SerpveilParams.PresetShape, StartLocation, Direction, Range);
}

// ─── BuildPathPoints ──────────────────────────────────────────────────────────

TArray<FVector> UWTBRSerpveilTrigger::BuildPathPoints(
    EWTBRSerpveilShape Shape, FVector StartLocation, FRotator Direction, float Range,
    bool bMirrorY)
{
    const float R = Range;
    TArray<FVector> LocalPts;

    switch (Shape)
    {
    case EWTBRSerpveilShape::Curve:
        LocalPts = {
            FVector(0.0f,    0.0f,      0.0f),
            FVector(R*0.5f,  0.0f,      R*0.2f),
            FVector(R,       0.0f,      0.0f),
        };
        break;

    case EWTBRSerpveilShape::SCurve:
        LocalPts = {
            FVector(0.0f,    0.0f,      0.0f),
            FVector(R*0.3f,  R*0.2f,    0.0f),
            FVector(R*0.7f,  -R*0.2f,   0.0f),
            FVector(R,       0.0f,      0.0f),
        };
        break;

    case EWTBRSerpveilShape::Hook:
        LocalPts = {
            FVector(0.0f,    0.0f,      0.0f),
            FVector(R*0.5f,  0.0f,      0.0f),
            FVector(R*0.8f,  R*0.3f,    0.0f),
            FVector(R,       R*0.5f,    0.0f),
        };
        break;

    case EWTBRSerpveilShape::Spiral:
        for (int32 i = 0; i < 5; ++i)
        {
            const float Angle  = FMath::DegreesToRadians(90.0f * i);
            const float Radius = R * i * 0.25f;
            LocalPts.Add(FVector(
                Radius * FMath::Cos(Angle),
                Radius * FMath::Sin(Angle),
                0.0f));
        }
        break;

    case EWTBRSerpveilShape::Boomerang:
        LocalPts = {
            FVector(0.0f,    0.0f,      0.0f),
            FVector(R*0.5f,  R*0.4f,    0.0f),
            FVector(R,       0.0f,      0.0f),
            FVector(R*0.5f,  -R*0.4f,   0.0f),
            FVector(0.0f,    0.0f,      0.0f),
        };
        break;

    default:
        LocalPts = { FVector(0.0f, 0.0f, 0.0f), FVector(R, 0.0f, 0.0f) };
        break;
    }

    // Mirror Core B: flip Y of every local point before world-space transform
    if (bMirrorY)
    {
        for (FVector& Pt : LocalPts)
            Pt.Y = -Pt.Y;
    }

    // Transform local waypoints to world space via the fire direction
    const FRotationMatrix RotMat(Direction);
    TArray<FVector> WorldPts;
    WorldPts.Reserve(LocalPts.Num());
    for (const FVector& Local : LocalPts)
        WorldPts.Add(StartLocation + RotMat.TransformVector(Local));

    return WorldPts;
}

// ─── Charge progress helpers ──────────────────────────────────────────────────

void UWTBRSerpveilTrigger::UpdateChargeProgress()
{
    if (!OwnerCharacter.IsValid() || !IsValid(DataAsset)) return;

    const float Elapsed    = OwnerCharacter->GetWorld()->GetTimeSeconds() - ChargeStartTime;
    const float MaxTime    = FMath::Max(DataAsset->SerpveilParams.SerpveilChargeTime, 0.01f);
    const float ChargePct  = FMath::Clamp(Elapsed / MaxTime, 0.0f, 1.0f);

    OnSerpveilCharging(ChargePct);

    if (ChargePct >= 1.0f)
        StopChargeTracking();
}

void UWTBRSerpveilTrigger::StopChargeTracking()
{
    if (!OwnerCharacter.IsValid()) return;
    OwnerCharacter->GetWorld()->GetTimerManager().ClearTimer(ChargeUpdateTimer);
}
