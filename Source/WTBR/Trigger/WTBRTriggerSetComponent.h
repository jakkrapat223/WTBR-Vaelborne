// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/StreamableManager.h"
#include "Trigger/WTBRTriggerBase.h"      // ETriggerCategory, UWTBRTriggerBase
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRTriggerDataAsset.h" // full type required for TSoftObjectPtr<T>::Get()
#include "Misc/Guid.h"
#include "WTBRTriggerSetComponent.generated.h"

class UWTBRMantornTrigger;

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

// Server-only snapshot of the state that started a composite merge — captured once at merge
// start so FireComposite never re-reads live, possibly-changed slot state. Deliberately
// captures slot index + archetype (NOT a raw DataAsset pointer) so a mid-merge slot swap can
// be detected and refunded instead of silently firing mismatched data.
struct FWTBRCompositeMergeSnapshot
{
    EWTBRCompositeBulletType CompositeType = EWTBRCompositeBulletType::None;
    int32 MainSlotIndex = INDEX_NONE;
    int32 SubSlotIndex = INDEX_NONE;
    EWTBRBulletArchetype MainArchetype = EWTBRBulletArchetype::None;
    EWTBRBulletArchetype SubArchetype = EWTBRBulletArchetype::None;
    FGuid ReservationHandle;
    float MergeStartServerTime = 0.0f;
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

    // Trigger Option attached to this slot (canon: Senkū attaches to Kogetsu's
    // own slot, doesn't occupy a separate slot or count toward the 2-trigger
    // limit). Generic — any future Trigger+Option pairing can reuse this field;
    // only Lacern's Hold logic reads it today (Arcven/Senku).
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UWTBRTriggerDataAsset> OptionDataAsset;

    UPROPERTY()
    ETriggerCategory CachedCategory = ETriggerCategory::None;

    bool IsEmpty()  const { return DataAsset.IsNull(); }
    bool IsLoaded() const { return DataAsset.IsValid() && !DataAsset.IsNull(); }
};

USTRUCT(BlueprintType)
struct FWTBRInstalledTriggerSlotSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Trigger Set")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Trigger Set")
    TSoftObjectPtr<UWTBRTriggerDataAsset> DataAsset;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Trigger Set")
    ETriggerCategory CachedCategory = ETriggerCategory::None;

    bool IsValid() const { return SlotIndex != INDEX_NONE && !DataAsset.IsNull(); }
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

    // Fired when a Sub-Trigger becomes active (e.g. Vexorn equip → radar cloak on)
    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnSubTriggerEquipped OnSubTriggerEquipped;

    // Fired when the active Sub-Trigger is replaced (e.g. Vexorn unequip → radar cloak off)
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

    // Display name for any slot, including ones the player is not currently
    // holding — GetTriggerInSlot only returns a runtime Trigger for slots that
    // have been instantiated, which is not enough for UI that lists the whole set
    // (the selection wheel). Reads the slot's DataAsset if it is already loaded and
    // falls back to the asset name while the soft pointer is still resolving, so
    // the wheel never shows a blank segment for a slot that does have a Trigger.
    UFUNCTION(BlueprintPure, Category="TriggerSet")
    FText GetSlotDisplayName(int32 SlotIndex) const;

    // True when the slot has a Trigger assigned at all, loaded or not.
    UFUNCTION(BlueprintPure, Category="TriggerSet")
    bool IsSlotOccupied(int32 SlotIndex) const;

    // ── Trigger Option (canon: Senkū-style attachment to a Trigger's own slot) ──
    // Lazily instantiates the active Main/Sub slot's OptionDataAsset runtime
    // trigger the first time it's needed (Options are EditDefaultsOnly / design-
    // time authored, so unlike the main slot this skips the async-streaming
    // pipeline — same "already in memory" assumption InstantiateRuntimeTrigger
    // relies on for its synchronous fallback). Returns nullptr if no Option is
    // attached to that slot.
    UWTBRTriggerBase* GetActiveMainOptionTrigger();
    UWTBRTriggerBase* GetActiveSubOptionTrigger();

    UFUNCTION(BlueprintPure, Category="TriggerSet")
    EWTBRDualWieldState GetDualWieldState() const { return CurrentDualWieldState; }

    void GetInstalledTriggerSlotSnapshots(TArray<FWTBRInstalledTriggerSlotSnapshot>& OutSnapshots) const;
    bool GetTriggerSlotSnapshot(int32 SlotIndex, FWTBRInstalledTriggerSlotSnapshot& OutSnapshot) const;

    // UI-facing read-only compatibility query. Delegates to the server-side slot
    // constraint rules so corpse loot UI can check compatibility without duplicating logic.
    UFUNCTION(BlueprintPure, Category="TriggerSet | UI")
    bool IsDataAssetCompatibleWithTargetSlot(int32 SlotIndex, const UWTBRTriggerDataAsset* DataAsset) const;

