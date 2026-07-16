// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRSerpveilTrigger.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "WTBRCharacter.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"

void UWTBRSerpveilTrigger::InitializeTrigger(
    AWTBRCharacter* InOwnerCharacter,
    UWTBRTriggerDataAsset* InDataAsset)
{
    Super::InitializeTrigger(InOwnerCharacter, InDataAsset);

    if (IsValid(InOwnerCharacter) && IsValid(InOwnerCharacter->HealthComponent))
    {
        InOwnerCharacter->HealthComponent->OnDeath.AddUniqueDynamic(
            this, &UWTBRSerpveilTrigger::HandleOwnerDeath);
    }
}

// ─── Activate (no-op — fire is RPC-driven on release) ────────────────────────

bool UWTBRSerpveilTrigger::Activate_Implementation(
    const FInputActionValue& /*InputValue*/,
    bool /*bIsDualWield*/)
{
    // ExecuteServerTriggerInput calls OnTriggerActivated before Activate.
    return true;
}

// ─── OnTriggerActivated — client starts charge tracking ──────────────────────

void UWTBRSerpveilTrigger::OnTriggerActivated_Implementation(
    AActor* /*OwnerActor*/, bool bIsMain)
{
    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->HasAuthority()) return;

    UWorld* World = OwnerCharacter->GetWorld();
    if (!IsValid(World) || !IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[Serpveil S1] WindupBlocked | Owner=%s | Main=%s | Reason=%s"),
            *GetNameSafe(OwnerCharacter.Get()),
            bIsMain ? TEXT("true") : TEXT("false"),
            !IsValid(World) ? TEXT("WorldInvalid") : TEXT("DataAssetInvalid"));
        OwnerCharacter->SetSerpveilChargeTelegraphActive(false);
        return;
    }

    if (!IsValid(OwnerCharacter->HealthComponent)
        || !OwnerCharacter->HealthComponent->IsAlive())
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[Serpveil S1] WindupBlocked | Owner=%s | Main=%s | Reason=OwnerNotAlive"),
            *GetNameSafe(OwnerCharacter.Get()),
            bIsMain ? TEXT("true") : TEXT("false"));
        OwnerCharacter->SetSerpveilChargeTelegraphActive(false);
        return;
    }

    if (World->GetTimerManager().IsTimerActive(WindupTimer) || bIsCharging)
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[Serpveil S1] WindupBlocked | Owner=%s | Main=%s | Reason=AlreadyWindingUp"),
            *GetNameSafe(OwnerCharacter.Get()),
            bIsMain ? TEXT("true") : TEXT("false"));
        return;
    }

    const FWTBRSerpveilParams& Params = DataAsset->SerpveilParams;
    const float CurrentTime = World->GetTimeSeconds();
    const float Cooldown = FMath::Max(Params.SerpveilFireCooldown, 0.0f);
    const float TimeSinceLastFire = CurrentTime - LastSerpveilFireTime;
    if (TimeSinceLastFire < Cooldown)
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[Serpveil S1] WindupBlocked | Owner=%s | Main=%s | Reason=Cooldown | TimeSinceLastFire=%.3f | Cooldown=%.3f"),
            *GetNameSafe(OwnerCharacter.Get()),
            bIsMain ? TEXT("true") : TEXT("false"),
            TimeSinceLastFire,
            Cooldown);
        OwnerCharacter->SetSerpveilChargeTelegraphActive(false);
        return;
    }

    const UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    const float VaelCost = FMath::Max(Params.SerpveilVaelCostPerShot, 0.0f);
    const float CurrentVael = IsValid(VaelComp) ? VaelComp->GetCurrentVael() : -1.0f;
    if (!IsValid(VaelComp) || CurrentVael < VaelCost)
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[Serpveil S1] WindupBlocked | Owner=%s | Main=%s | Reason=%s | RequiredVael=%.2f | CurrentVael=%.2f"),
            *GetNameSafe(OwnerCharacter.Get()),
            bIsMain ? TEXT("true") : TEXT("false"),
            IsValid(VaelComp) ? TEXT("InsufficientVael") : TEXT("VaelComponentInvalid"),
            VaelCost,
            CurrentVael);
        OwnerCharacter->SetSerpveilChargeTelegraphActive(false);
        return;
    }

    bCachedIsMain = bIsMain;
    bIsCharging = true;
    ChargeStartTime = CurrentTime;
    bButtonReleased = false;
    bModeIsPreset = false;
    bWindupReady = false;
    CommittedReach = FMath::Max(DataAsset->SerpveilParams.SerpveilMaxRange, 0.0f);
    OwnerCharacter->SetSerpveilChargeTelegraphActive(true);

    const float SplitDelay = FMath::Max(Params.SerpveilSplitDelay, 0.0f);
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Serpveil S1] WindupStarted | Owner=%s | Main=%s | Time=%.3f | Delay=%.3f | CurrentVael=%.2f | ReservedVael=0.00"),
        *GetNameSafe(OwnerCharacter.Get()),
        bIsMain ? TEXT("true") : TEXT("false"),
        CurrentTime,
        SplitDelay,
        CurrentVael);

    if (SplitDelay <= 0.0f)
    {
        ExecuteSplitVolley();
        return;
    }

    World->GetTimerManager().SetTimer(
        WindupTimer,
        this,
        &UWTBRSerpveilTrigger::OnWindupComplete,
        SplitDelay,
        /*bLoop=*/false);
}

