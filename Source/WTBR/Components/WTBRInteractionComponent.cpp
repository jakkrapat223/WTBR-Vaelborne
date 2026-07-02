// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRInteractionComponent.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "WTBRCharacter.h"

UWTBRInteractionComponent::UWTBRInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

AWTBRCorpseLootContainerActor* UWTBRInteractionComponent::GetFocusedCorpseLootContainer() const
{
    const AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
    {
        return nullptr;
    }

    UWorld* World = OwnerCharacter->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    if (AController* Controller = OwnerCharacter->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        OwnerCharacter->GetActorEyesViewPoint(ViewLocation, ViewRotation);
    }

    const FVector TraceStart = ViewLocation;
    const FVector TraceEnd = TraceStart + (ViewRotation.Vector() * InteractionTraceDistance);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WTBRInteractionFocusedCorpseLootContainer), false, OwnerCharacter);
    QueryParams.AddIgnoredActor(OwnerCharacter);

    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(
        Hit,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams);

    return bHit ? Cast<AWTBRCorpseLootContainerActor>(Hit.GetActor()) : nullptr;
}

FText UWTBRInteractionComponent::GetFocusedInteractionPromptText() const
{
    const AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
    {
        return FText::GetEmpty();
    }

    const AWTBRCorpseLootContainerActor* FocusedContainer = GetFocusedCorpseLootContainer();
    if (!IsValid(FocusedContainer) || !FocusedContainer->CanBeInteractedWithBy(OwnerCharacter))
    {
        return FText::GetEmpty();
    }

    return FocusedContainer->GetInteractionPromptText();
}

bool UWTBRInteractionComponent::RequestCorpseLootInteract()
{
    const AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
    {
        return false;
    }

    AWTBRCorpseLootContainerActor* FocusedContainer = GetFocusedCorpseLootContainer();
    if (!IsValid(FocusedContainer))
    {
        return false;
    }

    if (!FocusedContainer->CanBeInteractedWithBy(OwnerCharacter))
    {
        return false;
    }

    OnCorpseLootInteractRequested.Broadcast(FocusedContainer);
    return true;
}
