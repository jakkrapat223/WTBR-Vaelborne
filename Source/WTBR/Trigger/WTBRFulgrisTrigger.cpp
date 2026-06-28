// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRFulgrisTrigger.h"
#include "WTBRValidationLog.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"
#include "Engine/World.h"

bool UWTBRFulgrisTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    UWTBRVaelComponent* Vael = OwnerCharacter.IsValid()
        ? OwnerCharacter->VaelComponent
        : nullptr;
    const float CurrentVael = IsValid(Vael) ? Vael->GetCurrentVael() : -1.0f;
    const bool bHasAuthority =
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority();

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Activate Start | Owner=%s | HasAuthority=%s | Main=%s | CurrentVael=%.2f"),
        *GetNameSafe(OwnerCharacter.Get()),
        bHasAuthority ? TEXT("true") : TEXT("false"),
        TEXT("unknown"),
        CurrentVael);

    if (!OwnerCharacter.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=OwnerInvalid"));
        return false;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=NoAuthority"));
        return false;
    }
    if (!IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=DataAssetInvalid"));
        return false;
    }

    const TSubclassOf<AWTBRProjectileBase> ProjClass = DataAsset->FulgrisParams.FulgrisProjectileClass;
    const float Damage    = DataAsset->FulgrisParams.FulgrisDamage;
    const float Speed     = DataAsset->FulgrisParams.FulgrisSpeed;
    const float Range     = DataAsset->FulgrisParams.FulgrisRange;
    const int32 CubeSplit = DataAsset->FulgrisParams.FulgrisCubeSplitCount;
    const float Cooldown  = DataAsset->FulgrisParams.FulgrisFireCooldown;
    const float VaelCost  = DataAsset->VaelCostPerUse;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Data | Damage=%.1f | VaelCost=%.1f | Cooldown=%.3f | ProjectileClass=%s | Range=%.1f | Speed=%.1f"),
        Damage,
        VaelCost,
        Cooldown,
        *GetNameSafe(ProjClass.Get()),
        Range,
        Speed);

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=WorldInvalid"));
        return false;
    }

    const float CurrentTime = World->GetTimeSeconds();
    const float TimerElapsed = World->GetTimerManager().GetTimerElapsed(CooldownTimer);
    const float LastFire = (TimerElapsed >= 0.0f) ? CurrentTime - TimerElapsed : -1.0f;
    const float TimeSinceLastFire = (TimerElapsed >= 0.0f) ? TimerElapsed : 9999.0f;
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] CooldownCheck | CurrentTime=%.3f | LastFire=%.3f | TimeSinceLastFire=%.3f | Cooldown=%.3f"),
        CurrentTime,
        LastFire,
        TimeSinceLastFire,
        Cooldown);

    if (IsOnCooldown())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] CooldownBlocked | TimeSinceLastFire=%.3f | Cooldown=%.3f"),
            TimeSinceLastFire,
            Cooldown);
        return false;
    }

    if (!IsValid(ProjClass))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=ProjectileClassNull"));
        return false;
    }

    if (!IsValid(Vael))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=VaelComponentInvalid"));
        return false;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] ConsumeCheck | CurrentVael=%.2f | Cost=%.2f"),
        Vael->GetCurrentVael(),
        VaelCost);

    if (!Vael->TryConsumeVael(VaelCost))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] ConsumeFail | CurrentVael=%.2f | Cost=%.2f"),
            Vael->GetCurrentVael(),
            VaelCost);
        return false;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] ConsumeSuccess | NewVael=%.2f | Cost=%.2f"),
        Vael->GetCurrentVael(),
        VaelCost);

    if (!FireSniper(ProjClass, Damage, Speed, Range, false, CubeSplit))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=SpawnFailed"));
        return false;
    }
    StartCooldown();
    return true;
}

float UWTBRFulgrisTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 1.0f;
    return DataAsset->FulgrisParams.FulgrisFireCooldown;
}
