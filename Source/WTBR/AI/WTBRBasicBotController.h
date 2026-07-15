// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "WTBRBasicBotController.generated.h"

class AWTBRCharacter;
class UWTBRTriggerDataAsset;
class UWTBRAegornTrigger;
enum class ETriggerCategory : uint8;

// Deliberately simple server-only combat bot. It follows radar-visible enemies,
// moves to an archetype range, and taps the active Main Trigger. It does not use
// cover, dodge, holds, trigger switching, or squad tactics.
UCLASS(BlueprintType)
class WTBR_API AWTBRBasicBotController : public AAIController
{
    GENERATED_BODY()

public:
    AWTBRBasicBotController();

    UFUNCTION(BlueprintPure, Category="WTBR | Bot")
    AWTBRCharacter* GetCombatTarget() const { return CombatTarget.Get(); }

#if WITH_DEV_AUTOMATION_TESTS
    // Test-only: runs the targeting step directly, without going through Tick()
    // (which needs a possessed-pawn tick lifecycle headless fixtures don't run).
    void AcquireClosestRadarContactForTest(AWTBRCharacter* BotCharacter) { AcquireClosestRadarContact(BotCharacter); }
#endif

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Bot", meta=(ClampMin="0.05"))
    float DecisionInterval = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Bot", meta=(ClampMin="0.1"))
    float FireInterval = 0.75f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Bot", meta=(ClampMin="100.0"))
    float RadarSearchRange = 6000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Bot | Defense", meta=(ClampMin="100.0"))
    float AegornDefenseRange = 800.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Bot | Defense", meta=(ClampMin="0.0", ClampMax="1.0"))
    float AegornLowHealthFraction = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Bot | Defense", meta=(ClampMin="0.1"))
    float AegornDefenseInterval = 2.0f;

private:
    TWeakObjectPtr<AWTBRCharacter> CombatTarget;
    float NextDecisionTime = 0.0f;
    float NextFireTime = 0.0f;
    float NextDefenseTime = 0.0f;

    void AcquireClosestRadarContact(AWTBRCharacter* BotCharacter);
    void UpdateCombat(AWTBRCharacter* BotCharacter, float Now);
    bool HasLockedAegornLoadout(const AWTBRCharacter* BotCharacter, UWTBRAegornTrigger*& OutAegorn) const;
    void TryRaiseAegorn(AWTBRCharacter* BotCharacter, UWTBRAegornTrigger* Aegorn, float TargetDistance, float Now);
    float GetPreferredCombatRange(const UWTBRTriggerDataAsset* DataAsset) const;
    static bool IsCombatCategory(ETriggerCategory Category);
};
