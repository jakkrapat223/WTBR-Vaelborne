// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRNexilTrigger.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRNexilWireActor.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

namespace
{
    // Cadence for the Cypher-style ghost preview recompute — matches
    // UWTBRLacernTrigger::TickExtend's existing ~20Hz cosmetic-update rate.
    constexpr float WTBRNexilGhostPreviewTickInterval = 0.05f;

    // Shortest A→B span that still counts as a real wire (below this the two anchors
    // are effectively the same point). ⚠ PLAYTEST PENDING alongside NexilMaxWireSpan.
    constexpr float WTBRNexilMinWireSpan = 100.0f;

    // Ghost wire thickness in cm. Matches the real wire, whose WireMesh is authored
    // at X/Z scale 0.02 on a 100cm-diameter Cylinder = 2cm. Derived from mesh bounds
    // at runtime rather than trusting the ghost Blueprint's authored scale, which was
    // left at 1.0 and rendered the preview as a metre-thick slab.
    constexpr float WTBRNexilGhostThicknessCm = 2.0f;

    // On-screen diagnostics for the placement flow, gated by the same
    // wtbr.Debug.ValidationLogs CVar as the file logs. UE_LOG(Verbose) is filtered
    // out of the editor log by default, which made an earlier "no preview / can't
    // place" report impossible to diagnose from the log alone — these are visible
    // in PIE without touching log verbosity.
    void WTBRNexilScreenMsg(const FString& Msg, const FColor& Color)
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 4.0f, Color, FString(TEXT("[Nexil] ")) + Msg);
        }
    }
}

UWTBRNexilTrigger::UWTBRNexilTrigger()
{
    // Working defaults so the ghost preview needs no Blueprint assignment at all.
    // Still EditDefaultsOnly-overridable on BP_WTBRNexilTrigger if the assets move.
    GhostPreviewActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(
        TEXT("/Game/Blueprints/Triggers/BP_NexilGhostWire.BP_NexilGhostWire_C")));
    GhostValidMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
        TEXT("/Game/VFX/Materials/MI_NexilGhost_Valid.MI_NexilGhost_Valid")));
    GhostInvalidMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
        TEXT("/Game/VFX/Materials/MI_NexilGhost_Invalid.MI_NexilGhost_Invalid")));
}

void UWTBRNexilTrigger::InitializeTrigger(
    AWTBRCharacter* InOwnerCharacter,
    UWTBRTriggerDataAsset* InDataAsset)
{
    Super::InitializeTrigger(InOwnerCharacter, InDataAsset);
    ActiveWires.Empty();
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Initialize | Trigger=%s | TriggerClass=%s | Owner=%s | DataAsset=%s | WireActorClass=%s"),
        *GetNameSafe(this),
        *GetNameSafe(GetClass()),
        *GetNameSafe(InOwnerCharacter),
        *GetNameSafe(InDataAsset),
        *WireActorClass.ToString());

    // Ghost preview: start/stop tracking whichever slot(s) this instance
    // occupies, no button held required (owner's own Cypher-style ask).
    if (IsValid(InOwnerCharacter) && IsValid(InOwnerCharacter->TriggerSetComponent))
    {
        InOwnerCharacter->TriggerSetComponent->OnActiveTriggerChanged.AddDynamic(
            this, &UWTBRNexilTrigger::OnActiveTriggerSlotChanged);
    }
    RefreshGhostPreviewActiveState();
}

