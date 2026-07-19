// Copyright Vaelborne: Dominion Project. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WTBRCharacter.h"
#include "Components/WTBRMovementExtComponent.h"
#include "Components/WTBRCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"

// ═════════════════════════════════════════════════════════════════════════════
// Character stance (Standing / Crouching / Prone)
//
// Owner-found PIE bugs 2026-07-19, all from one root cause: ACharacter::Crouch /
// UnCrouch only raise bWantsToCrouch, and bIsCrouched does not flip until the next
// character-movement update. TrySetCharacterStance tested bIsCrouched immediately
// after calling them, so the first press always bailed out *before* committing
// CharacterStance and before RefreshStanceSpeeds() — which is why every stance
// change needed the key pressed twice, and why a visually crouched/prone character
// still moved at standing speed.
//
// The second half of the speed bug lived in UWTBRMovementExtComponent, which wrote
// MaxWalkSpeed directly on every limb/stamina/debuff change and never touched
// MaxWalkSpeedCrouched, clobbering the stance multiplier.
// ═════════════════════════════════════════════════════════════════════════════

namespace
{
    class FWTBRStanceWorldFixture
    {
    public:
        explicit FWTBRStanceWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRStanceWorldFixture()
        {
            if (World)
            {
                World->DestroyWorld(false);
                if (GEngine)
                {
                    GEngine->DestroyWorldContext(World);
                }
                World = nullptr;
            }
        }

        UWorld* GetWorld() const { return World; }

    private:
        UWorld* World = nullptr;
    };

    AWTBRCharacter* StanceTest_SpawnCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AWTBRCharacter* Character = World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

        // In this fixture the movement component never reaches a real movement mode
        // (it stays MOVE_None), and UCharacterMovementComponent::CanCrouchInCurrentState
        // requires walking or falling — without this every stance change is refused
        // for reasons that have nothing to do with what these tests cover.
        if (Character)
        {
            if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
            {
                MoveComp->SetMovementMode(MOVE_Walking);
            }
        }
        return Character;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRStanceSinglePressCommitsTest,
    "WTBR.Character.Stance.SinglePressCommitsStance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRStanceSinglePressCommitsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRStanceWorldFixture Fixture(TEXT("WTBR_Stance_SinglePress"));
    AWTBRCharacter* Char = StanceTest_SpawnCharacter(Fixture.GetWorld());
    if (!Char) return false;

    TestEqual(TEXT("Starts standing"),
        static_cast<int32>(Char->GetCharacterStance()),
        static_cast<int32>(EWTBRCharacterStance::Standing));

    // Each of these used to return false on the first call.
    TestTrue(TEXT("Crouch commits on the first request"),
        Char->TrySetCharacterStance(EWTBRCharacterStance::Crouching));
    TestEqual(TEXT("Stance is Crouching"),
        static_cast<int32>(Char->GetCharacterStance()),
        static_cast<int32>(EWTBRCharacterStance::Crouching));

    TestTrue(TEXT("Prone commits on the first request"),
        Char->TrySetCharacterStance(EWTBRCharacterStance::Prone));
    TestEqual(TEXT("Stance is Prone"),
        static_cast<int32>(Char->GetCharacterStance()),
        static_cast<int32>(EWTBRCharacterStance::Prone));

    TestTrue(TEXT("Standing commits on the first request"),
        Char->TrySetCharacterStance(EWTBRCharacterStance::Standing));
    TestEqual(TEXT("Stance is Standing"),
        static_cast<int32>(Char->GetCharacterStance()),
        static_cast<int32>(EWTBRCharacterStance::Standing));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRStanceAppliesWalkSpeedTest,
    "WTBR.Character.Stance.StanceAppliesWalkSpeed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRStanceAppliesWalkSpeedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRStanceWorldFixture Fixture(TEXT("WTBR_Stance_WalkSpeed"));
    AWTBRCharacter* Char = StanceTest_SpawnCharacter(Fixture.GetWorld());
    if (!Char || !Char->GetCharacterMovement() || !Char->MovementExtComponent) return false;

    UWTBRCharacterMovementComponent* MoveComp =
        Cast<UWTBRCharacterMovementComponent>(Char->GetCharacterMovement());
    if (!MoveComp) return false;

    const float StandingSpeed = Char->MovementExtComponent->GetNonSprintingSpeed();
    if (StandingSpeed <= 0.0f) return false;

