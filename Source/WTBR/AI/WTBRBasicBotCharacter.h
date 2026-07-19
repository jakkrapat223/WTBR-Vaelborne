// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WTBRCharacter.h"
#include "WTBRBasicBotCharacter.generated.h"

class UWTBRBotLoadoutSetDataAsset;

// Place or spawn this class (or a Blueprint child) for a bot. Assign an
// ApprovedLoadoutSet; it owns the bot's randomized Main + Aegorn Sub slots.
UCLASS(BlueprintType)
class WTBR_API AWTBRBasicBotCharacter : public AWTBRCharacter
{
    GENERATED_BODY()

public:
    // Forwards the ObjectInitializer so AWTBRCharacter can keep substituting
    // UWTBRCharacterMovementComponent for the default movement component.
    explicit AWTBRBasicBotCharacter(const FObjectInitializer& ObjectInitializer);

    // Assign an editor-authored set containing approved Main + Aegorn Sub pairs.
    // Bots wait for LoadoutSetup before choosing a role, then lock slots 0 and 4.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Bot | Loadout")
    TSoftObjectPtr<UWTBRBotLoadoutSetDataAsset> ApprovedLoadoutSet;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Bot | Loadout", meta=(ClampMin="0.25"))
    float LoadoutAssignmentTimeoutSeconds = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Bot | Loadout", meta=(ClampMin="1"))
    int32 MaxLoadoutAssignmentRetries = 80;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FTimerHandle LoadoutAssignmentRetryTimer;
    bool bApprovedLoadoutAssigned = false;
    bool bMissingLoadoutSetLogged = false;
    bool bLoadoutAssignmentTimeoutLogged = false;
    float LoadoutAssignmentStartTime = -1.0f;
    int32 LoadoutAssignmentRetryCount = 0;

    void TryAssignApprovedLoadout();
};