bool UWTBRNexilTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Activate Start | Trigger=%s | TriggerClass=%s | Owner=%s | HasAuthority=%s | Main=unknown | DualWield=%s | ActiveWires=%d"),
        *GetNameSafe(this),
        *GetNameSafe(GetClass()),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        bIsDualWield ? TEXT("true") : TEXT("false"),
        GetActiveWireCount());
    WTBRNexilScreenMsg(TEXT("Activate reached"), FColor::Cyan);

    if (!OwnerCharacter.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=OwnerInvalid | Function=Activate"));
        return false;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=NoAuthority | Function=Activate"));
        return false;
    }
    if (!Super::Activate_Implementation(InputValue, bIsDualWield))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=SuperActivateFalse"));
        return false;
    }

    // True Cypher trapwire: one press. Both endpoints are resolved right here —
    // exactly what the ghost has been previewing — so what you see is what you place.
    FVector WireA, WireB;
    if (!ComputeWirePlacement(WireA, WireB))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=NoPlacementSurfaces"));
        WTBRNexilScreenMsg(TEXT("REJECTED: need a surface to aim at with another facing it"), FColor::Red);
        return false;
    }
    if (!ValidateWireSpan(WireA, WireB))
    {
        const float Span = FVector::Dist(WireA, WireB);
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=InvalidSpan | Span=%.0f"), Span);
        WTBRNexilScreenMsg(
            FString::Printf(TEXT("REJECTED: gap %.0f out of range (need %.0f-%.0f)"),
                Span, WTBRNexilMinWireSpan,
                IsValid(DataAsset) ? DataAsset->NexilParams.NexilMaxWireSpan : 0.0f),
            FColor::Red);
        return false;
    }

    // Charge Vael per wire (base Activate consumes nothing). Fail-closed: not enough
    // Vael = no wire placed, no cost. Cost is DA-driven (NexilVaelCost, GDD 15/wire).
    const float VaelCost = IsValid(DataAsset) ? DataAsset->NexilParams.NexilVaelCost : 0.0f;
    if (VaelCost > 0.0f)
    {
        if (!IsValid(OwnerCharacter->VaelComponent)
            || !OwnerCharacter->VaelComponent->TryConsumeVael(VaelCost))
        {
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=InsufficientVael | Cost=%.2f"), VaelCost);
            WTBRNexilScreenMsg(FString::Printf(TEXT("REJECTED: not enough Vael (need %.0f)"), VaelCost), FColor::Red);
            return false;
        }
    }

    FTransform SpawnTransform;
    float SpanLength = 0.0f;
    BuildSpanTransform(WireA, WireB, SpawnTransform, SpanLength);
    WTBRNexilScreenMsg(FString::Printf(TEXT("WIRE PLACED | span %.0f"), SpanLength), FColor::Green);
    PlaceWire(SpawnTransform, SpanLength);
    return true;
}

void UWTBRNexilTrigger::PlaceWire(const FTransform& SpawnTransform, float WireSpanLength)
{
    if (!OwnerCharacter.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=OwnerInvalid | Function=PlaceWire"));
        return;
    }
    if (!IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=DataAssetInvalid | Function=PlaceWire"));
        return;
    }
    if (!GetWorld())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=WorldInvalid | Function=PlaceWire"));
        return;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] PlaceWire Start | Owner=%s | WireActorClass=%s | Duration=%.1f | Stagger=%.2f | SpanLength=%.1f | MaxWires=%d | ActiveBefore=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        *WireActorClass.ToString(),
        DataAsset->NexilParams.WireDuration,
        DataAsset->NexilParams.StaggerDuration,
        WireSpanLength,
        DataAsset->NexilParams.MaxWires,
        GetActiveWireCount());

    if (WireActorClass.IsNull())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=WireActorClassNull | TriggerClass=%s | ExpectedBP=BP_WTBRNexilTrigger_C"),
            *GetNameSafe(GetClass()));
        return;
    }

    TSubclassOf<AWTBRNexilWireActor> WireClass =
        WireActorClass.LoadSynchronous();
    if (!IsValid(WireClass))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=WireClassLoadFailed | WireActorClass=%s"),
            *WireActorClass.ToString());
        return;
    }

    // Cypher two-point placement: SpawnTransform spans anchor A→B (midpoint
    // location, local Y aligned to A→B), built by BuildSpanTransform — the same
    // math the ghost preview uses, so preview and placement can't drift apart.
    const FVector  SpawnLoc = SpawnTransform.GetLocation();
    const FRotator SpawnRot = SpawnTransform.Rotator();
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] WireSpawnAttempt | Class=%s | Location=%s | Rotation=%s"),
        *GetNameSafe(WireClass.Get()),
        *SpawnLoc.ToString(),
        *SpawnRot.ToString());

    FActorSpawnParameters Params;
    Params.Owner      = OwnerCharacter.Get();
    Params.Instigator = OwnerCharacter.Get();
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AWTBRNexilWireActor* Wire =
        GetWorld()->SpawnActor<AWTBRNexilWireActor>(
            WireClass, SpawnLoc, SpawnRot, Params);
    if (!IsValid(Wire))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=SpawnFailed"));
        return;
    }

    Wire->InitializeWire(
        DataAsset->NexilParams.WireDuration,
        DataAsset->NexilParams.StaggerDuration,
        WireSpanLength,
        DataAsset->NexilParams.WireHP,
        this,
        DataAsset->NexilParams.NexilZiplineLaunchSpeed);

    // TD Fix 1: bind OnDestroyed to cleanup ActiveWires automatically
    Wire->OnDestroyed.AddDynamic(
        this, &UWTBRNexilTrigger::NotifyWireDestroyed);

    ActiveWires.Add(Wire);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] WireSpawned | Wire=%s | Location=%s | ActiveAfterAdd=%d"),
        *GetNameSafe(Wire),
        *Wire->GetActorLocation().ToString(),
        GetActiveWireCount());

    const int32 MaxWires = DataAsset->NexilParams.MaxWires;
    while (ActiveWires.Num() > MaxWires)
        RemoveOldestWire();

    OnWirePlaced.Broadcast();
    OnNexilWirePlaced(Wire);
}

