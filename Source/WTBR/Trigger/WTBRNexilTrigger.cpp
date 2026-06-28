// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRNexilTrigger.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRNexilWireActor.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/World.h"

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
    PlaceWire();
    return true;
}

void UWTBRNexilTrigger::PlaceWire()
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

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] PlaceWire Start | Owner=%s | WireActorClass=%s | Duration=%.1f | Stagger=%.2f | Length=%.1f | MaxWires=%d | ActiveBefore=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        *WireActorClass.ToString(),
        DataAsset->NexilParams.WireDuration,
        DataAsset->NexilParams.StaggerDuration,
        DataAsset->NexilParams.WireLength,
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

    const FVector  SpawnLoc = OwnerCharacter->GetActorLocation();
    const FRotator SpawnRot = OwnerCharacter->GetActorRotation();
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
        DataAsset->NexilParams.WireLength,
        this);

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

void UWTBRNexilTrigger::Deactivate_Implementation()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Nexil Test] Deactivate Cleanup | Owner=%s | ActiveBefore=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        GetActiveWireCount());
    for (auto& W : ActiveWires)
        if (W.IsValid()) W->Destroy();
    ActiveWires.Empty();
    Super::Deactivate_Implementation();
}
