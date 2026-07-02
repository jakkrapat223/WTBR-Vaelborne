// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCorpseLootContainerActor.generated.h"

class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UWTBRTriggerDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWTBRCorpseLootEntriesChanged);

// ─── UI-facing read-only view structs ────────────────────────────────────────
// These expose safe display/query data only. They carry no authority and do not
// mutate container or trigger-set state.

USTRUCT(BlueprintType)
struct FWTBRCorpseLootEntryViewModel
{
    GENERATED_BODY()

    // Stable array index in the container's LootEntries — use this when calling server RPCs.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot | UI")
    int32 StableEntryIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot | UI")
    TSoftObjectPtr<UWTBRTriggerDataAsset> TriggerDataAsset;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot | UI")
    ETriggerCategory CachedCategory = ETriggerCategory::None;

    // Original slot index on the dead character — informational only.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot | UI")
    int32 SourceSlotIndex = INDEX_NONE;

    // Derived from entry.IsValidForPickup() — false if consumed or asset null.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot | UI")
    bool bIsAvailable = false;
};

USTRUCT(BlueprintType)
struct FWTBRTargetSlotOptionViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot | UI")
    int32 SlotIndex = INDEX_NONE;

    // Localised label: "Main 1"–"Main 4" / "Sub 1"–"Sub 4".
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot | UI")
    FText SlotLabel;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot | UI")
    bool bIsEmpty = true;

    // Null when slot is empty.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot | UI")
    TSoftObjectPtr<UWTBRTriggerDataAsset> EquippedDataAsset;

    // True when the loot entry's SlotConstraint permits this slot.
    // Optimistically true when the DataAsset isn't loaded yet (server validates actual pickup).
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot | UI")
    bool bIsCompatible = false;

    // Non-empty only when bIsCompatible is false and the DataAsset was loaded.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot | UI")
    FText IncompatibleReason;
};

// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FWTBRCorpseLootEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot")
    TSoftObjectPtr<UWTBRTriggerDataAsset> TriggerDataAsset;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot")
    int32 SourceSlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot")
    ETriggerCategory CachedCategory = ETriggerCategory::None;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Corpse Loot")
    bool bConsumed = false;

    bool IsValidForPickup() const { return !bConsumed && !TriggerDataAsset.IsNull(); }
};

UCLASS()
class WTBR_API AWTBRCorpseLootContainerActor : public AActor
{
    GENERATED_BODY()

public:
    AWTBRCorpseLootContainerActor();

    void InitializeCorpseLootContainer(const TArray<FWTBRInstalledTriggerSlotSnapshot>& InstalledTriggerSnapshots);

    bool IsEntryValidForPickup(int32 LootEntryIndex) const;
    bool IsEntryConsumed(int32 LootEntryIndex) const;
    bool TryMarkEntryConsumed(int32 LootEntryIndex);
    void ClearEntryConsumedForFailedPickup(int32 LootEntryIndex);
    bool ReplaceEntryWithSnapshot(int32 LootEntryIndex, const FWTBRInstalledTriggerSlotSnapshot& Snapshot);
    bool AreAllEntriesConsumed() const;

    int32 GetLootEntryCount() const { return LootEntries.Num(); }
    TSoftObjectPtr<UWTBRTriggerDataAsset> GetEntryTriggerDataAsset(int32 LootEntryIndex) const;

    UFUNCTION(BlueprintPure, Category="WTBR | Corpse Loot")
    bool HasAvailableLootEntries() const;

    UFUNCTION(BlueprintPure, Category="WTBR | Corpse Loot")
    bool CanBeInteractedWithBy(const AActor* InteractingActor) const;

    UFUNCTION(BlueprintPure, Category="WTBR | Corpse Loot")
    FText GetInteractionPromptText() const;

    UFUNCTION(BlueprintPure, Category="WTBR | Corpse Loot")
    void GetLootEntriesForUIReadOnly(TArray<FWTBRCorpseLootEntry>& OutLootEntries) const;

    // ── UI model / query functions ────────────────────────────────────────────
    // Read-only. None of these mutate container entries, trigger slots, consume
    // entries, destroy the container, or bypass server RPC validation.

    // Build a display/query view of one loot entry by its stable array index.
    UFUNCTION(BlueprintPure, Category="WTBR | Corpse Loot | UI")
    FWTBRCorpseLootEntryViewModel BuildLootEntryViewModel(int32 LootEntryIndex) const;

    // Build one target-slot option per trigger slot (always 8 options).
    // LootDataAsset: pre-loaded DataAsset from the selected loot entry.
    // Pass null if the asset isn't in memory — slots will be shown as compatible
    // (the server validates the actual pickup attempt).
    UFUNCTION(BlueprintCallable, Category="WTBR | Corpse Loot | UI")
    void BuildTargetSlotOptions(
        const UWTBRTriggerSetComponent* TriggerSet,
        const UWTBRTriggerDataAsset* LootDataAsset,
        TArray<FWTBRTargetSlotOptionViewModel>& OutOptions) const;

    // Convenience overload: resolves the DataAsset from the loot entry at LootEntryIndex
    // then calls BuildTargetSlotOptions. If the asset is not yet in memory the same
    // optimistic-compatible behaviour applies.
    UFUNCTION(BlueprintCallable, Category="WTBR | Corpse Loot | UI")
    void BuildTargetSlotOptionsForEntry(
        const UWTBRTriggerSetComponent* TriggerSet,
        int32 LootEntryIndex,
        TArray<FWTBRTargetSlotOptionViewModel>& OutOptions) const;

    UPROPERTY(BlueprintAssignable, Category="WTBR | Corpse Loot")
    FWTBRCorpseLootEntriesChanged OnCorpseLootEntriesChanged;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WTBR | Corpse Loot")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WTBR | Corpse Loot")
    TObjectPtr<UStaticMeshComponent> VisualMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WTBR | Corpse Loot")
    TObjectPtr<USphereComponent> InteractionCollision;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Corpse Loot", meta=(ClampMin="0.0"))
    float InteractionRadius = 160.0f;

    UPROPERTY(ReplicatedUsing=OnRep_LootEntries, BlueprintReadOnly, Category="WTBR | Corpse Loot")
    TArray<FWTBRCorpseLootEntry> LootEntries;

    UFUNCTION()
    void OnRep_LootEntries();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    void NotifyLootEntriesChanged();
};
