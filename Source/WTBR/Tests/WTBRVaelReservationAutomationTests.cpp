// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/WTBRVaelComponent.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRVaelReservationWorldFixture
    {
    public:
        explicit FWTBRVaelReservationWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRVaelReservationWorldFixture()
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

    AWTBRCharacter* SpawnReservationCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    UWTBRVaelComponent* MakeReservationVael(FWTBRVaelReservationWorldFixture& Fixture, FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnReservationCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent) return nullptr;

        // BeginPlay does not run in this fixture, so seed the component directly.
        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->VaelComponent;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVaelReserveDoesNotChangeCurrentVaelButReducesAvailableTest,
    "WTBR.Vael.Reservation.ReserveDoesNotChangeCurrentVaelButReducesAvailable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVaelReserveDoesNotChangeCurrentVaelButReducesAvailableTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVaelReservationWorldFixture Fixture(TEXT("WTBR_VaelReserve_NoDeduct"));
    UWTBRVaelComponent* Vael = MakeReservationVael(Fixture, *this);
    if (!Vael) return false;

    FGuid Handle;
    TestTrue(TEXT("Reservation succeeds"), Vael->TryReserveVael(30.0f, Handle));
    TestTrue(TEXT("Reservation handle is valid"), Handle.IsValid());
    TestEqual(TEXT("Current Vael is unchanged"), Vael->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Available Vael excludes reservation"), Vael->GetAvailableVael(), 70.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVaelReserveFailsWhenAmountExceedsAvailableTest,
    "WTBR.Vael.Reservation.ReserveFailsWhenAmountExceedsAvailable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVaelReserveFailsWhenAmountExceedsAvailableTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVaelReservationWorldFixture Fixture(TEXT("WTBR_VaelReserve_Insufficient"));
    UWTBRVaelComponent* Vael = MakeReservationVael(Fixture, *this);
    if (!Vael) return false;

    FGuid FirstHandle;
    FGuid FailedHandle;
    FGuid RemainingHandle;
    TestTrue(TEXT("Initial reservation succeeds"), Vael->TryReserveVael(60.0f, FirstHandle));
    TestFalse(TEXT("Reservation above available balance fails"), Vael->TryReserveVael(41.0f, FailedHandle));
    TestFalse(TEXT("Failed reservation does not assign a handle"), FailedHandle.IsValid());
    TestTrue(TEXT("Full remaining balance still reserves after failure"), Vael->TryReserveVael(40.0f, RemainingHandle));
    TestEqual(TEXT("All Vael is reserved without leaked partial state"), Vael->GetAvailableVael(), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVaelReserveNeverSetsOverheatedTest,
    "WTBR.Vael.Reservation.ReserveNeverSetsOverheatedEvenWhenReservingEntireBalance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVaelReserveNeverSetsOverheatedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVaelReservationWorldFixture Fixture(TEXT("WTBR_VaelReserve_NoOverheat"));
    UWTBRVaelComponent* Vael = MakeReservationVael(Fixture, *this);
    if (!Vael) return false;

    FGuid Handle;
    TestTrue(TEXT("Entire balance reserves"), Vael->TryReserveVael(100.0f, Handle));
    TestEqual(TEXT("Current Vael remains intact"), Vael->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Available Vael reaches zero"), Vael->GetAvailableVael(), 0.0f);
    TestFalse(TEXT("Reservation does not overheat"), Vael->IsOverheated());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVaelCommitDeductsCurrentVaelAndRestoresAvailableTest,
    "WTBR.Vael.Reservation.CommitDeductsCurrentVaelAndRestoresAvailable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVaelCommitDeductsCurrentVaelAndRestoresAvailableTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVaelReservationWorldFixture Fixture(TEXT("WTBR_VaelCommit_Deduct"));
    UWTBRVaelComponent* Vael = MakeReservationVael(Fixture, *this);
    if (!Vael) return false;

    FGuid Handle;
    TestTrue(TEXT("Reservation succeeds"), Vael->TryReserveVael(30.0f, Handle));
    TestTrue(TEXT("Commit succeeds"), Vael->CommitReservation(Handle));
    TestEqual(TEXT("Commit deducts the reserved amount"), Vael->GetCurrentVael(), 70.0f);
    TestEqual(TEXT("Available Vael matches current after commit"), Vael->GetAvailableVael(), 70.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVaelCommitCanTriggerOverheatTest,
    "WTBR.Vael.Reservation.CommitCanTriggerOverheatWhenResultingVaelIsZero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVaelCommitCanTriggerOverheatTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVaelReservationWorldFixture Fixture(TEXT("WTBR_VaelCommit_Overheat"));
    UWTBRVaelComponent* Vael = MakeReservationVael(Fixture, *this);
    if (!Vael) return false;

    FGuid Handle;
    TestTrue(TEXT("Entire balance reserves"), Vael->TryReserveVael(100.0f, Handle));
    TestTrue(TEXT("Commit succeeds"), Vael->CommitReservation(Handle));
    TestEqual(TEXT("Commit reaches zero Vael"), Vael->GetCurrentVael(), 0.0f);
    TestTrue(TEXT("Commit at zero overheats"), Vael->IsOverheated());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVaelReleaseRestoresAvailableWithZeroVaelDeductedTest,
    "WTBR.Vael.Reservation.ReleaseRestoresAvailableWithZeroVaelDeducted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVaelReleaseRestoresAvailableWithZeroVaelDeductedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVaelReservationWorldFixture Fixture(TEXT("WTBR_VaelRelease_Restore"));
    UWTBRVaelComponent* Vael = MakeReservationVael(Fixture, *this);
    if (!Vael) return false;

    FGuid Handle;
    TestTrue(TEXT("Reservation succeeds"), Vael->TryReserveVael(30.0f, Handle));
    TestTrue(TEXT("Release succeeds"), Vael->ReleaseReservation(Handle));
    TestEqual(TEXT("Release never deducts Current Vael"), Vael->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Release restores the available balance"), Vael->GetAvailableVael(), 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVaelDoubleCommitOrDoubleReleaseIsIdempotentTest,
    "WTBR.Vael.Reservation.DoubleCommitOrDoubleReleaseIsIdempotent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVaelDoubleCommitOrDoubleReleaseIsIdempotentTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVaelReservationWorldFixture Fixture(TEXT("WTBR_VaelReservation_Idempotent"));
    UWTBRVaelComponent* Vael = MakeReservationVael(Fixture, *this);
    if (!Vael) return false;

    FGuid CommitHandle;
    TestTrue(TEXT("Reservation to commit succeeds"), Vael->TryReserveVael(30.0f, CommitHandle));
    TestTrue(TEXT("First commit succeeds"), Vael->CommitReservation(CommitHandle));
    TestFalse(TEXT("Second commit is idempotently rejected"), Vael->CommitReservation(CommitHandle));
    TestEqual(TEXT("Second commit does not deduct again"), Vael->GetCurrentVael(), 70.0f);

    FGuid ReleaseHandle;
    TestTrue(TEXT("Reservation to release succeeds"), Vael->TryReserveVael(20.0f, ReleaseHandle));
    TestTrue(TEXT("First release succeeds"), Vael->ReleaseReservation(ReleaseHandle));
    TestFalse(TEXT("Second release is idempotently rejected"), Vael->ReleaseReservation(ReleaseHandle));
    TestEqual(TEXT("Second release does not change balance"), Vael->GetCurrentVael(), 70.0f);
    TestEqual(TEXT("No reservation remains after idempotent operations"), Vael->GetAvailableVael(), 70.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVaelConcurrentReservationsTrackIndependentlyTest,
    "WTBR.Vael.Reservation.ConcurrentReservationsTrackIndependently",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVaelConcurrentReservationsTrackIndependentlyTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVaelReservationWorldFixture Fixture(TEXT("WTBR_VaelReservation_Concurrent"));
    UWTBRVaelComponent* Vael = MakeReservationVael(Fixture, *this);
    if (!Vael) return false;

    FGuid FirstHandle;
    FGuid SecondHandle;
    TestTrue(TEXT("First reservation succeeds"), Vael->TryReserveVael(30.0f, FirstHandle));
    TestTrue(TEXT("Second reservation succeeds"), Vael->TryReserveVael(40.0f, SecondHandle));
    TestEqual(TEXT("Both reservations reduce availability"), Vael->GetAvailableVael(), 30.0f);
    TestTrue(TEXT("Releasing first reservation succeeds"), Vael->ReleaseReservation(FirstHandle));
    TestEqual(TEXT("Second reservation remains active"), Vael->GetAvailableVael(), 60.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVaelTryConsumeBlockedByActiveReservationTest,
    "WTBR.Vael.Reservation.TryConsumeVaelIsBlockedByAnActiveReservationEvenWithSufficientCurrentVael",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVaelTryConsumeBlockedByActiveReservationTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVaelReservationWorldFixture Fixture(TEXT("WTBR_VaelReservation_ConsumeExcluded"));
    UWTBRVaelComponent* Vael = MakeReservationVael(Fixture, *this);
    if (!Vael) return false;

    FGuid Handle;
    TestTrue(TEXT("Most of the balance reserves"), Vael->TryReserveVael(80.0f, Handle));
    TestFalse(TEXT("Consume is blocked by the active reservation"), Vael->TryConsumeVael(30.0f));
    TestEqual(TEXT("Blocked consume leaves Current Vael unchanged"), Vael->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Reservation remains excluded from availability"), Vael->GetAvailableVael(), 20.0f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
