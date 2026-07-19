// Copyright Vaelborne: Dominion Project. All Rights Reserved.

#include "Components/WTBRCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "WTBRCharacter.h"

UWTBRCharacterMovementComponent::UWTBRCharacterMovementComponent()
    : bStandingOrientRotationToMovement(1)
    , bStandingUseControllerDesiredRotation(0)
{
}

void UWTBRCharacterMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    // Captured here, not in the constructor: AWTBRCharacter's own constructor sets
    // bOrientRotationToMovement after this component is created.
    bStandingOrientRotationToMovement = bOrientRotationToMovement ? 1 : 0;
    bStandingUseControllerDesiredRotation = bUseControllerDesiredRotation ? 1 : 0;
    StandingMaxAcceleration = MaxAcceleration;
    StandingBrakingDeceleration = BrakingDecelerationWalking;
}

float UWTBRCharacterMovementComponent::GetMaxSpeed() const
{
    const float BaseMax = Super::GetMaxSpeed();
    if (MovementMode != MOVE_Walking && MovementMode != MOVE_NavWalking)
    {
        return BaseMax;
    }

    // Keyed off the predicted capsule state, so the owning client applies the same
    // multiplier on the same frame the server does.
    if (IsProne())
    {
        return BaseMax * ProneSpeedMultiplier;
    }
    if (IsCrouching())
    {
        return BaseMax * CrouchSpeedMultiplier;
    }
    return BaseMax;
}

void UWTBRCharacterMovementComponent::RefreshStanceMovementTuning()
{
    const bool bProne = IsProne();
    const bool bLowStance = bProne || IsCrouching();

    if (bStrafeWhileCrouchedOrProne)
    {
        // bUseControllerDesiredRotation rather than the pawn's bUseControllerRotationYaw:
        // it turns toward the aim at RotationRate instead of snapping, so entering
        // crouch does not whip the character around.
        bOrientRotationToMovement = bLowStance ? false : (bStandingOrientRotationToMovement != 0);
        bUseControllerDesiredRotation = bLowStance ? true : (bStandingUseControllerDesiredRotation != 0);
    }

    // Guard against BeginPlay not having run yet (headless fixtures construct the
    // component and drive the update directly).
    if (StandingMaxAcceleration <= 0.0f)
    {
        StandingMaxAcceleration = MaxAcceleration;
        StandingBrakingDeceleration = BrakingDecelerationWalking;
    }

    if (bProne)
    {
        MaxAcceleration = ProneMaxAcceleration;
        BrakingDecelerationWalking = ProneBrakingDeceleration;
    }
    else if (IsCrouching())
    {
        MaxAcceleration = CrouchMaxAcceleration;
        BrakingDecelerationWalking = CrouchBrakingDeceleration;
    }
    else
    {
        MaxAcceleration = StandingMaxAcceleration;
        BrakingDecelerationWalking = StandingBrakingDeceleration;
    }
}

float UWTBRCharacterMovementComponent::GetEffectiveProneHalfHeight() const
{
    const ACharacter* Owner = CharacterOwner;
    if (!IsValid(Owner)) return ProneCapsuleHalfHeight;

    const UCapsuleComponent* Capsule = Owner->GetCapsuleComponent();
    if (!IsValid(Capsule)) return ProneCapsuleHalfHeight;

    return FMath::Max(ProneCapsuleHalfHeight, Capsule->GetUnscaledCapsuleRadius());
}

bool UWTBRCharacterMovementComponent::IsProne() const
{
    const ACharacter* Owner = CharacterOwner;
    if (!IsValid(Owner)) return false;

    const UCapsuleComponent* Capsule = Owner->GetCapsuleComponent();
    if (!IsValid(Capsule)) return false;

    // Derived from the capsule rather than cached in a member: a correction/replay
    // rewinds the capsule with the rest of the move, and a cached flag would drift
    // out of step with it. Compared against the clamped height so this stays true
    // when the requested prone height is below the capsule radius.
    return FMath::IsNearlyEqual(
        Capsule->GetUnscaledCapsuleHalfHeight(), GetEffectiveProneHalfHeight());
}

bool UWTBRCharacterMovementComponent::CanProneInCurrentState() const
{
    // Prone rides on the crouch state, so it needs everything crouching needs.
    return CanCrouchInCurrentState();
}

void UWTBRCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);

    // Server side of the prediction: reconstruct the client's intent for this move.
    bWantsToProne = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* UWTBRCharacterMovementComponent::GetPredictionData_Client() const
{
    if (ClientPredictionData == nullptr)
    {
        UWTBRCharacterMovementComponent* Mutable = const_cast<UWTBRCharacterMovementComponent*>(this);
        Mutable->ClientPredictionData = new FNetworkPredictionData_Client_WTBR(*this);
    }
    return ClientPredictionData;
}

void UWTBRCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    // Prone implies crouched. Raising bWantsToCrouch here (rather than at the input
    // site) keeps the two in step through replays, where only bWantsToProne is
    // restored from the saved move.
    if (bWantsToProne && CanProneInCurrentState())
    {
        bWantsToCrouch = true;
    }

    // Let the native crouch logic settle the capsule at crouched height first.
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

    const ACharacter* Owner = CharacterOwner;
    if (!IsValid(Owner)) return;

    // Then apply or remove the extra prone shrink on top of it. Both steps run in
    // the same predicted function on client and server, so a replay that re-runs
    // the crouch resize immediately re-runs this one too and the capsule lands on
    // the same height either way — that back-and-forth was the source of the jitter.
    const bool bShouldBeProne = bWantsToProne && Owner->bIsCrouched && CanProneInCurrentState();
    const bool bCurrentlyProne = IsProne();

    if (bShouldBeProne && !bCurrentlyProne)
    {
        SetProneCapsule(true);
    }
    else if (!bShouldBeProne && bCurrentlyProne)
    {
        SetProneCapsule(false);
    }

    // Last, so it sees the capsule state this move settled on.
    RefreshStanceMovementTuning();
}

void UWTBRCharacterMovementComponent::SetProneCapsule(bool bProne)
{
    ACharacter* Owner = CharacterOwner;
    if (!IsValid(Owner)) return;

    UCapsuleComponent* Capsule = Owner->GetCapsuleComponent();
    if (!IsValid(Capsule)) return;

    const float TargetHalfHeight = bProne ? GetEffectiveProneHalfHeight() : GetCrouchedHalfHeight();
    const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
    if (FMath::IsNearlyEqual(CurrentHalfHeight, TargetHalfHeight)) return;

    // Standing back up needs the room; lying down never does.
    if (!bProne)
    {
        const FVector TestLocation = Owner->GetActorLocation()
            + FVector(0.0f, 0.0f, TargetHalfHeight - CurrentHalfHeight);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(WTBRProneExpansion), false, Owner);
        const bool bBlocked = GetWorld() && GetWorld()->OverlapBlockingTestByChannel(
            TestLocation,
            Owner->GetActorQuat(),
            Capsule->GetCollisionObjectType(),
            FCollisionShape::MakeCapsule(Capsule->GetUnscaledCapsuleRadius(), TargetHalfHeight),
            Params);
        if (bBlocked)
        {
            return; // stay prone under the obstruction; retried next update
        }
    }

    Capsule->SetCapsuleHalfHeight(TargetHalfHeight, true);
    // Keep the feet planted. Safe to move the actor directly here because this runs
    // inside the movement update on both machines, so it is part of the predicted
    // move rather than an out-of-band teleport the client has to be corrected for.
    Owner->SetActorLocation(
        Owner->GetActorLocation() + FVector(0.0f, 0.0f, TargetHalfHeight - CurrentHalfHeight),
        false);

    // Mesh and camera offsets are owned by the character, which recomputes both
    // absolutely from the current capsule height. Doing it here additively would be
    // wiped by ACharacter::OnStartCrouch, which assigns the mesh offset outright.
    if (AWTBRCharacter* WTBROwner = Cast<AWTBRCharacter>(Owner))
    {
        WTBROwner->RefreshStanceViewOffsets();
    }
}

// ─── FSavedMove_WTBR ─────────────────────────────────────────────────────────

FSavedMove_WTBR::FSavedMove_WTBR()
    : bSavedWantsToProne(0)
{
}

void FSavedMove_WTBR::Clear()
{
    Super::Clear();
    bSavedWantsToProne = 0;
}

uint8 FSavedMove_WTBR::GetCompressedFlags() const
{
    uint8 Result = Super::GetCompressedFlags();
    if (bSavedWantsToProne)
    {
        Result |= FLAG_Custom_0;
    }
    return Result;
}

bool FSavedMove_WTBR::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character,
    float MaxDelta) const
{
    // Never fold together moves that differ in prone intent — the combined move
    // would replay with the wrong capsule.
    const FSavedMove_WTBR* Other = static_cast<const FSavedMove_WTBR*>(NewMove.Get());
    if (Other && bSavedWantsToProne != Other->bSavedWantsToProne)
    {
        return false;
    }
    return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void FSavedMove_WTBR::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel,
    FNetworkPredictionData_Client_Character& ClientData)
{
    Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

    if (const UWTBRCharacterMovementComponent* MoveComp =
            Cast<UWTBRCharacterMovementComponent>(Character ? Character->GetCharacterMovement() : nullptr))
    {
        bSavedWantsToProne = MoveComp->bWantsToProne ? 1 : 0;
    }
}

void FSavedMove_WTBR::PrepMoveFor(ACharacter* Character)
{
    Super::PrepMoveFor(Character);

    if (UWTBRCharacterMovementComponent* MoveComp =
            Cast<UWTBRCharacterMovementComponent>(Character ? Character->GetCharacterMovement() : nullptr))
    {
        MoveComp->bWantsToProne = bSavedWantsToProne != 0;
    }
}

// ─── FNetworkPredictionData_Client_WTBR ──────────────────────────────────────

FNetworkPredictionData_Client_WTBR::FNetworkPredictionData_Client_WTBR(
    const UCharacterMovementComponent& ClientMovement)
    : Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_WTBR::AllocateNewMove()
{
    return FSavedMovePtr(new FSavedMove_WTBR());
}