#if WITH_DEV_AUTOMATION_TESTS
    // Test-only: write a soft DataAsset reference directly into a slot without runtime instantiation.
    void SetSlotDataAssetForTest(int32 SlotIndex, TSoftObjectPtr<UWTBRTriggerDataAsset> InDataAsset);
    // Test-only: write a soft OptionDataAsset reference directly into a slot.
    void SetSlotOptionDataAssetForTest(int32 SlotIndex, TSoftObjectPtr<UWTBRTriggerDataAsset> InOptionDataAsset);
    // Test-only: install a runtime trigger straight into RuntimeTriggers, bypassing
    // the CanMutateTriggerLoadout()/AWTBRGameState gate InstallTrigger enforces
    // (headless trigger-regression fixtures never spawn a GameState).
    void InstallTriggerForTest(ETriggerSlot Slot, UWTBRTriggerBase* Trigger);

    // Test-only: exposes the private static sanitizer directly, the same way other
    // Server_* validation logic in this codebase is unit-tested by calling
    // _Implementation directly rather than going over a real RPC channel.
    static bool SanitizeCustomVenyxPresetForTest(FWTBRPathPreset& Preset)
    {
        return SanitizeCustomVenyxPreset(Preset);
    }

    static void SanitizeCustomVenyxPresetListForTest(
        const TArray<FWTBRPathPreset>& InPresets, TArray<FWTBRPathPreset>& OutPresets)
    {
        SanitizeCustomVenyxPresetList(InPresets, OutPresets);
    }