void UWTBRNexilTrigger::RemoveOldestWire()
{
    if (ActiveWires.Num() == 0) return;
    TWeakObjectPtr<AWTBRNexilWireActor> Oldest = ActiveWires[0];
    ActiveWires.RemoveAt(0);
    if (Oldest.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] MaxWires RemovingOldest | Wire=%s | ActiveAfterRemove=%d"),
            *GetNameSafe(Oldest.Get()),
            GetActiveWireCount());
        Oldest->Destroy();
    }
}

void UWTBRNexilTrigger::NotifyWireTriggered(
    AWTBRNexilWireActor* TriggeredWire)
{
    if (!OwnerCharacter.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Fail | Reason=OwnerInvalid | Function=NotifyWireTriggered"));
        return;
    }

    // Nexil tripwire fires Action Ping when wire is tripped (Vael leaves capsule)
    if (OwnerCharacter->VaelComponent)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] ActionPing | Owner=%s | Wire=%s"),
            *GetNameSafe(OwnerCharacter.Get()),
            *GetNameSafe(TriggeredWire));
        OwnerCharacter->VaelComponent->NotifyVaelLeftCharacterBounds();
    }

    ActiveWires.RemoveAll([TriggeredWire](
        const TWeakObjectPtr<AWTBRNexilWireActor>& W)
    {
        return W.Get() == TriggeredWire;
    });
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] WireTriggered Notify | Wire=%s | ActiveAfterRemove=%d"),
        *GetNameSafe(TriggeredWire),
        GetActiveWireCount());
    OnWireTriggered.Broadcast(TriggeredWire);
    OnNexilWireTriggeredVFX(TriggeredWire);
}

void UWTBRNexilTrigger::NotifyWireDestroyed(AActor* DestroyedActor)
{
    ActiveWires.RemoveAll([DestroyedActor](
        const TWeakObjectPtr<AWTBRNexilWireActor>& W)
    {
        return W.Get() == DestroyedActor;
    });
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] WireDestroyed Notify | Wire=%s | ActiveAfterRemove=%d"),
        *GetNameSafe(DestroyedActor),
        GetActiveWireCount());
}

int32 UWTBRNexilTrigger::GetActiveWireCount() const
{
    int32 Count = 0;
    for (const auto& W : ActiveWires)
        if (W.IsValid()) Count++;
    return Count;
}

AActor* UWTBRNexilTrigger::SpawnNexilGhostPreviewActor(TSubclassOf<AActor> GhostActorClass)
{
    if (!IsValid(GhostActorClass) || !GetWorld())
    {
        return nullptr;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Spawned = GetWorld()->SpawnActor<AActor>(GhostActorClass, Params);

    // Take ownership of the preview actor's lifecycle so StopGhostPreview can
    // guarantee it disappears regardless of what the BP hidden-event does.
    GhostPreviewActorInstance = Spawned;
    return Spawned;
}

void UWTBRNexilTrigger::Deactivate_Implementation()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Deactivate Cleanup | Owner=%s | ActiveBefore=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        GetActiveWireCount());
    TArray<TWeakObjectPtr<AWTBRNexilWireActor>> WiresToDestroy = MoveTemp(ActiveWires);
    ActiveWires.Empty();

    for (TWeakObjectPtr<AWTBRNexilWireActor>& Wire : WiresToDestroy)
    {
        if (Wire.IsValid())
        {
            Wire->Destroy();
        }
    }

    StopGhostPreview();
    if (OwnerCharacter.IsValid() && IsValid(OwnerCharacter->TriggerSetComponent))
    {
        OwnerCharacter->TriggerSetComponent->OnActiveTriggerChanged.RemoveDynamic(
            this, &UWTBRNexilTrigger::OnActiveTriggerSlotChanged);
    }

    Super::Deactivate_Implementation();
}

