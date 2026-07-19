// Copyright Vaelborne: Dominion Project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WTBRCharacterMovementComponent.generated.h"

/**
 * Adds prone as a *predicted* movement state.
 *
 * Prone used to be applied by the server (capsule resize + actor move) and pushed
 * to clients through the replicated stance enum. The client never simulated it, so
 * every correction/replay re-ran the move with a crouch-height capsule while the
 * server had a prone-height one — the two disagreed continuously and the character
 * jittered while crawling.
 *
 * The fix is the standard UE pattern for a custom movement state: a bWantsToProne
 * flag that travels to the server inside the saved move's compressed flags, with
 * the capsule change applied from UpdateCharacterStateBeforeMovement so client and
 * server run the exact same code at the exact same point in the simulation, and a
 * replay reproduces it identically.
 *
 * Prone deliberately rides on top of the native crouch (prone implies crouched)
 * rather than replacing it: that keeps ACharacter's crouch bookkeeping — mesh
 * offset, OnStartCrouch/OnEndCrouch, navigation — working untouched, and means the
 * only thing this component owns is the extra shrink from crouched height down to
 * prone height.
 */
UCLASS()
class WTBR_API UWTBRCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UWTBRCharacterMovementComponent();

    // Requested capsule half-height while prone. A capsule cannot be shorter than it
    // is wide, so UCapsuleComponent silently clamps this up to the capsule radius —
    // use GetEffectiveProneHalfHeight() for the height actually applied. The 36 kept
    // here is the value the pre-prediction stance code used; with the character's
    // 42 radius it has always resolved to 42 in practice.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement (Prone)",
        meta = (ClampMin = "10.0"))
    float ProneCapsuleHalfHeight = 36.0f;

    // ProneCapsuleHalfHeight clamped to what the capsule can actually represent.
    UFUNCTION(BlueprintPure, Category = "Character Movement (Prone)")
    float GetEffectiveProneHalfHeight() const;

    // Set from input (owning client and authority alike) and replicated to the
    // server through GetCompressedFlags — never assign this on a simulated proxy.
    UPROPERTY(BlueprintReadOnly, Category = "Character Movement (Prone)")
    bool bWantsToProne = false;

    // Stance speed multipliers. Applied in GetMaxSpeed() off the *predicted* crouch/
    // prone state rather than the replicated stance enum: the enum reaches the owning
    // client a round trip after it has already predicted the crouch, and during that
    // window the client simulated at standing speed while the server used the crouch
    // one — the client ran ahead and got yanked back a step at a time.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement (Stance)",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CrouchSpeedMultiplier = 0.40f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement (Stance)",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ProneSpeedMultiplier = 0.20f;

    // While crouched or prone, face where the camera faces and strafe (PUBG-style)
    // instead of turning to face the movement direction.
    //
    // OFF until the crouch/prone locomotion animations exist. Strafing needs an
    // 8-direction BlendSpace: with only a forward loop authored, a strafing
    // character slides sideways while playing a forward walk, which looks worse than
    // turning to face the movement direction (where the forward loop always roughly
    // matches). Turn this on in the same pass that hooks up BS_Crouch / BS_Prone —
    // that is when it starts being an improvement rather than a regression.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement (Stance)")
    bool bStrafeWhileCrouchedOrProne = false;

    // Standing acceleration (UE's 2048 default) reaches the much lower crouch/prone
    // top speed in about a tenth of a second, so a crouched character snaps to full
    // speed the instant a key goes down — it reads as lunging rather than settling
    // into a crouch-walk. These give the stance a visible ramp instead.
    // ⚠ PLAYTEST PENDING — feel values, tune against the real crouch animation.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement (Stance)",
        meta = (ClampMin = "1.0"))
    float CrouchMaxAcceleration = 700.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement (Stance)",
        meta = (ClampMin = "1.0"))
    float ProneMaxAcceleration = 250.0f;

    // Matching braking, so stopping is as deliberate as starting.
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement (Stance)",
        meta = (ClampMin = "1.0"))
    float CrouchBrakingDeceleration = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement (Stance)",
        meta = (ClampMin = "1.0"))
    float ProneBrakingDeceleration = 400.0f;

    UFUNCTION(BlueprintPure, Category = "Character Movement (Prone)")
    bool IsProne() const;

    // Prone needs the same grounded state crouching does.
    UFUNCTION(BlueprintPure, Category = "Character Movement (Prone)")
    bool CanProneInCurrentState() const;

    virtual void BeginPlay() override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
    virtual float GetMaxSpeed() const override;

private:
    // Resize between the crouched and prone capsule, keeping the feet planted.
    // Called only from UpdateCharacterStateBeforeMovement so it is part of the move.
    void SetProneCapsule(bool bProne);

    // Rotation mode plus acceleration/braking for the current stance. Driven from
    // the predicted capsule state so client and server always simulate identically —
    // all three of these feed CalcVelocity, so a mismatch would cause corrections.
    void RefreshStanceMovementTuning();

    // Standing values, captured once so restoring them does not hardcode whatever
    // the character class happened to configure.
    uint8 bStandingOrientRotationToMovement : 1;
    uint8 bStandingUseControllerDesiredRotation : 1;
    float StandingMaxAcceleration = 0.0f;
    float StandingBrakingDeceleration = 0.0f;
};

/** Carries bWantsToProne through the client's saved-move history. */
class FSavedMove_WTBR : public FSavedMove_Character
{
public:
    typedef FSavedMove_Character Super;

    uint32 bSavedWantsToProne : 1;

    FSavedMove_WTBR();

    virtual void Clear() override;
    virtual uint8 GetCompressedFlags() const override;
    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character,
        float MaxDelta) const override;
    virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel,
        class FNetworkPredictionData_Client_Character& ClientData) override;
    virtual void PrepMoveFor(ACharacter* Character) override;
};

class FNetworkPredictionData_Client_WTBR : public FNetworkPredictionData_Client_Character
{
public:
    typedef FNetworkPredictionData_Client_Character Super;

    explicit FNetworkPredictionData_Client_WTBR(const UCharacterMovementComponent& ClientMovement);

    virtual FSavedMovePtr AllocateNewMove() override;
};
