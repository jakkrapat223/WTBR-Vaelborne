// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "WTBRTeamThree15PGameMode.h"

#include "AI/WTBRBasicBotCharacter.h"
#include "AI/WTBRBotLoadoutSetDataAsset.h"
#include "UObject/ConstructorHelpers.h"

AWTBRTeamThree15PGameMode::AWTBRTeamThree15PGameMode()
{
    DefaultMatchMode = EWTBRMatchMode::TeamThree15P;

    // This level is one kilometre square, so it must not inherit the small
    // ThirdPerson graybox spawn area used by the project's generic GameMode.
    static ConstructorHelpers::FObjectFinder<UWTBRRandomSpawnConfigDataAsset> BlockoutSpawnConfig(
        TEXT("/Game/Data/DA_RandomSpawnConfig_15P_Blockout.DA_RandomSpawnConfig_15P_Blockout"));
    if (BlockoutSpawnConfig.Succeeded())
    {
        RandomSpawnConfig = BlockoutSpawnConfig.Object;
    }

    BasicBotClass = AWTBRBasicBotCharacter::StaticClass();
    BotLoadoutSet = TSoftObjectPtr<UWTBRBotLoadoutSetDataAsset>(FSoftObjectPath(
        TEXT("/Game/Data/DA_BotLoadoutSet_15P_Blockout.DA_BotLoadoutSet_15P_Blockout")));
}

void AWTBRTeamThree15PGameMode::BeginPlay()
{
    // Spawn before the base flow opens LoadoutSetup. Deferred construction lets
    // every bot receive its approved loadout asset before its BeginPlay retry
    // loop starts; then all fifteen are present for the same InMatch scatter.
    if (HasAuthority())
    {
        SpawnMatchFillBots();
    }

    Super::BeginPlay();
}

void AWTBRTeamThree15PGameMode::SpawnMatchFillBots()
{
    UWorld* World = GetWorld();
    if (!World || BotsToSpawn <= 0)
    {
        return;
    }

    const TSubclassOf<AWTBRBasicBotCharacter> SpawnClass = BasicBotClass
        ? BasicBotClass
        : AWTBRBasicBotCharacter::StaticClass();
    int32 SpawnedCount = 0;

    for (int32 BotIndex = 0; BotIndex < BotsToSpawn; ++BotIndex)
    {
        const FTransform SpawnTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 250.0f));
        AWTBRBasicBotCharacter* Bot = World->SpawnActorDeferred<AWTBRBasicBotCharacter>(
            SpawnClass,
            SpawnTransform,
            nullptr,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!IsValid(Bot))
        {
            UE_LOG(LogTemp, Warning, TEXT("WTBR 15P bot fill: failed to spawn bot %d/%d."), BotIndex + 1, BotsToSpawn);
            continue;
        }

        Bot->ApprovedLoadoutSet = BotLoadoutSet;
        Bot->FinishSpawning(SpawnTransform);
        ++SpawnedCount;
    }

    UE_LOG(LogTemp, Log, TEXT("WTBR 15P bot fill: spawned %d/%d bots; they will choose approved loadouts during LoadoutSetup and scatter with the human player at InMatch."),
        SpawnedCount,
        BotsToSpawn);
}