bool UWTBRNexilTrigger::ComputeWirePlacement(FVector& OutA, FVector& OutB) const
{
    if (!OwnerCharacter.IsValid() || !IsValid(DataAsset) || !GetWorld())
    {
        return false;
    }

#if WITH_DEV_AUTOMATION_TESTS
    if (TestPlacementOverride.IsSet())
    {
        OutA = TestPlacementOverride.GetValue().Key;
        OutB = TestPlacementOverride.GetValue().Value;
        return true;
    }
#endif

    FVector  EyeLoc;
    FRotator EyeRot;
    OwnerCharacter->GetActorEyesViewPoint(EyeLoc, EyeRot);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter.Get());
    // The ghost is cosmetic (NoCollision), but ignoring it explicitly keeps these
    // traces correct even if its collision is ever changed in the Blueprint.
    if (GhostPreviewActorInstance.IsValid())
    {
        QueryParams.AddIgnoredActor(GhostPreviewActorInstance.Get());
    }

    // Endpoint A: the surface under the crosshair. Rejecting character hits matches
    // UWTBRVoltisLaunchTrigger's Trap placement (no anchoring onto a player).
    FHitResult AimHit;
    const bool bAimHit = GetWorld()->LineTraceSingleByChannel(
        AimHit, EyeLoc,
        EyeLoc + EyeRot.Vector() * DataAsset->NexilParams.NexilPlacementRange,
        ECC_Visibility, QueryParams);
    if (!bAimHit || !IsValid(AimHit.GetActor())
        || AimHit.GetActor()->IsA(AWTBRCharacter::StaticClass()))
    {
        const float Now = GetWorld()->GetTimeSeconds();
        if (Now - LastAimPointFailLogTime > 0.5f)
        {
            LastAimPointFailLogTime = Now;
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Placement Fail | Reason=%s | Range=%.0f"),
                !bAimHit ? TEXT("NoSurfaceUnderCrosshair")
                    : !IsValid(AimHit.GetActor()) ? TEXT("HitActorInvalid")
                    : TEXT("HitCharacter"),
                DataAsset->NexilParams.NexilPlacementRange);
        }
        return false;
    }

    OutA = AimHit.Location;

    // Endpoint B: fire perpendicularly out of the aimed surface (along its impact
    // normal) to find the surface facing it across the gap — the trapwire strings
    // itself across a doorway/corridor. No facing surface in range = nothing to
    // anchor the far end to, so the placement is rejected outright.
    const FVector Normal = AimHit.ImpactNormal.GetSafeNormal();
    const float MaxSpan = DataAsset->NexilParams.NexilMaxWireSpan;
    FHitResult OppositeHit;
    const bool bOppositeHit = GetWorld()->LineTraceSingleByChannel(
        OppositeHit,
        OutA + Normal * 2.0f, // small offset so the trace doesn't re-hit surface A
        OutA + Normal * MaxSpan,
        ECC_Visibility, QueryParams);
    if (!bOppositeHit)
    {
        const float Now = GetWorld()->GetTimeSeconds();
        if (Now - LastAimPointFailLogTime > 0.5f)
        {
            LastAimPointFailLogTime = Now;
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Placement Fail | Reason=NoFacingSurface | A=%s | Normal=%s | MaxSpan=%.0f"),
                *OutA.ToString(), *Normal.ToString(), MaxSpan);
        }
        return false;
    }

    OutB = OppositeHit.Location;
    return true;
}

bool UWTBRNexilTrigger::ValidateWireSpan(const FVector& A, const FVector& B) const
{
    if (!IsValid(DataAsset))
    {
        return false;
    }

    const float Dist = FVector::Dist(A, B);
    return Dist >= WTBRNexilMinWireSpan && Dist <= DataAsset->NexilParams.NexilMaxWireSpan;
}

