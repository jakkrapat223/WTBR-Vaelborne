// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Inventory/WTBRInventoryComponent.h"

#include "Inventory/WTBRItemDataAsset.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UWTBRInventoryComponent::UWTBRInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UWTBRInventoryComponent::BeginPlay()
{
    Super::BeginPlay();

    // Server sizes the slot array once gameplay begins; clients receive it via
    // replication and OnRep_InventorySlots.
    InitializeServerInventory();
}

bool UWTBRInventoryComponent::HasServerAuthority() const
{
    const AActor* Owner = GetOwner();
    return Owner && Owner->HasAuthority();
}

void UWTBRInventoryComponent::InitializeServerInventory()
{
    if (!HasServerAuthority())
    {
        return;
    }

    const int32 SlotCountBefore = InventorySlots.Num();
    EnsureSlotsSized();

    if (InventorySlots.Num() != SlotCountBefore)
    {
        OnInventoryChanged.Broadcast();
    }
}

void UWTBRInventoryComponent::EnsureSlotsSized()
{
    const int32 DesiredCount = FMath::Max(InventorySlotCount, 0);
    if (InventorySlots.Num() != DesiredCount)
    {
        // SetNum preserves existing entries when growing and truncates when
        // shrinking, keeping behavior deterministic if InventorySlotCount changes.
        InventorySlots.SetNum(DesiredCount);
    }
}

bool UWTBRInventoryComponent::SlotMatchesItem(const FWTBRInventorySlot& Slot, const UWTBRItemDataAsset* ItemData) const
{
    if (Slot.ItemData.IsNull() || ItemData == nullptr)
    {
        return false;
    }

    // Prefer resolved-pointer identity (no load); fall back to soft-path identity.
    // Never compare by DisplayName.
    if (const UWTBRItemDataAsset* SlotItem = Slot.ItemData.Get())
    {
        return SlotItem == ItemData;
    }

    return Slot.ItemData.ToSoftObjectPath() == FSoftObjectPath(ItemData);
}

int32 UWTBRInventoryComponent::ComputeAddableCapacity(const UWTBRItemDataAsset* ItemData) const
{
    if (ItemData == nullptr)
    {
        return 0;
    }

    const int32 Limit = ItemData->MaxStackSize;
    if (Limit <= 0)
    {
        return 0;
    }

    int32 Capacity = 0;
    for (const FWTBRInventorySlot& Slot : InventorySlots)
    {
        if (Slot.IsEmpty())
        {
            Capacity += Limit;
        }
        else if (SlotMatchesItem(Slot, ItemData) && Slot.Quantity < Limit)
        {
            Capacity += Limit - Slot.Quantity;
        }
    }

    // Account for empty slots not yet materialized (e.g. before InitializeServerInventory).
    const int32 Shortfall = InventorySlotCount - InventorySlots.Num();
    if (Shortfall > 0)
    {
        Capacity += Shortfall * Limit;
    }

    return Capacity;
}

bool UWTBRInventoryComponent::CanAddItem(const UWTBRItemDataAsset* ItemData, int32 Quantity) const
{
    if (ItemData == nullptr || Quantity <= 0)
    {
        return false;
    }

    if (ItemData->MaxStackSize <= 0)
    {
        return false;
    }

    return ComputeAddableCapacity(ItemData) >= Quantity;
}

bool UWTBRInventoryComponent::TryAddItem(const UWTBRItemDataAsset* ItemData, int32 Quantity)
{
    if (!HasServerAuthority())
    {
        return false;
    }

    if (ItemData == nullptr || Quantity <= 0)
    {
        return false;
    }

    const int32 Limit = ItemData->MaxStackSize;
    if (Limit <= 0)
    {
        return false;
    }

    // Materialize the full slot array before simulating so the transactional
    // capacity check runs against the real, final-size inventory.
    EnsureSlotsSized();

    // Transactional pre-check: if the full quantity does not fit, mutate nothing.
    if (ComputeAddableCapacity(ItemData) < Quantity)
    {
        return false;
    }

    int32 Remaining = Quantity;

    // Pass 1 — fill existing partial stacks of the same item.
    for (FWTBRInventorySlot& Slot : InventorySlots)
    {
        if (Remaining <= 0)
        {
            break;
        }

        if (!Slot.IsEmpty() && SlotMatchesItem(Slot, ItemData) && Slot.Quantity < Limit)
        {
            const int32 Added = FMath::Min(Limit - Slot.Quantity, Remaining);
            Slot.Quantity += Added;
            Remaining -= Added;
        }
    }

    // Pass 2 — fill the first empty slot(s).
    for (FWTBRInventorySlot& Slot : InventorySlots)
    {
        if (Remaining <= 0)
        {
            break;
        }

        if (Slot.IsEmpty())
        {
            const int32 Added = FMath::Min(Limit, Remaining);
            Slot.ItemData = TSoftObjectPtr<UWTBRItemDataAsset>(const_cast<UWTBRItemDataAsset*>(ItemData));
            Slot.Quantity = Added;
            Remaining -= Added;
        }
    }

    // Pre-check guarantees Remaining == 0 here; broadcast only on real mutation.
    OnInventoryChanged.Broadcast();
    return true;
}

bool UWTBRInventoryComponent::ConsumeItemAtSlot(int32 SlotIndex, int32 Quantity)
{
    if (!HasServerAuthority())
    {
        return false;
    }

    if (!InventorySlots.IsValidIndex(SlotIndex))
    {
        return false;
    }

    if (Quantity <= 0)
    {
        return false;
    }

    FWTBRInventorySlot& Slot = InventorySlots[SlotIndex];
    if (Slot.IsEmpty() || Quantity > Slot.Quantity)
    {
        return false;
    }

    Slot.Quantity -= Quantity;
    if (Slot.Quantity <= 0)
    {
        Slot.Clear();
    }

    OnInventoryChanged.Broadcast();
    return true;
}

void UWTBRInventoryComponent::OnRep_InventorySlots()
{
    OnInventoryChanged.Broadcast();
}

void UWTBRInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UWTBRInventoryComponent, InventorySlots);
}
