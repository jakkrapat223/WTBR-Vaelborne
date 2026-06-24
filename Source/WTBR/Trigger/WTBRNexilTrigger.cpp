// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRNexilTrigger.h"
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
}

bool UWTBRNexilTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!Super::Activate_Implementation(InputValue, bIsDualWield))
        return false;
    PlaceWire();
    return true;
}

void UWTBRNexilTrigger::PlaceWire()
{
    if (!OwnerCharacter.IsValid() || !IsValid(DataAsset)) return;
    if (WireActorClass.IsNull()) return;

    TSubclassOf<AWTBRNexilWireActor> WireClass =
        WireActorClass.LoadSynchronous();
    if (!IsValid(WireClass)) return;

    const FVector  SpawnLoc = OwnerCharacter->GetActorLocation();
    const FRotator SpawnRot = OwnerCharacter->GetActorRotation();

    FActorSpawnParameters Params;
    Params.Owner      = OwnerCharacter.Get();
    Params.Instigator = OwnerCharacter.Get();
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AWTBRNexilWireActor* Wire =
        GetWorld()->SpawnActor<AWTBRNexilWireActor>(
            WireClass, SpawnLoc, SpawnRot, Params);
    if (!IsValid(Wire)) return;

    Wire->InitializeWire(
        DataAsset->NexilParams.WireDuration,
        DataAsset->NexilParams.StaggerDuration,
        DataAsset->NexilParams.WireLength,
        this);

    // TD Fix 1: bind OnDestroyed to cleanup ActiveWires automatically
    Wire->OnDestroyed.AddDynamic(
        this, &UWTBRNexilTrigger::NotifyWireDestroyed);

    ActiveWires.Add(Wire);

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
        Oldest->Destroy();
}

void UWTBRNexilTrigger::NotifyWireTriggered(
    AWTBRNexilWireActor* TriggeredWire)
{
    if (!OwnerCharacter.IsValid()) return;

    // Nexil tripwire fires Action Ping when wire is tripped (Vael leaves capsule)
    if (OwnerCharacter->VaelComponent)
        OwnerCharacter->VaelComponent->NotifyVaelLeftCharacterBounds();

    ActiveWires.RemoveAll([TriggeredWire](
        const TWeakObjectPtr<AWTBRNexilWireActor>& W)
    {
        return W.Get() == TriggeredWire;
    });
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
    for (auto& W : ActiveWires)
        if (W.IsValid()) W->Destroy();
    ActiveWires.Empty();
    Super::Deactivate_Implementation();
}