void UWTBRNexilTrigger::BuildSpanTransform(
    const FVector& A, const FVector& B, FTransform& OutTransform, float& OutLength) const
{
    const FVector Delta = B - A;
    OutLength = Delta.Size();

    FQuat Rot = FQuat::Identity;
    if (OutLength > KINDA_SMALL_NUMBER)
    {
        // Align the wire's local Y axis (its length axis, matching
        // AWTBRNexilWireActor's box extent) with the A→B direction. FindBetweenNormals
        // handles any orientation — flat, diagonal, or vertical.
        Rot = FQuat::FindBetweenNormals(FVector(0.0f, 1.0f, 0.0f), Delta / OutLength);
    }
    OutTransform = FTransform(Rot, (A + B) * 0.5f);
}

bool UWTBRNexilTrigger::ComputeGhostPreviewTransform(
    FTransform& OutTransform, float& OutLength, bool& bOutValid) const
{
    bOutValid = false;
    OutLength = 0.0f;
    if (!OwnerCharacter.IsValid() || !IsValid(DataAsset) || !GetWorld())
    {
        return false;
    }

    // Preview exactly what a press right now would place — both endpoints, no press
    // required. Nothing placeable under the crosshair hides the ghost entirely.
    FVector WireA, WireB;
    if (!ComputeWirePlacement(WireA, WireB))
    {
        return false;
    }

    BuildSpanTransform(WireA, WireB, OutTransform, OutLength);
    bOutValid = ValidateWireSpan(WireA, WireB);
    return true;
}

void UWTBRNexilTrigger::OnActiveTriggerSlotChanged(ETriggerSlot /*NewSlot*/)
{
    RefreshGhostPreviewActiveState();
}

void UWTBRNexilTrigger::RefreshGhostPreviewActiveState()
{
    if (!OwnerCharacter.IsValid() || !IsValid(OwnerCharacter->TriggerSetComponent))
    {
        StopGhostPreview();
        return;
    }

    UWTBRTriggerSetComponent* TSC = OwnerCharacter->TriggerSetComponent;
    const bool bIsActive = (TSC->GetActiveMainTrigger() == this) || (TSC->GetActiveSubTrigger() == this);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] RefreshGhostPreviewActiveState | Trigger=%s | IsActiveMain=%s | IsActiveSub=%s | WillStart=%s"),
        *GetNameSafe(this),
        (TSC->GetActiveMainTrigger() == this) ? TEXT("true") : TEXT("false"),
        (TSC->GetActiveSubTrigger() == this) ? TEXT("true") : TEXT("false"),
        bIsActive ? TEXT("true") : TEXT("false"));
    WTBRNexilScreenMsg(
        FString::Printf(TEXT("Preview %s (Nexil is %s active trigger)"),
            bIsActive ? TEXT("ON") : TEXT("OFF"),
            bIsActive ? TEXT("the") : TEXT("NOT the")),
        bIsActive ? FColor::Green : FColor::Yellow);
    if (bIsActive)
    {
        StartGhostPreview();
    }
    else
    {
        StopGhostPreview();
    }
}

void UWTBRNexilTrigger::StartGhostPreview()
{
    if (!GetWorld()) return;
    if (GetWorld()->GetTimerManager().IsTimerActive(GhostPreviewTimer)) return;

    GetWorld()->GetTimerManager().SetTimer(
        GhostPreviewTimer,
        this, &UWTBRNexilTrigger::TickGhostPreview,
        WTBRNexilGhostPreviewTickInterval,
        true);
    TickGhostPreview();
}

void UWTBRNexilTrigger::StopGhostPreview()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(GhostPreviewTimer);
    }
    OnNexilGhostPreviewHidden();

    // Authoritative hide: destroy the actor we cached at spawn. The BP's lazy-spawn
    // re-creates it on the next preview start, so this can't leave a stuck ghost.
    if (GhostPreviewActorInstance.IsValid())
    {
        GhostPreviewActorInstance->Destroy();
    }
    GhostPreviewActorInstance.Reset();
}

