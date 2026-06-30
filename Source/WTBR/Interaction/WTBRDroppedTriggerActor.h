// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "WTBRDroppedTriggerActor.generated.h"

class USceneComponent;

UCLASS()
class WTBR_API AWTBRDroppedTriggerActor : public AActor
{
	GENERATED_BODY()

public:
	AWTBRDroppedTriggerActor();

	void InitializeDroppedTrigger(
		const TSoftObjectPtr<UWTBRTriggerDataAsset>& InDroppedTriggerDataAsset,
		int32 InSourceSlotIndex,
		ETriggerCategory InCachedCategory);

	TSoftObjectPtr<UWTBRTriggerDataAsset> GetDroppedTriggerDataAsset() const { return DroppedTriggerDataAsset; }

	int32 GetSourceSlotIndex() const { return SourceSlotIndex; }

	ETriggerCategory GetCachedCategory() const { return CachedCategory; }

	bool IsConsumed() const { return bConsumed; }

	bool TryMarkConsumed();

	void ClearConsumedForFailedPickup();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WTBR | Dropped Trigger")
	TObjectPtr<USceneComponent> RootSceneComponent;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="WTBR | Dropped Trigger")
	TSoftObjectPtr<UWTBRTriggerDataAsset> DroppedTriggerDataAsset;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="WTBR | Dropped Trigger")
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="WTBR | Dropped Trigger")
	ETriggerCategory CachedCategory = ETriggerCategory::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="WTBR | Dropped Trigger")
	bool bConsumed = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
