// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRHUDViewModelComponent.h"

UWTBRHUDViewModelComponent::UWTBRHUDViewModelComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWTBRHUDViewModelComponent::BeginPlay()
{
    Super::BeginPlay();
    CachedSnapshot = BuildDefaultSnapshot();
}

FWTBRHUDSnapshot UWTBRHUDViewModelComponent::GetHUDSnapshot() const
{
    return CachedSnapshot;
}

void UWTBRHUDViewModelComponent::RefreshHUDSnapshot()
{
    CachedSnapshot = BuildDefaultSnapshot();
    OnHUDSnapshotChanged.Broadcast();
}

bool UWTBRHUDViewModelComponent::RequestUseDisplayedQuickItem()
{
    // TODO(C2): route to the existing server-authoritative inventory use path
    // after the owning character/viewmodel wiring is approved.
    return false;
}

bool UWTBRHUDViewModelComponent::RequestPickupFocusedTarget()
{
    // TODO(C2): route to the existing context-interact request path after the
    // owning character/viewmodel wiring is approved.
    return false;
}

bool UWTBRHUDViewModelComponent::RequestCancelCurrentUIOrAction()
{
    // TODO(C2/C3): wire only to the existing cancel input path once the exact
    // owner relationship is approved. This wrapper must not decide gameplay
    // state or directly mutate UI/gameplay-owned data.
    return false;
}

FWTBRHUDSnapshot UWTBRHUDViewModelComponent::BuildDefaultSnapshot() const
{
    return FWTBRHUDSnapshot();
}
