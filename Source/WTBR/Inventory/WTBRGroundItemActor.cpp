// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Inventory/WTBRGroundItemActor.h"

#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"

AWTBRGroundItemActor::AWTBRGroundItemActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
    SetRootComponent(RootSceneComponent);
}

void AWTBRGroundItemActor::InitializeGroundItem(const TSoftObjectPtr<UWTBRItemDataAsset>& InItemData, int32 InQuantity)
{
    if (!HasAuthority())
    {
        return;
    }

    ItemData = InItemData;
    Quantity = InQuantity;
}

bool AWTBRGroundItemActor::TryMarkConsumed()
{
    if (!HasAuthority() || bConsumed)
    {
        return false;
    }

    bConsumed = true;
    return true;
}

void AWTBRGroundItemActor::ClearConsumedForFailedPickup()
{
    if (!HasAuthority())
    {
        return;
    }

    bConsumed = false;
}

void AWTBRGroundItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AWTBRGroundItemActor, ItemData);
    DOREPLIFETIME(AWTBRGroundItemActor, Quantity);
    DOREPLIFETIME(AWTBRGroundItemActor, bConsumed);
}
