// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "WTBRBotLoadoutSetDataAsset.generated.h"

UENUM(BlueprintType)
enum class EWTBRBotCombatRole : uint8
{
    Gunner UMETA(DisplayName="Gunner"),
    Sniper UMETA(DisplayName="Sniper"),
    Melee UMETA(DisplayName="Melee"),
};

// An approved, complete Bot v1 loadout. Main is intentionally separate from
// AegornSub so a random role can never replace the defensive Sub requirement.
USTRUCT(BlueprintType)
struct FWTBRApprovedBotLoadout
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bot Loadout")
    EWTBRBotCombatRole Role = EWTBRBotCombatRole::Gunner;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bot Loadout")
    TSoftObjectPtr<UWTBRTriggerDataAsset> MainTrigger;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bot Loadout")
    TSoftObjectPtr<UWTBRTriggerDataAsset> AegornSubTrigger;
};

// Author this Data Asset in the editor with only loadouts approved for basic
// bots. Each represented role is selected with equal probability, then one of
// that role's approved entries is selected at random.
UCLASS(BlueprintType)
class WTBR_API UWTBRBotLoadoutSetDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bot Loadout")
    TArray<FWTBRApprovedBotLoadout> ApprovedLoadouts;
};