void UWTBRNexilTrigger::UpdateGhostPreviewActor(
    const FTransform& Transform, float PreviewLength, bool bValid)
{
    if (!GetWorld())
    {
        return;
    }

    // Spawn on first use (and re-spawn after StopGhostPreview destroyed it).
    if (!GhostPreviewActorInstance.IsValid())
    {
        UClass* GhostClass = GhostPreviewActorClass.LoadSynchronous();
        if (!IsValid(GhostClass))
        {
            WTBR_VALIDATION_LOG(Warning, TEXT("[Nexil Test] Ghost Fail | Reason=GhostClassLoadFailed | Class=%s"),
                *GhostPreviewActorClass.ToString());
            return;
        }

        FActorSpawnParameters Params;
        Params.Owner = OwnerCharacter.Get();
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        GhostPreviewActorInstance = GetWorld()->SpawnActor<AActor>(GhostClass, Transform, Params);
        if (!GhostPreviewActorInstance.IsValid())
        {
            WTBR_VALIDATION_LOG(Warning, TEXT("[Nexil Test] Ghost Fail | Reason=SpawnFailed | Class=%s"),
                *GetNameSafe(GhostClass));
            return;
        }
    }

    AActor* Ghost = GhostPreviewActorInstance.Get();
    Ghost->SetActorTransform(Transform);

    UStaticMeshComponent* GhostMesh = Ghost->FindComponentByClass<UStaticMeshComponent>();
    if (!IsValid(GhostMesh))
    {
        return;
    }

    // Stretch along local Y only (the wire's length axis), preserving the authored
    // thin X/Z — same math AWTBRNexilWireActor::InitializeWire uses for the real wire.
    if (IsValid(GhostMesh->GetStaticMesh()))
    {
        // Drive all three axes from mesh bounds so the ghost reads as a taut cable
        // regardless of the Blueprint's authored scale: length along local Y (the
        // wire's span axis), fixed thin cross-section on X/Z.
        const FVector MeshSize = GhostMesh->GetStaticMesh()->GetBounds().BoxExtent * 2.0f;
        if (MeshSize.Y > KINDA_SMALL_NUMBER)
        {
            FVector MeshScale;
            MeshScale.X = MeshSize.X > KINDA_SMALL_NUMBER ? WTBRNexilGhostThicknessCm / MeshSize.X : 0.02f;
            MeshScale.Y = PreviewLength / MeshSize.Y;
            MeshScale.Z = MeshSize.Z > KINDA_SMALL_NUMBER ? WTBRNexilGhostThicknessCm / MeshSize.Z : 0.02f;
            GhostMesh->SetRelativeScale3D(MeshScale);

            // Throttled numeric readout — turns "the ghost looks wrong" into actual
            // numbers (mesh bounds vs requested length vs resulting scale).
            const float Now = GetWorld()->GetTimeSeconds();
            if (Now - LastGhostScaleLogTime > 1.0f)
            {
                LastGhostScaleLogTime = Now;
                WTBRNexilScreenMsg(
                    FString::Printf(TEXT("ghost len=%.0f meshSize=(%.0f,%.0f,%.0f) scale=(%.3f, %.2f, %.3f)"),
                        PreviewLength, MeshSize.X, MeshSize.Y, MeshSize.Z,
                        MeshScale.X, MeshScale.Y, MeshScale.Z),
                    FColor::White);
            }
        }
    }

    UMaterialInterface* DesiredMaterial =
        bValid ? GhostValidMaterial.LoadSynchronous() : GhostInvalidMaterial.LoadSynchronous();
    if (IsValid(DesiredMaterial) && GhostMesh->GetMaterial(0) != DesiredMaterial)
    {
        GhostMesh->SetMaterial(0, DesiredMaterial);
    }
}

void UWTBRNexilTrigger::TickGhostPreview()
{
    FTransform Transform;
    float PreviewLength = 0.0f;
    bool bValid = false;
    if (!ComputeGhostPreviewTransform(Transform, PreviewLength, bValid))
    {
        // Nothing placeable under the crosshair — hide the ghost but keep the timer
        // running so it reappears as soon as the aim finds a surface again.
        if (GhostPreviewActorInstance.IsValid())
        {
            GhostPreviewActorInstance->Destroy();
            GhostPreviewActorInstance.Reset();
        }
        OnNexilGhostPreviewHidden();
        return;
    }

    UpdateGhostPreviewActor(Transform, PreviewLength, bValid);
    // Still fired for any extra Blueprint-side cosmetics; C++ no longer depends on it.
    OnNexilGhostPreviewUpdated(Transform, PreviewLength, bValid);
}
