// Copyright Vaelborne: Dominion Project. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WTBRCharacter.h"
#include "Components/WTBRMovementExtComponent.h"

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

    UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement();
    const float StandingSpeed = Char->MovementExtComponent->GetNonSprintingSpeed();
    if (StandingSpeed <= 0.0f) return false;

    Char->TrySetCharacterStance(EWTBRCharacterStance::Crouching);
    TestEqual(TEXT("Crouch walk speed is the crouch multiplier"),
        MoveComp->MaxWalkSpeed, StandingSpeed * 0.40f);
    // Kept in sync deliberately: the movement component reads MaxWalkSpeedCrouched
    // only while bIsCrouched is true, which lags a stance change by one update.
    TestEqual(TEXT("Crouch crouched-speed matches"),
        MoveComp->MaxWalkSpeedCrouched, StandingSpeed * 0.40f);

    Char->TrySetCharacterStance(EWTBRCharacterStance::Prone);
    TestEqual(TEXT("Prone walk speed is the prone multiplier"),
        MoveComp->MaxWalkSpeed, StandingSpeed * 0.20f);
    TestEqual(TEXT("Prone crouched-speed matches"),
        MoveComp->MaxWalkSpeedCrouched, StandingSpeed * 0.20f);

    Char->TrySetCharacterStance(EWTBRCharacterStance::Standing);
    TestEqual(TEXT("Standing restores full speed"),
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

    UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement();
    UWTBRMovementExtComponent* MoveExt = Char->MovementExtComponent;

    Char->TrySetCharacterStance(EWTBRCharacterStance::Prone);

    // A movement-penalty change recomputes speed. This used to write MaxWalkSpeed
    // straight from ComputeFinalSpeed(), dropping the prone multiplier entirely.
    MoveExt->SetStaminaPenalty(0.10f);

    const float ExpectedProneSpeed = MoveExt->GetNonSprintingSpeed() * 0.20f;
    TestEqual(TEXT("Prone multiplier survives a penalty recompute"),
        MoveComp->MaxWalkSpeed, ExpectedProneSpeed);
    TestEqual(TEXT("Prone crouched-speed survives a penalty recompute"),
        MoveComp->MaxWalkSpeedCrouched, ExpectedProneSpeed);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
