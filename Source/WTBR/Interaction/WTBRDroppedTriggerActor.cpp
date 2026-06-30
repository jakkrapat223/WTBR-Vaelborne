// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Interaction/WTBRDroppedTriggerActor.h"

#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"

AWTBRDroppedTriggerActor::AWTBRDroppedTriggerActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);
}

void AWTBRDroppedTriggerActor::InitializeDroppedTrigger(
	const TSoftObjectPtr<UWTBRTriggerDataAsset>& InDroppedTriggerDataAsset,
	int32 InSourceSlotIndex,
	ETriggerCategory InCachedCategory)
{
	if (!HasAuthority())
	{
		return;
	}

	DroppedTriggerDataAsset = InDroppedTriggerDataAsset;
	SourceSlotIndex = InSourceSlotIndex;
	CachedCategory = InCachedCategory;
}

bool AWTBRDroppedTriggerActor::TryMarkConsumed()
{
	if (!HasAuthority() || bConsumed)
	{
		return false;
	}

	bConsumed = true;
	return true;
}

void AWTBRDroppedTriggerActor::ClearConsumedForFailedPickup()
{
	if (!HasAuthority())
	{
		return;
	}

	bConsumed = false;
}

void AWTBRDroppedTriggerActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWTBRDroppedTriggerActor, DroppedTriggerDataAsset);
	DOREPLIFETIME(AWTBRDroppedTriggerActor, SourceSlotIndex);
	DOREPLIFETIME(AWTBRDroppedTriggerActor, CachedCategory);
	DOREPLIFETIME(AWTBRDroppedTriggerActor, bConsumed);
}