    // GetMaxSpeed() is what the movement simulation actually reads, and it derives
    // the stance multiplier from the predicted capsule state — so it needs the
    // movement update to have applied the stance first.
    Char->TrySetCharacterStance(EWTBRCharacterStance::Crouching);
    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);
    TestEqual(TEXT("Crouch max speed is the crouch multiplier"),
        MoveComp->GetMaxSpeed(), StandingSpeed * MoveComp->CrouchSpeedMultiplier);

    Char->TrySetCharacterStance(EWTBRCharacterStance::Prone);
    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);
    TestEqual(TEXT("Prone max speed is the prone multiplier"),
        MoveComp->GetMaxSpeed(), StandingSpeed * MoveComp->ProneSpeedMultiplier);

    Char->TrySetCharacterStance(EWTBRCharacterStance::Standing);
    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);
    TestEqual(TEXT("Standing restores full speed"),
        MoveComp->GetMaxSpeed(), StandingSpeed);

    // The base fields stay unmultiplied — the multiplier belongs to GetMaxSpeed only,
    // so applying it twice can never creep back in.
    TestEqual(TEXT("Base walk speed is left unmultiplied"),
        MoveComp->MaxWalkSpeed, StandingSpeed);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRStanceSurvivesSpeedRecomputeTest,
    "WTBR.Character.Stance.StanceSurvivesSpeedRecompute",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRStanceSurvivesSpeedRecomputeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRStanceWorldFixture Fixture(TEXT("WTBR_Stance_SpeedRecompute"));
    AWTBRCharacter* Char = StanceTest_SpawnCharacter(Fixture.GetWorld());
    if (!Char || !Char->GetCharacterMovement() || !Char->MovementExtComponent) return false;

    UWTBRCharacterMovementComponent* MoveComp =
        Cast<UWTBRCharacterMovementComponent>(Char->GetCharacterMovement());
    UWTBRMovementExtComponent* MoveExt = Char->MovementExtComponent;
    if (!MoveComp) return false;

    Char->TrySetCharacterStance(EWTBRCharacterStance::Prone);
    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);

    // A movement-penalty change recomputes speed. This used to write MaxWalkSpeed
    // straight from ComputeFinalSpeed(), dropping the prone multiplier entirely.
    MoveExt->SetStaminaPenalty(0.10f);

    TestEqual(TEXT("Prone multiplier survives a penalty recompute"),
        MoveComp->GetMaxSpeed(),
        MoveExt->GetNonSprintingSpeed() * MoveComp->ProneSpeedMultiplier);

    return true;
}

