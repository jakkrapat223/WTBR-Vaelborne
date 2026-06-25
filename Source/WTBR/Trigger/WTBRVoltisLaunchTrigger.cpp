// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRVoltisLaunchTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

void UWTBRVoltisLaunchTrigger::InitializeTrigger(
    AWTBRCharacter* InOwnerCharacter,
    UWTBRTriggerDataAsset* InDataAsset)
{
    Super::InitializeTrigger(InOwnerCharacter, InDataAsset);
    if (!IsValid(InDataAsset)) return;

    RemainingLaunches = InDataAsset->VoltisParams.MaxAirLaunches;

    if (OwnerCharacter.IsValid())
    {
        ACharacter* Char = Cast<ACharacter>(OwnerCharacter.Get());
        if (IsValid(Char))
        {
            Char->LandedDelegate.AddDynamic(
                this, &UWTBRVoltisLaunchTrigger::OnCharacterLanded);
        }
    }
}

void UWTBRVoltisLaunchTrigger::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRVoltisLaunchTrigger, bIsStaggered);
}

bool UWTBRVoltisLaunchTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset)) return false;
    if (bIsStaggered) return false;
    if (RemainingLaunches <= 0) return false;

    if (!Super::Activate_Implementation(InputValue, bIsDualWield))
        return false;

    // Build launch direction from movement input vector
    FVector InputVec = OwnerCharacter->GetCharacterMovement()->GetLastInputVector();
    const float V = DataAsset->VoltisParams.VerticalLaunchForce;

    FVector LaunchDir;
    if (InputVec.SizeSquared2D() > 0.01f)
    {
        const FVector HorizDir = InputVec.GetSafeNormal2D();
        const float H = DataAsset->VoltisParams.HorizontalLaunchForce;
        LaunchDir = (HorizDir * H) + FVector(0.0f, 0.0f, V);
    }
    else
    {
        LaunchDir = FVector(0.0f, 0.0f, V);
    }

    if (HasCeilingNearby(LaunchDir))
    {
        bLastLandingWasCeilingBounce = true;
        ACharacter* Char = Cast<ACharacter>(OwnerCharacter.Get());
        if (IsValid(Char) && IsValid(Char->GetCharacterMovement()))
        {
            FVector Vel = Char->GetCharacterMovement()->Velocity;
            Vel.Z = 0.0f;
            Char->GetCharacterMovement()->Velocity = Vel;
        }
        return false;
    }

    PerformLaunch(bIsDualWield, LaunchDir);
    RemainingLaunches--;
    OnVoltisLaunch.Broadcast(bIsDualWield);
    OnVoltisLaunched(bIsDualWield);
    return true;
}

bool UWTBRVoltisLaunchTrigger::HasCeilingNearby(const FVector& LaunchDir) const
{
    if (!OwnerCharacter.IsValid() || !GetWorld()) return false;

    const FVector Start = OwnerCharacter->GetActorLocation();
    const FVector End   = Start + (LaunchDir.GetSafeNormal() * MIN_CEILING_CLEARANCE);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter.Get());

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit, Start, End, ECC_WorldStatic, Params);

#if ENABLE_DRAW_DEBUG
    DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End,
        bHit ? FColor::Red : FColor::Green, false, 0.5f, 0, 2.0f);
#endif

    return bHit;
}

void UWTBRVoltisLaunchTrigger::PerformLaunch(bool bIsDualWield, const FVector& LaunchDir)
{
    if (!OwnerCharacter.IsValid() || !IsValid(DataAsset)) return;
    ACharacter* Char = Cast<ACharacter>(OwnerCharacter.Get());
    if (!IsValid(Char)) return;

    Char->LaunchCharacter(LaunchDir, true, true);
}

void UWTBRVoltisLaunchTrigger::OnCharacterLanded(const FHitResult& Hit)
{
    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->HasAuthority()) return;

    if (IsValid(DataAsset))
        RemainingLaunches = DataAsset->VoltisParams.MaxAirLaunches;

    if (bLastLandingWasCeilingBounce)
    {
        bLastLandingWasCeilingBounce = false;
        StartStagger();
        OnVoltisLand.Broadcast(true);
        OnVoltisHardLanding();
    }
    else
    {
        OnVoltisLand.Broadcast(false);
    }
}

void UWTBRVoltisLaunchTrigger::StartStagger()
{
    if (!GetWorld()) return;
    bIsStaggered = true;
    GetWorld()->GetTimerManager().SetTimer(
        StaggerTimer,
        this, &UWTBRVoltisLaunchTrigger::OnStaggerExpired,
        STAGGER_DURATION,
        false);
}

void UWTBRVoltisLaunchTrigger::OnStaggerExpired()
{
    bIsStaggered = false;
}

void UWTBRVoltisLaunchTrigger::OnRep_bIsStaggered()
{
    // Blueprint: bIsStaggered=true → disable input + play stagger anim
    // Blueprint: bIsStaggered=false → re-enable input
}
