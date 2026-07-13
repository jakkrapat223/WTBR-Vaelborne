// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRFulgrisTrigger.h"
#include "WTBRValidationLog.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"
#include "Engine/World.h"

bool UWTBRFulgrisTrigger::ExecuteFire()
{
    if (!IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=DataAssetInvalid"));
        return false;
    }

    UWTBRVaelComponent* Vael = OwnerCharacter->VaelComponent;
    if (!IsValid(Vael))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] Fail | Reason=VaelComponentInvalid"));
        return false;
    }

    const TSubclassOf<AWTBRProjectileBase> ProjClass = DataAsset->FulgrisParams.FulgrisProjectileClass;
    const float Damage    = DataAsset->FulgrisParams.FulgrisDamage;
    const float Speed     = DataAsset->FulgrisParams.FulgrisSpeed;
    const float Range     = DataAsset->FulgrisParams.FulgrisRange;
    const int32 CubeSplit = DataAsset->FulgrisParams.FulgrisCubeSplitCount;
    const float VaelCost  = DataAsset->VaelCostPerUse;

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
    return true;
}

float UWTBRFulgrisTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 1.0f;
    return DataAsset->FulgrisParams.FulgrisFireCooldown;
}

float UWTBRFulgrisTrigger::GetZoomFOV() const
{
    if (!IsValid(DataAsset)) return Super::GetZoomFOV();
    return DataAsset->FulgrisParams.FulgrisZoomFOV;
}