// Prone is a predicted movement state (UWTBRCharacterMovementComponent): the stance
// request only raises bWantsToProne, and the capsule change happens inside the
// movement update so client and server produce it from the same simulated move.
// These two tests cover the halves of that contract that are reachable headlessly.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRProneCapsuleAppliedByMovementUpdateTest,
    "WTBR.Character.Stance.ProneCapsuleAppliedByMovementUpdate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRProneCapsuleAppliedByMovementUpdateTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRStanceWorldFixture Fixture(TEXT("WTBR_Stance_ProneCapsule"));
    AWTBRCharacter* Char = StanceTest_SpawnCharacter(Fixture.GetWorld());
    if (!Char) return false;

    UWTBRCharacterMovementComponent* MoveComp =
        Cast<UWTBRCharacterMovementComponent>(Char->GetCharacterMovement());
    UCapsuleComponent* Capsule = Char->GetCapsuleComponent();
    if (!MoveComp || !Capsule) return false;

    TestTrue(TEXT("Character uses the WTBR movement component"), MoveComp != nullptr);

    // A capsule cannot be shorter than it is wide, so the requested 36 resolves to
    // the 42 radius. Asserting this keeps the clamp visible instead of letting a
    // future radius change silently move the prone height.
    TestEqual(TEXT("Prone height is clamped up to the capsule radius"),
        MoveComp->GetEffectiveProneHalfHeight(), Capsule->GetUnscaledCapsuleRadius());
    TestTrue(TEXT("Prone is still shorter than crouched"),
        MoveComp->GetEffectiveProneHalfHeight() < MoveComp->GetCrouchedHalfHeight());

    Char->TrySetCharacterStance(EWTBRCharacterStance::Prone);
    TestTrue(TEXT("Prone request raises the predicted flag"), MoveComp->bWantsToProne);

    // The capsule is deliberately untouched until the movement update runs.
    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);
    TestEqual(TEXT("Movement update shrinks the capsule to prone height"),
        Capsule->GetUnscaledCapsuleHalfHeight(), MoveComp->GetEffectiveProneHalfHeight());
    TestTrue(TEXT("IsProne reads the prone capsule"), MoveComp->IsProne());

    Char->TrySetCharacterStance(EWTBRCharacterStance::Crouching);
    TestFalse(TEXT("Leaving prone lowers the predicted flag"), MoveComp->bWantsToProne);

    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);
    TestEqual(TEXT("Movement update grows the capsule back to crouched height"),
        Capsule->GetUnscaledCapsuleHalfHeight(), MoveComp->GetCrouchedHalfHeight());
    TestFalse(TEXT("IsProne is false once back at crouch height"), MoveComp->IsProne());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRProneSurvivesCompressedFlagRoundTripTest,
    "WTBR.Character.Stance.ProneSurvivesCompressedFlagRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRProneSurvivesCompressedFlagRoundTripTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRStanceWorldFixture Fixture(TEXT("WTBR_Stance_ProneFlagRoundTrip"));
    AWTBRCharacter* Char = StanceTest_SpawnCharacter(Fixture.GetWorld());
    if (!Char) return false;

    UWTBRCharacterMovementComponent* MoveComp =
        Cast<UWTBRCharacterMovementComponent>(Char->GetCharacterMovement());
    if (!MoveComp) return false;

    // This is the whole reason prone stopped jittering: the client's intent has to
    // reach the server inside the move, not out of band. If the flag ever stops
    // making the round trip, the two sides silently diverge again.
    FSavedMove_WTBR Move;
    Move.Clear();
    Move.bSavedWantsToProne = 1;
    const uint8 ProneFlags = Move.GetCompressedFlags();
    TestTrue(TEXT("Prone intent is packed into a custom compressed flag"),
        (ProneFlags & FSavedMove_Character::FLAG_Custom_0) != 0);

    MoveComp->bWantsToProne = false;
    MoveComp->UpdateFromCompressedFlags(ProneFlags);
    TestTrue(TEXT("Server restores prone intent from the flags"), MoveComp->bWantsToProne);

    Move.Clear();
    TestEqual(TEXT("Clear resets the saved prone intent"),
        static_cast<int32>(Move.bSavedWantsToProne), 0);

    const uint8 StandingFlags = Move.GetCompressedFlags();
    MoveComp->UpdateFromCompressedFlags(StandingFlags);
    TestFalse(TEXT("Server clears prone intent when the flag is absent"),
        MoveComp->bWantsToProne);

    return true;
}

