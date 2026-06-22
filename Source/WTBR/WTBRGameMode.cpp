// Copyright Epic Games, Inc. All Rights Reserved.

#include "WTBRGameMode.h"
#include "WTBRCharacter.h"
#include "UObject/ConstructorHelpers.h"

AWTBRGameMode::AWTBRGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