void UWTBRSerpveilTrigger::OnWindupComplete()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return;
    if (!bIsCharging) return;

    if (bButtonReleased)
    {
        ExecuteSplitVolley();
        return;
    }

    if (IsValid(DataAsset) && DataAsset->SerpveilParams.bSerpveilAutoFireAtFullCharge)
    {
        bModeIsPreset = true;
        CommittedReach = FMath::Max(DataAsset->SerpveilParams.SerpveilPresetMaxRange, 0.0f);
        ExecuteSplitVolley();
        return;
    }

    bWindupReady = true;
}

// ─── OnTriggerDeactivated — release captures hold mode and reach ──────────────

void UWTBRSerpveilTrigger::OnTriggerDeactivated_Implementation(
    AActor* /*OwnerActor*/, bool /*bIsMain*/)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return;

    UWorld* World = OwnerCharacter->GetWorld();
    if (!IsValid(World)) return;

    const bool bWindupActive = World->GetTimerManager().IsTimerActive(WindupTimer);
    if (!bWindupActive && !bIsCharging)
    {
        return;
    }

    const float Elapsed = World->GetTimeSeconds() - ChargeStartTime;
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Serpveil S2] ReleaseCaptured | Owner=%s | Elapsed=%.3f | WindupActive=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        Elapsed,
        bWindupActive ? TEXT("true") : TEXT("false"));
    HandleReleaseAtElapsed(Elapsed);
}

void UWTBRSerpveilTrigger::HandleReleaseAtElapsed(float Elapsed)
{
    bButtonReleased = true;
    bModeIsPreset = Elapsed >= (IsValid(DataAsset)
        ? DataAsset->SerpveilParams.SerpveilHoldThresholdSeconds : 0.0f);
    CommittedReach = ComputeReachForElapsed(Elapsed);

    if (bWindupReady)
    {
        ExecuteSplitVolley();
    }
}

float UWTBRSerpveilTrigger::ComputeReachForElapsed(float Elapsed) const
{
    if (!IsValid(DataAsset)) return 0.0f;
    const FWTBRSerpveilParams& Params = DataAsset->SerpveilParams;
    const float BasicRange = FMath::Max(Params.SerpveilMaxRange, 0.0f);

    if (Elapsed < Params.SerpveilHoldThresholdSeconds)
    {
        return BasicRange;
    }

    const float PresetMaxRange = FMath::Max(Params.SerpveilPresetMaxRange, BasicRange);
    const float WindupDuration = FMath::Max(Params.SerpveilSplitDelay, KINDA_SMALL_NUMBER);
    const float ThresholdToWindupSpan = FMath::Max(
        WindupDuration - Params.SerpveilHoldThresholdSeconds, KINDA_SMALL_NUMBER);
    const float ChargeFraction = FMath::Clamp(
        (Elapsed - Params.SerpveilHoldThresholdSeconds) / ThresholdToWindupSpan, 0.0f, 1.0f);

    return FMath::Lerp(BasicRange, PresetMaxRange, ChargeFraction);
}

void UWTBRSerpveilTrigger::Deactivate_Implementation()
{
    CancelCharge();
}

