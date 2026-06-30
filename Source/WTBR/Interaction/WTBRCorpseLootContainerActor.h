// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCorpseLootContainerActor.generated.h"

class USceneComponent;
class UWTBRTriggerDataAsset;

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
    bool AreAllEntriesConsumed() const;

    int32 GetLootEntryCount() const { return LootEntries.Num(); }
    TSoftObjectPtr<UWTBRTriggerDataAsset> GetEntryTriggerDataAsset(int32 LootEntryIndex) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WTBR | Corpse Loot")
    TObjectPtr<USceneComponent> RootSceneComponent;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="WTBR | Corpse Loot")
    TArray<FWTBRCorpseLootEntry> LootEntries;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