#endif

    bool ReplaceTriggerSlotFromDataAsset(int32 SlotIndex, TSoftObjectPtr<UWTBRTriggerDataAsset> NewDataAsset);

    // Loadout assignment — called from PlayerController after character spawn
    UFUNCTION(Server, Reliable)
    void Server_SetTriggerLoadout(const TArray<TSoftObjectPtr<UWTBRTriggerDataAsset>>& InLoadout);

    // ── Preset Editor: player-authored custom Venyx presets (session-only) ──────
    //
    // Uploads this player's Preset-Editor-authored custom Venyx presets so a
    // remote listen-server/dedicated-server has a validated copy before any fire
    // request can reference an index into them (see
    // UWTBRVenyxTrigger::RefreshCustomHoldPresets). Every preset/lane/scalar is
    // clamped server-side on receipt — this is the first RPC in the project
    // carrying player-authored preset DATA instead of a trusted index, so nothing
    // here is trusted as-is.
    UFUNCTION(Server, Reliable)
    void Server_SetCustomVenyxPresets(const TArray<FWTBRPathPreset>& InCustomPresets);

    // Called once this player's pawn is locally controlled (BeginPlay): pulls
    // custom presets straight from this machine's UWTBRCustomPresetSubsystem (no
    // round trip needed for the local copy — it's the same process), refreshes any
    // already-instantiated local Venyx Trigger immediately, then uploads via the
    // RPC above so the server side (which may be a different machine/process) has
    // them too.
    void RefreshCustomVenyxPresetsFromLocalSubsystem();

    void DebugPrintTriggerLoadoutMutationGate() const;

    // Serpveil legacy release RPC: retained for compatibility; S1 ignores it.
    UFUNCTION(Server, Reliable)
    void Server_FireSerpveil(EWTBRSerpveilShape Shape, FRotator Direction, float ChargedRange, bool bIsMain);

    // TEMP_TEST46: server-authoritative Aegorn wall spawn hook for PIE/BP testing only.
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "WTBR | Debug")
    void Server_TEMP_TEST46_PlaceAegornWall(bool bIsMain);

    // ── Composite Bullet Merge ────────────────────────────────────────────────

    // Central composite recipe registry (Step 4) — resolves the two active archetypes into a
    // composite Type server-side. Author one shared asset and assign per-character/BP default.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Composite")
    TSoftObjectPtr<UWTBRCompositeRegistryDataAsset> CompositeRegistryAsset;

    // Replicated merge state — clients read this for VFX/UI; server drives it
    UPROPERTY(ReplicatedUsing = OnRep_MergeState, BlueprintReadOnly,
        Category = "WTBR | Composite")
    EWTBRCompositeBulletType CurrentMergeState = EWTBRCompositeBulletType::None;

    // Replicated for the HUD; the server retains the originating Main slot separately.
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "WTBR | Composite")
    EWTBRCompositeBulletType ReadyCompositeType = EWTBRCompositeBulletType::None;

    // Total merge duration replicated once at merge start for client-local HUD countdowns.
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "WTBR | Composite")
    float CompositeMergeDuration = 0.0f;

    // Replicated cooldown state for client HUD availability indicators.
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "WTBR | Composite")
    bool bCompositeCooldownActive = false;

    // Client → Server: request merge. Server resolves the active archetypes, validates the whole
    // definition, reserves Vael (not yet deducted), and starts the timer.
    UFUNCTION(Server, Reliable)
    void Server_StartCompositeMerge();

    // Authority-only cancel — called on damage or stagger; Vael is NOT refunded.
    UFUNCTION(BlueprintCallable, Category = "WTBR | Composite")
    void CancelMerge();

    // Authority-only: fires the already committed composite, then clears its ready state.
    bool FireReadyComposite();

    // Authority-only: discards an already committed composite without a Vael refund.
    void DiscardReadyComposite();

    // A match restart is a fresh round, so it must not inherit a successful-fire cooldown.
    void ClearCompositeCooldownForMatchRestart()
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(CompositeCooldownTimer);
        }
        bCompositeCooldownActive = false;
    }

    UFUNCTION(BlueprintPure, Category = "WTBR | Composite")
    EWTBRCompositeBulletType GetCurrentMergeState() const { return CurrentMergeState; }

    UFUNCTION(BlueprintPure, Category = "WTBR | Composite")
    bool HasReadyComposite() const { return ReadyCompositeType != EWTBRCompositeBulletType::None; }

    // Registry definition for the composite currently held ready, or null. Lets
    // callers ask what the composite IS (its archetypes, its balance) without
    // needing their own handle on the registry asset.
    const FWTBRCompositeDefinition* FindReadyCompositeDefinition() const;

    UFUNCTION(BlueprintPure, Category = "WTBR | Composite")
    EWTBRCompositeBulletType GetReadyCompositeType() const { return ReadyCompositeType; }

    // Client-safe read-only availability check. Server validation still owns Vael reservation.
    UFUNCTION(BlueprintPure, Category = "WTBR | Composite")
    bool CanStartMerge() const;

    // Test-only seam: invokes the merge-complete path directly without waiting for MergeTimer.
    UFUNCTION(BlueprintCallable, Category = "WTBR | Debug")
    void TriggerMergeCompleteForTest() { OnMergeCompleteCallback(); }

    UFUNCTION(BlueprintCallable, Category = "WTBR | Debug")
    void EndPlayForTest() { EndPlay(EEndPlayReason::Destroyed); }

    UFUNCTION(BlueprintPure, Category = "WTBR | Debug")
    bool HasPendingMergeReservationForTest() const { return ActiveMergeSnapshot.ReservationHandle.IsValid(); }

    UFUNCTION(BlueprintPure, Category = "WTBR | Debug")
    int32 GetActiveMergeMainSlotIndexForTest() const { return ActiveMergeSnapshot.MainSlotIndex; }

    UFUNCTION(BlueprintPure, Category = "WTBR | Debug")
    int32 GetActiveMergeSubSlotIndexForTest() const { return ActiveMergeSnapshot.SubSlotIndex; }

    UFUNCTION(BlueprintPure, Category = "WTBR | Debug")
    bool IsCompositeCooldownActiveForTest() const
    {
        return GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(CompositeCooldownTimer);
    }

    // ── VFX Hooks (implement in Blueprint) ───────────────────────────────────
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Composite | VFX")
    void OnMergeStart(EWTBRCompositeBulletType Type);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Composite | VFX")
    void OnMergeComplete(EWTBRCompositeBulletType Type);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Composite | VFX")
    void OnMergeCancelled();

    // ── Mantorn (Feryx + Feryx composite form) ────────────────────────────────

    // Client → Server: toggle Mantorn form. Server validates both active Main+Sub
    // are Feryx with bCanFormMantorn, charges TransformVaelCost on enter (never
    // refunds on exit). Sent by the gesture component on a simultaneous LMB+RMB hold.
    UFUNCTION(Server, Reliable)
    void Server_RequestMantornToggle();

    UFUNCTION(BlueprintPure, Category = "WTBR | Mantorn")
    bool IsMantornFormActive() const { return bMantornFormActive; }

    // Client-usable gate for the dual-hold toggle gesture: true if already in form
    // (hold-both exits) OR both active Main+Sub are Mantorn-capable Feryx (hold-both
    // enters). RuntimeTriggers exist on clients too, so this is valid client-side.
    UFUNCTION(BlueprintPure, Category = "WTBR | Mantorn")
    bool CanToggleMantornForm() const
    {
        return bMantornFormActive || AreBothActiveFeryxMantornCapable();
    }

    // Server-only accessor for the active form's behavior executor (whip/spin).
    // Null on clients and when not in form.
    UWTBRMantornTrigger* GetActiveMantornForm() const { return ActiveMantornForm; }

    // Authority-only force-exit — called on death/stun/loadout swap. No refund.
    void ExitMantornForm(const TCHAR* Reason);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Mantorn | VFX")
    void OnMantornFormEntered();

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Mantorn | VFX")
    void OnMantornFormExited();

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

    // Lazy cache for OptionDataAsset runtime triggers (Trigger Option system,
    // e.g. Arcven attached to a Lacern slot). Key = slot index. Populated on
    // first access by GetActive{Main,Sub}OptionTrigger() — Options are
    // EditDefaultsOnly/design-time authored, so no async-streaming pipeline
    // is needed (mirrors InstantiateRuntimeTrigger's synchronous-fallback
    // assumption for the main slot).
    UPROPERTY(Transient)
    TMap<int32, TObjectPtr<UWTBRTriggerBase>> RuntimeOptionTriggers;

    UWTBRTriggerBase* GetOrCreateOptionRuntimeTrigger(int32 SlotIndex);

    // Tracks in-flight async load handles per slot index.
    // Key = slot index (0-7). Value = streamable handle.
    // Stored here (not in FWTBRTriggerSlot) to avoid UStruct serialization issues
    // with TSharedPtr. Allows CancelHandle() on rapid slot switching.
    // TMap is NOT replicated. Server uses it for gameplay slot loads; clients
    // use it only for HUD/name DataAsset loads after TriggerSlots replication.
    TMap<int32, TSharedPtr<FStreamableHandle>> PendingSlotLoads;

    // This side's own belief about this player's custom Venyx presets — set
    // directly from UWTBRCustomPresetSubsystem when this side IS the locally
    // controlled owner, or from the validated RPC receipt otherwise. Deliberately
    // NOT replicated: nobody but the owning player and the server needs it, and
    // the owning client already has the authoritative copy in its own subsystem.
    UPROPERTY(Transient)
    TArray<FWTBRPathPreset> CachedCustomVenyxPresets;

    // Applies CachedCustomVenyxPresets to every currently-instantiated Venyx
    // Trigger in RuntimeTriggers (Main or Sub — a preset upload doesn't know or
    // care which slot Venyx occupies).
    void RefreshAllVenyxHoldPresetCaches();

    // Clamps one uploaded preset's fields in place to the ranges their own
    // UPROPERTY metadata documents (metadata is a details-panel constraint only —
    // it enforces nothing on data that arrives over an RPC) and truncates its lane
    // waypoints via UWTBRCompositeRegistryDataAsset::ClampLaneTurns. Returns false
    // if the preset should be dropped entirely (no PresetId, or every lane was
    // invalid after sanitizing).
    static bool SanitizeCustomVenyxPreset(FWTBRPathPreset& Preset);

    // Whole-list sanitize: applies the preset-count ceiling and drops every preset
    // SanitizeCustomVenyxPreset rejects.
    //
    // ⚠ Both the client-local path (RefreshCustomVenyxPresetsFromLocalSubsystem)
    // and the server receipt path (Server_SetCustomVenyxPresets) MUST go through
    // this, so the two sides hold arrays of identical length and order. The fire
    // RPC carries only an index; divergent arrays mean an index that resolves to a
    // different preset on each side. Idempotent, so running it twice is safe.
    static void SanitizeCustomVenyxPresetList(
        const TArray<FWTBRPathPreset>& InPresets, TArray<FWTBRPathPreset>& OutPresets);

    bool IsValidSlotIndex(int32 Index) const { return TriggerSlots.IsValidIndex(Index); }
    bool HasServerAuthority() const;
    bool CanMutateTriggerLoadout() const;
    bool IsDataAssetCompatibleWithSlot(int32 SlotIndex, const UWTBRTriggerDataAsset* DataAsset) const;
    void CleanupRuntimeTriggerForReplacement(int32 SlotIndex);

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

    FWTBRCompositeMergeSnapshot ActiveMergeSnapshot;

    // Server-only source slot captured when the merge completes.
    int32 ReadyCompositeMainSlotIndex = INDEX_NONE;

    // Cooldown after a successful composite fire (separate from any future cancel lockout) —
    // sourced from the registry Definition's CompositeCooldown; gates a NEW merge attempt.
    FTimerHandle CompositeCooldownTimer;

    // Discards a READY composite that was never fired, with no refund. Started when
    // the merge completes, cleared inside DiscardReadyComposite so every existing
    // discard path tears it down without needing its own call.
    FTimerHandle ReadyCompositeExpiryTimer;

    UFUNCTION()
    void OnReadyCompositeExpired();

    void OnCompositeCooldownExpired();

    // Replicated alongside CurrentMergeState so OnRep_MergeState can distinguish
    // a completed merge / ready bullet (true) from a forced cancel (false) without an extra RPC.
    UPROPERTY(Replicated)
    bool bMergeWasFired = false;

    UFUNCTION()
    void OnRep_MergeState(EWTBRCompositeBulletType OldState);

    UFUNCTION()
    void OnMergeCompleteCallback();

    void FireComposite(EWTBRCompositeBulletType Type, int32 MainSlotIndex);

    UFUNCTION()
    void OnRep_TriggerSlots();

    UFUNCTION()
    void OnRep_ActiveMainIndex();

    UFUNCTION()
    void OnRep_ActiveSubIndex();

    UFUNCTION()
    void OnRep_DualWieldState();

    void UpdateDualWieldState();

    // ── Mantorn form internals ────────────────────────────────────────────────

    // Replicated so clients drive form VFX via OnRep. Server-authoritative.
    UPROPERTY(ReplicatedUsing = OnRep_MantornForm)
    bool bMantornFormActive = false;

    // Server-only behavior executor for the active form (whip/spin). Constructed on
    // enter with the active Main Feryx DataAsset; cleared on exit.
    UPROPERTY(Transient)
    TObjectPtr<UWTBRMantornTrigger> ActiveMantornForm;

    UFUNCTION()
    void OnRep_MantornForm();

    // True only when BOTH active Main and active Sub are Feryx triggers whose
    // DataAsset has MantornParams.bCanFormMantorn = true.
    bool AreBothActiveFeryxMantornCapable() const;

    // Authority-only enter: validates, charges TransformVaelCost, spawns executor.
    bool EnterMantornForm();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