void UWTBRSerpveilTrigger::OnUnequipped()
{
    CancelCharge();
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
    UWorld* World = OwnerCharacter->GetWorld();
    const bool bWindupActive = IsValid(World)
        && World->GetTimerManager().IsTimerActive(WindupTimer);
    if (!bWindupActive && !bIsCharging)
    {
        return false;
    }

    const float CurrentTime = IsValid(World)
        ? World->GetTimeSeconds()
        : 0.0f;
    const float Elapsed = CurrentTime - ChargeStartTime;
    if (IsValid(World))
    {
        World->GetTimerManager().ClearTimer(WindupTimer);
    }
    bIsCharging = false;
    bButtonReleased = false;
    bModeIsPreset = false;
    bWindupReady = false;
    CommittedReach = 0.0f;
    StopChargeTracking();

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Serpveil S1] WindupCanceled | Owner=%s | Main=%s | Elapsed=%.3f | NoFire=true | NoVaelConsume=true"),
        *GetNameSafe(OwnerCharacter.Get()),
        bCachedIsMain ? TEXT("true") : TEXT("false"),
        Elapsed);

    OwnerCharacter->SetSerpveilChargeTelegraphActive(false);

    return true;
}

void UWTBRSerpveilTrigger::HandleOwnerDeath()
{
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Serpveil S1] OwnerDeathCancelRequested | Owner=%s"),
        *GetNameSafe(OwnerCharacter.Get()));
    CancelCharge();
}

// ─── ExecuteServerFire — server validates Vael and spawns projectile ──────────


void UWTBRSerpveilTrigger::ExecuteServerFire(
    EWTBRSerpveilShape Shape, FRotator Direction, float ChargedRange)
{
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Serpveil S1] DeprecatedReleaseFireIgnored | Owner=%s | Shape=%d | Direction=%s | ChargedRange=%.1f"),
        *GetNameSafe(OwnerCharacter.Get()),
        static_cast<int32>(Shape),
        *Direction.ToString(),
        ChargedRange);
}

