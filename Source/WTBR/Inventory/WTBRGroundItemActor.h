// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Inventory/WTBRItemDataAsset.h"
#include "WTBRGroundItemActor.generated.h"

class USceneComponent;
class USphereComponent;

// ─────────────────────────────────────────────────────────────────────────────
// AWTBRGroundItemActor — a BR ground item that can be picked up into the
// inventory (S5-C). SEPARATE from AWTBRDroppedTriggerActor:
//   * Dropped Trigger = Trigger loot only.
//   * Ground Item     = consumables, Vael items, tuning, options, upgrade materials.
//
// Mirrors the dropped-trigger consumed-claim model: server-authoritative
// TryMarkConsumed / ClearConsumedForFailedPickup with a replicated bConsumed so a
// failed inventory add can roll the item back to available (all-or-nothing pickup).
//
// MVP payload is item data + quantity only. No mesh/asset setup is required.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class WTBR_API AWTBRGroundItemActor : public AActor
{
    GENERATED_BODY()

public:
    AWTBRGroundItemActor();

    // Server-only: set the item payload. No-op without authority.
    void InitializeGroundItem(const TSoftObjectPtr<UWTBRItemDataAsset>& InItemData, int32 InQuantity);

    TSoftObjectPtr<UWTBRItemDataAsset> GetItemData() const { return ItemData; }

    int32 GetQuantity() const { return Quantity; }

    bool IsConsumed() const { return bConsumed; }

    // Atomic server-side claim: returns false if already consumed or non-authority.
    bool TryMarkConsumed();

    // Rolls the claim back after a failed pickup (all-or-nothing). Authority only.
    void ClearConsumedForFailedPickup();

    // Optional UI-facing interaction prompt (designer-set per instance/DA).
    UFUNCTION(BlueprintPure, Category="WTBR | Ground Item")
    FText GetInteractionPromptText() const { return InteractionPromptText; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WTBR | Ground Item")
    TObjectPtr<USceneComponent> RootSceneComponent;

    // Query-only interaction volume that exists ONLY so the interaction visibility
    // trace (see UWTBRInteractionComponent::GetFocusedGroundItem) can focus this
    // actor. Mirrors the corpse container's setup: QueryOnly, WorldDynamic, ignores
    // all channels and blocks ECC_Visibility only. It never blocks movement or
    // projectiles and generates no overlap events.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WTBR | Ground Item | Interaction", meta=(AllowPrivateAccess="true"))
    TObjectPtr<USphereComponent> InteractionCollision;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Ground Item | Interaction", meta=(ClampMin="1.0"))
    float InteractionCollisionRadius = 60.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="WTBR | Ground Item")
    TSoftObjectPtr<UWTBRItemDataAsset> ItemData;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="WTBR | Ground Item", meta=(ClampMin="1"))
    int32 Quantity = 1;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="WTBR | Ground Item")
    bool bConsumed = false;

    // UI-facing only; not required for gameplay. Localization-ready.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Ground Item")
    FText InteractionPromptText;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
