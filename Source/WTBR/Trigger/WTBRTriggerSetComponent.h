// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/StreamableManager.h"
#include "Trigger/WTBRTriggerBase.h"      // ETriggerCategory, UWTBRTriggerBase
#include "Trigger/WTBRTriggerDataAsset.h" // full type required for TSoftObjectPtr<T>::Get()
#include "WTBRTriggerSetComponent.generated.h"

// 8-Slot system: Main[0..3] + Sub[4..7]
UENUM(BlueprintType)
enum class ETriggerSlot : uint8
{
    Main1 = 0, Main2 = 1, Main3 = 2, Main4 = 3,
    Sub1  = 4, Sub2  = 5, Sub3  = 6, Sub4  = 7,
};

// Dual Wield state: Pure Type-Match only — no Priority/Timing (GDD §3.4 Lock)
UENUM(BlueprintType)
enum class EWTBRDualWieldState : uint8
{
    None        UMETA(DisplayName="None"),
    DualMelee   UMETA(DisplayName="Dual Melee"),
    DualGunner  UMETA(DisplayName="Dual Gunner"),
    DualShield  UMETA(DisplayName="Dual Shield"),
    DualTrap    UMETA(DisplayName="Dual Trap"),
    DualSniper  UMETA(DisplayName="Dual Sniper"),
};

// Per-slot data. TSoftObjectPtr for DataAsset = lazy load only.
// TSharedPtr<FStreamableHandle> intentionally NOT here — stored in PendingSlotLoads TMap
// on the component to avoid UStruct serialization issues with TSharedPtr.
USTRUCT(BlueprintType)
struct FWTBRTriggerSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UWTBRTriggerDataAsset> DataAsset;

    UPROPERTY()
    TObjectPtr<UWTBRTriggerBase> RuntimeTrigger = nullptr;

    UPROPERTY()
    ETriggerCategory CachedCategory = ETriggerCategory::Melee;

    bool IsEmpty()  const { return DataAsset.IsNull(); }
    bool IsLoaded() const { return DataAsset.IsValid() && !DataAsset.IsNull(); }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDualWieldStateChanged, EWTBRDualWieldState, NewState, ETriggerCategory, ActiveCategory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveTriggerChanged, ETriggerSlot, NewSlot);

UCLASS(ClassGroup=(WTBR), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRTriggerSetComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRTriggerSetComponent();

    static const int32 MainSlotCount  = 4;
    static const int32 SubSlotCount   = 4;
    static const int32 TotalSlotCount = 8;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnDualWieldStateChanged OnDualWieldStateChanged;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnActiveTriggerChanged OnActiveTriggerChanged;

    UFUNCTION(BlueprintCallable, Category="TriggerSet")
    void InstallTrigger(ETriggerSlot Slot, UWTBRTriggerBase* Trigger);

    UFUNCTION(BlueprintCallable, Category="TriggerSet")
    void SwitchMainSlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category="TriggerSet")
    void SwitchSubSlot(int32 SlotIndex);

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    UWTBRTriggerBase* GetActiveMainTrigger() const;

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    UWTBRTriggerBase* GetActiveSubTrigger() const;

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    UWTBRTriggerBase* GetTriggerInSlot(ETriggerSlot Slot) const;

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    EWTBRDualWieldState GetDualWieldState() const { return CurrentDualWieldState; }

    // Loadout assignment — called from PlayerController after character spawn
    UFUNCTION(Server, Reliable)
    void Server_SetTriggerLoadout(const TArray<TSoftObjectPtr<UWTBRTriggerDataAsset>>& InLoadout);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Replicated)
    TArray<FWTBRTriggerSlot> TriggerSlots;

    UPROPERTY(Replicated)
    int32 ActiveMainIndex = 0;

    UPROPERTY(Replicated)
    int32 ActiveSubIndex  = 4;

    UPROPERTY(ReplicatedUsing=OnRep_DualWieldState)
    EWTBRDualWieldState CurrentDualWieldState = EWTBRDualWieldState::None;

    // Tracks in-flight async load handles per slot index.
    // Key = slot index (0-7). Value = streamable handle.
    // Stored here (not in FWTBRTriggerSlot) to avoid UStruct serialization issues
    // with TSharedPtr. Allows CancelHandle() on rapid slot switching.
    // Tech Director Note: TMap is NOT replicated — server-side only. Correct.
    TMap<int32, TSharedPtr<FStreamableHandle>> PendingSlotLoads;

    bool IsValidSlotIndex(int32 Index) const { return TriggerSlots.IsValidIndex(Index); }

    void AsyncLoadSlot(int32 SlotIndex, TFunction<void()> OnComplete);

    UFUNCTION()
    void OnRep_DualWieldState();

    void UpdateDualWieldState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