void UWTBRSerpveilTrigger::ExecuteSplitVolley()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority())
    {
        return;
    }

    UWorld* World = OwnerCharacter->GetWorld();
    if (IsValid(World))
    {
        World->GetTimerManager().ClearTimer(WindupTimer);
    }
    bIsCharging = false;

    auto AbortVolley = [this](const TCHAR* Reason)
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[Serpveil S1] VolleyAborted | Owner=%s | Reason=%s | TelegraphOff=true"),
            *GetNameSafe(OwnerCharacter.Get()),
            Reason);
        if (OwnerCharacter.IsValid())
        {
            OwnerCharacter->SetSerpveilChargeTelegraphActive(false);
        }
    };

    if (!IsValid(World))
    {
        AbortVolley(TEXT("WorldInvalid"));
        return;
    }
    if (!IsValid(DataAsset))
    {
        AbortVolley(TEXT("DataAssetInvalid"));
        return;
    }
    if (!IsValid(OwnerCharacter->HealthComponent)
        || !OwnerCharacter->HealthComponent->IsAlive())
    {
        AbortVolley(TEXT("OwnerNotAlive"));
        return;
    }

    const FWTBRSerpveilParams& Params = DataAsset->SerpveilParams;
    if (!Params.SerpveilProjectileClass)
    {
        AbortVolley(TEXT("ProjectileClassNull"));
        return;
    }

    const int32 CubeCount = Params.SerpveilCubeSplitCount;
    if (CubeCount <= 0)
    {
        AbortVolley(TEXT("CubeCountInvalid"));
        return;
    }

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp))
    {
        AbortVolley(TEXT("VaelComponentInvalid"));
        return;
    }

    const float VaelCost = FMath::Max(Params.SerpveilVaelCostPerShot, 0.0f);
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Serpveil S1] ConsumeAttempt | Owner=%s | Cost=%.2f | CurrentVael=%.2f"),
        *GetNameSafe(OwnerCharacter.Get()),
        VaelCost,
        VaelComp->GetCurrentVael());
    if (!VaelComp->TryConsumeVael(VaelCost))
    {
        AbortVolley(TEXT("TryConsumeVaelFailed"));
        return;
    }

    FVector EyeLocation;
    FRotator AimRotation;
    OwnerCharacter->GetActorEyesViewPoint(EyeLocation, AimRotation);
    AimRotation.Roll = 0.0f;

    static const FName HandSocketName(TEXT("hand_r"));
    FVector SpawnOrigin;
    if (const USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
        IsValid(CharacterMesh) && CharacterMesh->DoesSocketExist(HandSocketName))
    {
        SpawnOrigin = CharacterMesh->GetSocketLocation(HandSocketName);
    }
    else
    {
        SpawnOrigin = EyeLocation + AimRotation.Vector() * 100.0f;
        UE_LOG(LogTemp, Warning,
            TEXT("[Serpveil S1] HandSocketFallback | Owner=%s | Socket=%s | Reason=MeshMissingOrSocketAbsent"),
            *GetNameSafe(OwnerCharacter.Get()),
            *HandSocketName.ToString());
    }

    const float MaxRange = FMath::Max(CommittedReach, 0.0f);
    const float FormationSpacing = FMath::Max(Params.SerpveilFormationSpacing, 0.0f);
    const float FormationStagger = FMath::Max(Params.SerpveilFormationStagger, 0.0f);
    const FRotationMatrix AimMatrix(AimRotation);
    int32 SpawnedCount = 0;

    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Serpveil S1] VolleySpawnStart | Owner=%s | Cubes=%d | FormationSpacing=%.1f | FormationStagger=%.1f | Pitch=%.1f | Range=%.1f | Speed=%.1f | PerCubeDamage=%.1f"),
        *GetNameSafe(OwnerCharacter.Get()),
        CubeCount,
        FormationSpacing,
        FormationStagger,
        AimRotation.Pitch,
        MaxRange,
        Params.SerpveilSpeed,
        Params.SerpveilPerCubeDamage);

    for (int32 CubeIndex = 0; CubeIndex < CubeCount; ++CubeIndex)
    {
        // Saw-tooth launch formation (▲▼▲): cubes line up sideways, centred on
        // the aim line, with even cubes raised and odd cubes lowered. Every
        // cube then flies straight and parallel along the aim direction.
        const float LateralOffset = (CubeIndex - (CubeCount - 1) * 0.5f) * FormationSpacing;
        const float VerticalOffset = ((CubeIndex % 2 == 0) ? 0.5f : -0.5f) * FormationStagger;
        const FVector CubeOrigin = SpawnOrigin
            + AimMatrix.TransformVector(FVector(0.0f, LateralOffset, VerticalOffset));
        const FTransform SpawnTransform(AimRotation, CubeOrigin);

        AWTBRProjectileBase* Projectile = World->SpawnActorDeferred<AWTBRProjectileBase>(
            Params.SerpveilProjectileClass,
            SpawnTransform,
            nullptr,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!IsValid(Projectile))
        {
            WTBR_VALIDATION_LOG(Verbose,
                TEXT("[Serpveil S1] CubeSpawnFailed | Owner=%s | Cube=%d | Class=%s"),
                *GetNameSafe(OwnerCharacter.Get()),
                CubeIndex,
                *GetNameSafe(Params.SerpveilProjectileClass));
            continue;
        }

        Projectile->InitializeProjectile(
            Params.SerpveilPerCubeDamage,
            Params.SerpveilSpeed,
            ETriggerCategory::Gunner,
            false,
            false,
            0.0f);
        Projectile->CubeSplitCount = 0;
        Projectile->OwnerInstigator = OwnerCharacter.Get();
        Projectile->FinishSpawning(SpawnTransform);

        TArray<FVector> PathPoints;
        PathPoints.Reserve(2);
        PathPoints.Add(CubeOrigin);
        PathPoints.Add(CubeOrigin + AimMatrix.TransformVector(FVector(MaxRange, 0.0f, 0.0f)));
        Projectile->InitializePathMovement(
            PathPoints,
            Params.SerpveilSpeed,
            OwnerCharacter.Get());

        ++SpawnedCount;
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[Serpveil S1] CubeSpawned | Owner=%s | Cube=%d | Projectile=%s | LateralOffset=%.1f | VerticalOffset=%.1f | Damage=%.1f | CubeSplitCount=%d"),
            *GetNameSafe(OwnerCharacter.Get()),
            CubeIndex,
            *GetNameSafe(Projectile),
            LateralOffset,
            VerticalOffset,
            Params.SerpveilPerCubeDamage,
            Projectile->CubeSplitCount);
    }

    LastSerpveilFireTime = World->GetTimeSeconds();
    if (UWTBRActionPingSubsystem* PingSystem = World->GetSubsystem<UWTBRActionPingSubsystem>())
    {
        PingSystem->RegisterActionPing(OwnerCharacter.Get());
    }

    // Preserve the existing BP event signature until S2 replaces shape presets.
    OnSerpveilFired(Params.PresetShape);
    OwnerCharacter->SetSerpveilChargeTelegraphActive(false);

    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Serpveil S1] VolleyComplete | Owner=%s | Requested=%d | Spawned=%d | VaelCost=%.2f | NewVael=%.2f | TelegraphOff=true"),
        *GetNameSafe(OwnerCharacter.Get()),
        CubeCount,
        SpawnedCount,
        VaelCost,
        VaelComp->GetCurrentVael());
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
