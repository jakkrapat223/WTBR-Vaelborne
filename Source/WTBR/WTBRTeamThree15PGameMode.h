// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WTBRGameMode.h"
#include "WTBRTeamThree15PGameMode.generated.h"

class AWTBRBasicBotCharacter;
class UWTBRBotLoadoutSetDataAsset;

// Level-specific default for the 1 x 1 km combat blockout. Keeping this in a
// small native subclass makes the map self-contained: opening it always uses
// the 15-player rules without changing the project's default 3v3 GameMode.
UCLASS()
class WTBR_API AWTBRTeamThree15PGameMode : public AWTBRGameMode
{
    GENERATED_BODY()

public:
    AWTBRTeamThree15PGameMode();

    virtual void BeginPlay() override;

protected:
    // The blockout is a solo-friendly 15-player test map: one human joins and
    // this GameMode supplies the remaining fourteen combatants on the server.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | 15P Bot Fill", meta=(ClampMin="0", ClampMax="59"))
    int32 BotsToSpawn = 14;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | 15P Bot Fill")
    TSubclassOf<AWTBRBasicBotCharacter> BasicBotClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | 15P Bot Fill")
    TSoftObjectPtr<UWTBRBotLoadoutSetDataAsset> BotLoadoutSet;

private:
    void SpawnMatchFillBots();
};
