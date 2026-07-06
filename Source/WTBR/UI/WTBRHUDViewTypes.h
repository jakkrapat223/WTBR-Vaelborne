// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WTBRHUDViewTypes.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct FWTBRHUDBindingDisplay
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Binding")
    bool bIsBound = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Binding")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Binding")
    FName GlyphToken = NAME_None;
};

USTRUCT(BlueprintType)
struct FWTBRHUDVitalsSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Vitals")
    bool bIsValid = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Vitals")
    float HP = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Vitals")
    float MaxHP = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Vitals")
    float Vael = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Vitals")
    float MaxVael = 0.0f;
};

USTRUCT(BlueprintType)
struct FWTBRHUDMatchSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Match")
    bool bHasAliveCount = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Match")
    int32 AliveCount = 0;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Match")
    bool bHasKillCount = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Match")
    int32 KillCount = 0;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Match")
    bool bHasZoneTimer = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Match")
    float ZoneTimeRemaining = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Match")
    bool bHasMatchPhase = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Match")
    FText MatchPhaseText;
};

USTRUCT(BlueprintType)
struct FWTBRHUDTriggerCardSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Trigger")
    bool bIsValid = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Trigger")
    bool bIsMain = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Trigger")
    FText TriggerName;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Trigger")
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Trigger")
    int32 ActiveSlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Trigger")
    FWTBRHUDBindingDisplay UseBinding;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Trigger")
    bool bCanAffordVaelCost = true;

    // True only for a simple, known per-use Vael cost (mirrors
    // FWTBRHUDTriggerVaelAffordability::bIsCostKnownForHUD). False for
    // charge/variable-cost triggers (e.g. Serpveil) or drain-based triggers
    // (e.g. Aegorn, Black Trigger fields) where a single "Cost: N" number
    // would be misleading.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Trigger")
    bool bIsCostKnownForHUD = false;

    // True when the known cost is nonzero. False for zero-cost triggers by
    // design (Feryx, Lacern, Mantorn, Vexorn, etc. — see Vael cost design locks).
    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Trigger")
    bool bHasVaelCost = false;

    // Effective (post-multiplier) Vael cost. Only meaningful when bIsCostKnownForHUD is true.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Trigger")
    float EffectiveVaelCost = 0.0f;
};

UENUM(BlueprintType)
enum class EWTBRHUDQuickItemState : uint8
{
    Empty,
    Normal,
    Warning,
};

USTRUCT(BlueprintType)
struct FWTBRHUDQuickItemSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Quick Item")
    bool bHasItem = false;

    // Item display name (e.g. "HP Capsule"). Only meaningful when bHasItem is true.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Quick Item")
    FText ItemName;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Quick Item")
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Quick Item")
    int32 Count = 0;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Quick Item")
    EWTBRHUDQuickItemState State = EWTBRHUDQuickItemState::Empty;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Quick Item")
    FWTBRHUDBindingDisplay Binding;
};

USTRUCT(BlueprintType)
struct FWTBRHUDPickupPromptSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Pickup")
    bool bIsVisible = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Pickup")
    FText PromptText;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Pickup")
    FWTBRHUDBindingDisplay Binding;
};

USTRUCT(BlueprintType)
struct FWTBRHUDCancelPromptSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Cancel")
    bool bIsVisible = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Cancel")
    FText PromptText;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD | Cancel")
    FWTBRHUDBindingDisplay Binding;
};

USTRUCT(BlueprintType)
struct FWTBRHUDSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    FWTBRHUDMatchSnapshot Match;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    FWTBRHUDVitalsSnapshot Vitals;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    FWTBRHUDTriggerCardSnapshot MainTrigger;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    FWTBRHUDTriggerCardSnapshot SubTrigger;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    FWTBRHUDQuickItemSnapshot QuickItem;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    FWTBRHUDPickupPromptSnapshot PickupPrompt;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    FWTBRHUDCancelPromptSnapshot CancelPrompt;
};