// Owner-found PIE issue: crouch-walking and crawling felt like the whole view was
// being flung around, with the character right up against the camera. The camera
// boom is attached to the capsule, so it rode the shrinking capsule centre down
// toward the floor until the spring arm's collision probe hit the ground and
// dragged the camera in. Nothing compensated for it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRStanceCameraHoldsHeightTest,
    "WTBR.Character.Stance.CameraHoldsHeightAcrossStances",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRStanceCameraHoldsHeightTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRStanceWorldFixture Fixture(TEXT("WTBR_Stance_CameraHeight"));
    AWTBRCharacter* Char = StanceTest_SpawnCharacter(Fixture.GetWorld());
    if (!Char) return false;

    UWTBRCharacterMovementComponent* MoveComp =
        Cast<UWTBRCharacterMovementComponent>(Char->GetCharacterMovement());
    USpringArmComponent* Boom = Char->FindComponentByClass<USpringArmComponent>();
    UCapsuleComponent* Capsule = Char->GetCapsuleComponent();
    if (!MoveComp || !Boom || !Capsule) return false;

    // Full compensation is the default, so the boom's world height must not change.
    TestEqual(TEXT("Default is full camera height compensation"),
        Char->StanceCameraHeightCompensation, 1.0f);

    // FVector components are double in UE5 — keep every comparison double so the
    // TestEqual overload is unambiguous. The tolerance covers rounding accumulated
    // across several capsule resizes; it is a hundredth of a millimetre, far below
    // anything that could be seen or felt.
    constexpr double CameraHeightTolerance = 0.01;
    const double StandingBoomWorldZ =
        Char->GetActorLocation().Z + Boom->GetRelativeLocation().Z;

    Char->TrySetCharacterStance(EWTBRCharacterStance::Crouching);
    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);
    TestEqual(TEXT("Camera keeps its world height while crouched"),
        static_cast<double>(Char->GetActorLocation().Z + Boom->GetRelativeLocation().Z),
        StandingBoomWorldZ, CameraHeightTolerance);

    Char->TrySetCharacterStance(EWTBRCharacterStance::Prone);
    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);
    TestEqual(TEXT("Camera keeps its world height while prone"),
        static_cast<double>(Char->GetActorLocation().Z + Boom->GetRelativeLocation().Z),
        StandingBoomWorldZ, CameraHeightTolerance);

    // The mesh must still ride the capsule down, or the character floats.
    const float ProneShrink = Char->GetDefaultHalfHeight() - Capsule->GetUnscaledCapsuleHalfHeight();
    TestTrue(TEXT("Prone actually shrank the capsule"), ProneShrink > 0.0f);

    Char->TrySetCharacterStance(EWTBRCharacterStance::Standing);
    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);
    TestEqual(TEXT("Camera height is unchanged back at standing"),
        static_cast<double>(Char->GetActorLocation().Z + Boom->GetRelativeLocation().Z),
        StandingBoomWorldZ, CameraHeightTolerance);
    TestEqual(TEXT("Boom offset returns to zero when standing"),
        static_cast<double>(Boom->GetRelativeLocation().Z), 0.0, CameraHeightTolerance);

    return true;
}

// Owner-found PIE issue: crouch movement felt like the character lunged in the input
// direction rather than settling into a crouch-walk. Standing acceleration (2048)
// reaches the crouch top speed in about a tenth of a second, so the stance had no
// ramp at all.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRStanceAccelerationTest,
    "WTBR.Character.Stance.StanceTunesAcceleration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRStanceAccelerationTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRStanceWorldFixture Fixture(TEXT("WTBR_Stance_Acceleration"));
    AWTBRCharacter* Char = StanceTest_SpawnCharacter(Fixture.GetWorld());
    if (!Char) return false;

    UWTBRCharacterMovementComponent* MoveComp =
        Cast<UWTBRCharacterMovementComponent>(Char->GetCharacterMovement());
    if (!MoveComp) return false;

    const float StandingAcceleration = MoveComp->MaxAcceleration;

    Char->TrySetCharacterStance(EWTBRCharacterStance::Crouching);
    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);
    TestEqual(TEXT("Crouch uses the crouch acceleration"),
        MoveComp->MaxAcceleration, MoveComp->CrouchMaxAcceleration);
    TestTrue(TEXT("Crouch accelerates more slowly than standing"),
        MoveComp->MaxAcceleration < StandingAcceleration);

    Char->TrySetCharacterStance(EWTBRCharacterStance::Prone);
    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);
    TestEqual(TEXT("Prone uses the prone acceleration"),
        MoveComp->MaxAcceleration, MoveComp->ProneMaxAcceleration);
    TestTrue(TEXT("Prone accelerates more slowly than crouch"),
        MoveComp->ProneMaxAcceleration < MoveComp->CrouchMaxAcceleration);

    Char->TrySetCharacterStance(EWTBRCharacterStance::Standing);
    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);
    TestEqual(TEXT("Standing restores the original acceleration"),
        MoveComp->MaxAcceleration, StandingAcceleration);

    // Strafing must be off when standing, or normal movement stops facing where it
    // is heading.
    TestTrue(TEXT("Standing orients rotation to movement"),
        MoveComp->bOrientRotationToMovement);

    // Strafing stays disabled until the crouch/prone BlendSpaces exist — see the
    // property comment. This asserts the default so it cannot be switched on by
    // accident before the animations that make it look right are in place.
    TestFalse(TEXT("Strafe-while-crouched is off by default"),
        MoveComp->bStrafeWhileCrouchedOrProne);

    Char->TrySetCharacterStance(EWTBRCharacterStance::Crouching);
    MoveComp->UpdateCharacterStateBeforeMovement(0.016f);
    TestTrue(TEXT("Crouch still faces its movement direction while strafe is off"),
        MoveComp->bOrientRotationToMovement);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
