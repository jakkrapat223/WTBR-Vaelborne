// Copyright Vaelborne: Dominion. All Rights Reserved.

// Include TriggerSetComponent in .cpp only — avoids circular header dependency
#include "Components/WTBRInputGestureComponent.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Trigger/WTBRTriggerBase.h"
#include "GameFramework/Actor.h"

UWTBRInputGestureComponent::UWTBRInputGestureComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWTBRInputGestureComponent::BeginPlay()
{
    Super::BeginPlay();
    if (AActor* Owner = GetOwner())
    {
        TriggerSetComp = Owner->FindComponentByClass<UWTBRTriggerSetComponent>();
    }
}

void UWTBRInputGestureComponent::NotifyMainPressed()
{
    bMainDown       = true;
    bMainFired      = false;
    MainHoldStartTime = GetWorld()->GetTimeSeconds();
    CheckDualGesture();
}

void UWTBRInputGestureComponent::NotifyMainReleased()
{
    if (!bMainFired)
    {
        const float HoldDuration = GetWorld()->GetTimeSeconds() - MainHoldStartTime;
        OnGestureDetected.Broadcast(
            HoldDuration >= HoldThreshold ? EWTBRInputGesture::MainHold : EWTBRInputGesture::MainTap);
    }
    bMainDown  = false;
    bMainFired = false;
}

void UWTBRInputGestureComponent::NotifySubPressed()
{
    bSubDown       = true;
    bSubFired      = false;
    SubHoldStartTime = GetWorld()->GetTimeSeconds();
    CheckDualGesture();
}

void UWTBRInputGestureComponent::NotifySubReleased()
{
    if (!bSubFired)
    {
        const float HoldDuration = GetWorld()->GetTimeSeconds() - SubHoldStartTime;
        OnGestureDetected.Broadcast(
            HoldDuration >= HoldThreshold ? EWTBRInputGesture::SubHold : EWTBRInputGesture::SubTap);
    }
    bSubDown  = false;
    bSubFired = false;
}

void UWTBRInputGestureComponent::CheckDualGesture()
{
    if (!bMainDown || !bSubDown || !TriggerSetComp) return;

    const UWTBRTriggerBase* Main = TriggerSetComp->GetActiveMainTrigger();
    const UWTBRTriggerBase* Sub  = TriggerSetComp->GetActiveSubTrigger();

    // GDD §3.4 Lock — Pure Type-Match only, no priority or timing check
    if (Main && Sub && Main->Category == Sub->Category)
    {
        bMainFired = true;
        bSubFired  = true;
        OnGestureDetected.Broadcast(EWTBRInputGesture::DualTrigger);
    }
}
