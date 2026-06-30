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
    None         UMETA(DisplayName="None"),
    DualMelee    UMETA(DisplayName="Dual Melee"),
    DualGunner   UMETA(DisplayName="Dual Gunner"),
    DualMovement UMETA(DisplayName="Dual Movement"),
    DualDefense  UMETA(DisplayName="Dual Defense"),
    DualSniper   UMETA(DisplayName="Dual Sniper"),
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
    ETriggerCategory CachedCategory = ETriggerCategory::None;

    bool IsEmpty()  const { return DataAsset.IsNull(); }
    bool IsLoaded() const { return DataAsset.IsValid() && !DataAsset.IsNull(); }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDualWieldStateChanged, EWTBRDualWieldState, NewState, ETriggerCategory, ActiveCategory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveTriggerChanged, ETriggerSlot, NewSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSubTriggerEquipped, UWTBRTriggerBase*, Trigger);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSubTriggerUnequipped, UWTBRTriggerBase*, Trigger);

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

    // Fired when a Sub-Trigger becomes active (e.g. Vexorn equip → RegisterSignalBlock)
    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnSubTriggerEquipped OnSubTriggerEquipped;

    // Fired when the active Sub-Trigger is replaced (e.g. Vexorn unequip → UnregisterSignalBlock)
    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnSubTriggerUnequipped OnSubTriggerUnequipped;

    UFUNCTION(BlueprintCallable, Category="TriggerSet")
    void InstallTrigger(ETriggerSlot Slot, UWTBRTriggerBase* Trigger);

    UFUNCTION(BlueprintCallable, Category="TriggerSet")
    void SwitchMainSlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category="TriggerSet")
    void SwitchSubSlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category="TriggerSet")
    void CycleMainSlot();

    UFUNCTION(BlueprintCallable, Category="TriggerSet")
    void CycleSubSlot();

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    UWTBRTriggerBase* GetActiveMainTrigger() const;

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    UWTBRTriggerBase* GetActiveSubTrigger() const;

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    UWTBRTriggerBase* GetTriggerInSlot(ETriggerSlot Slot) const;

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    int32 GetActiveMainIndex() const { return ActiveMainIndex; }

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    int32 GetActiveSubIndex() const { return ActiveSubIndex; }

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    UWTBRTriggerDataAsset* GetActiveMainDataAsset() const;

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    UWTBRTriggerDataAsset* GetActiveSubDataAsset() const;

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    EWTBRDualWieldState GetDualWieldState() const { return CurrentDualWieldState; }

    // Loadout assignment — called from PlayerController after character spawn
    UFUNCTION(Server, Reliable)
    void Server_SetTriggerLoadout(const TArray<TSoftObjectPtr<UWTBRTriggerDataAsset>>& InLoadout);

    // Serpveil: client sends charged params, server validates Vael and fires
    UFUNCTION(Server, Reliable)
    void Server_FireSerpveil(EWTBRSerpveilShape Shape, FRotator Direction, float ChargedRange, bool bIsMain);

    // TEMP_TEST46: server-authoritative Aegorn wall spawn hook for PIE/BP testing only.
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "WTBR | Debug")
    void Server_TEMP_TEST46_PlaceAegornWall(bool bIsMain);

    // ── Composite Bullet Merge ────────────────────────────────────────────────

    // Replicated merge state — clients read this for VFX/UI; server drives it
    UPROPERTY(ReplicatedUsing = OnRep_MergeState, BlueprintReadOnly,
        Category = "WTBR | Composite")
    EWTBRCompositeBulletType CurrentMergeState = EWTBRCompositeBulletType::None;

    // Client → Server: request merge. Server deducts Vael immediately and starts timer.
    UFUNCTION(Server, Reliable)
    void Server_StartCompositeMerge(EWTBRCompositeBulletType Type);

    // Authority-only cancel — called on damage or stagger; Vael is NOT refunded.
    UFUNCTION(BlueprintCallable, Category = "WTBR | Composite")
    void CancelMerge();

    UFUNCTION(BlueprintPure, Category = "WTBR | Composite")
    EWTBRCompositeBulletType GetCurrentMergeState() const { return CurrentMergeState; }

    // ── VFX Hooks (implement in Blueprint) ───────────────────────────────────
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Composite | VFX")
    void OnMergeStart(EWTBRCompositeBulletType Type);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Composite | VFX")
    void OnMergeComplete(EWTBRCompositeBulletType Type);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Composite | VFX")
    void OnMergeCancelled();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // EditDefaultsOnly added: allows designers to set loadout directly
    // in BP_WTBRCharacter Details panel without Blueprint scripting.
    // ReplicatedUsing retained: clients still get OnRep_ for cosmetic updates.
    UPROPERTY(EditDefaultsOnly, ReplicatedUsing = OnRep_TriggerSlots,
        BlueprintReadOnly, Category = "WTBR | Trigger Set | Slots",
        meta = (AllowPrivateAccess = "true"))
    TArray<FWTBRTriggerSlot> TriggerSlots;

    UPROPERTY(ReplicatedUsing=OnRep_ActiveMainIndex)
    int32 ActiveMainIndex = 0;

    UPROPERTY(ReplicatedUsing=OnRep_ActiveSubIndex)
    int32 ActiveSubIndex  = 4;

    UPROPERTY(ReplicatedUsing=OnRep_DualWieldState)
    EWTBRDualWieldState CurrentDualWieldState = EWTBRDualWieldState::None;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UWTBRTriggerBase>> RuntimeTriggers;

    // Tracks in-flight async load handles per slot index.
    // Key = slot index (0-7). Value = streamable handle.
    // Stored here (not in FWTBRTriggerSlot) to avoid UStruct serialization issues
    // with TSharedPtr. Allows CancelHandle() on rapid slot switching.
    // TMap is NOT replicated. Server uses it for gameplay slot loads; clients
    // use it only for HUD/name DataAsset loads after TriggerSlots replication.
    TMap<int32, TSharedPtr<FStreamableHandle>> PendingSlotLoads;

    bool IsValidSlotIndex(int32 Index) const { return TriggerSlots.IsValidIndex(Index); }
    bool HasServerAuthority() const;
    bool CanMutateTriggerLoadout() const;

    // Fires OnUnequipped on OldIdx trigger then OnEquipped on NewIdx trigger (both delegates + virtual call)
    void NotifySubSlotChanged(int32 OldAbsIdx, int32 NewAbsIdx);

    void AsyncLoadSlot(int32 SlotIndex, TFunction<void()> OnComplete);

    void RequestClientSlotDataAssetLoad(int32 SlotIndex, const TCHAR* Reason);

    void InitializeLoadedSlot(int32 SlotIndex);

    // Synchronous fallback — creates RuntimeTrigger when DataAsset is already in memory.
    // Called from CycleMainSlot/CycleSubSlot so the trigger is ready immediately
    // without waiting for the next async-load callback.
    void InstantiateRuntimeTrigger(int32 SlotIndex);

    // ── Composite merge internals (server-only) ───────────────────────────────
    FTimerHandle MergeTimer;

    // Replicated alongside CurrentMergeState so OnRep_MergeState can distinguish
    // a successful fire (true) from a forced cancel (false) without an extra RPC.
    UPROPERTY(Replicated)
    bool bMergeWasFired = false;

    UFUNCTION()
    void OnRep_MergeState(EWTBRCompositeBulletType OldState);

    UFUNCTION()
    void OnMergeCompleteCallback();

    void FireComposite(EWTBRCompositeBulletType Type);

    UFUNCTION()
    void OnRep_TriggerSlots();

    UFUNCTION()
    void OnRep_ActiveMainIndex();

    UFUNCTION()
    void OnRep_ActiveSubIndex();

    UFUNCTION()
    void OnRep_DualWieldState();

    void UpdateDualWieldState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
