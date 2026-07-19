// Copyright Vaelborne: Dominion Project. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Actors/WTBRNexilWireActor.h"
#include "Actors/WTBRProjectileBase.h"
#include "WTBRCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
    class FWTBRNexilWireHPWorldFixture
    {
    public:
        explicit FWTBRNexilWireHPWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRNexilWireHPWorldFixture()
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

    AWTBRCharacter* SpawnNexilWireHPCharacter(UWorld* World, int32 TeamId)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AWTBRCharacter* Character = World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
        if (Character)
        {
            Character->SetTeamId(TeamId);
        }
        return Character;
    }

    AWTBRNexilWireActor* SpawnNexilWireHPWire(
        UWorld* World, AWTBRCharacter* OwnerChar, int32 WireHP)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.Owner      = OwnerChar;
        Params.Instigator = OwnerChar;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AWTBRNexilWireActor* Wire = World->SpawnActor<AWTBRNexilWireActor>(
            AWTBRNexilWireActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
        if (Wire)
        {
            // Lifetime=0 (no auto-despawn timer during the test), Stagger=1.5s,
            // Length=500, no owning UWTBRNexilTrigger needed for isolated wire tests.
            Wire->InitializeWire(0.0f, 1.5f, 500.0f, WireHP, nullptr);
        }
        return Wire;
    }

    AWTBRProjectileBase* SpawnNexilWireHPProjectile(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRProjectileBase>(
            AWTBRProjectileBase::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRNexilWireHPDestroyedAtZeroTest,
    "WTBR.Nexil.WireHP.HitDestroysDefaultOneHPWire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRNexilWireHPDestroyedAtZeroTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRNexilWireHPWorldFixture Fixture(TEXT("WTBR_NexilWireHP_DestroyedAtZero"));
    AWTBRCharacter* Owner = SpawnNexilWireHPCharacter(Fixture.GetWorld(), 0);
    AWTBRNexilWireActor* Wire = SpawnNexilWireHPWire(Fixture.GetWorld(), Owner, /*WireHP=*/1);
    AWTBRProjectileBase* Projectile = SpawnNexilWireHPProjectile(Fixture.GetWorld());
    if (!Wire || !Projectile) return false;

    TestEqual(TEXT("Default WireHP is 1"), Wire->WireHP, 1);

    Wire->OnWireOverlapBeginForTest(Projectile);
    TestFalse(TEXT("Single hit destroys a 1-HP wire"), IsValid(Wire));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRNexilWireHPPartialDamageTest,
    "WTBR.Nexil.WireHP.PartialDamageStaysFunctional",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRNexilWireHPPartialDamageTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRNexilWireHPWorldFixture Fixture(TEXT("WTBR_NexilWireHP_PartialDamage"));
    AWTBRCharacter* Owner = SpawnNexilWireHPCharacter(Fixture.GetWorld(), 0);
    AWTBRNexilWireActor* Wire = SpawnNexilWireHPWire(Fixture.GetWorld(), Owner, /*WireHP=*/2);
    AWTBRProjectileBase* Projectile = SpawnNexilWireHPProjectile(Fixture.GetWorld());
    if (!Wire || !Projectile) return false;

    Wire->OnWireOverlapBeginForTest(Projectile);
    TestTrue(TEXT("2-HP wire survives one hit"), IsValid(Wire));
    if (!IsValid(Wire)) return false;
    TestEqual(TEXT("WireHP drops by exactly 1 per hit"), Wire->WireHP, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRNexilWireHPTeammateIgnoredTest,
    "WTBR.Nexil.WireHP.TeammateDoesNotTripWire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRNexilWireHPTeammateIgnoredTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRNexilWireHPWorldFixture Fixture(TEXT("WTBR_NexilWireHP_TeammateIgnored"));
    AWTBRCharacter* Owner    = SpawnNexilWireHPCharacter(Fixture.GetWorld(), 0);
    AWTBRCharacter* Teammate = SpawnNexilWireHPCharacter(Fixture.GetWorld(), 0);
    AWTBRNexilWireActor* Wire = SpawnNexilWireHPWire(Fixture.GetWorld(), Owner, /*WireHP=*/1);
    if (!Teammate || !Wire) return false;

    Wire->OnWireOverlapBeginForTest(Teammate);
    TestTrue(TEXT("Wire survives a same-team walkthrough"), IsValid(Wire));
    if (!IsValid(Wire)) return false;
    TestFalse(TEXT("Same-team walkthrough does not trip the wire"), Wire->bIsTriggered);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRNexilWireHPEnemyStillTripsTest,
    "WTBR.Nexil.WireHP.EnemyStillTripsWireRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRNexilWireHPEnemyStillTripsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRNexilWireHPWorldFixture Fixture(TEXT("WTBR_NexilWireHP_EnemyTrips"));
    AWTBRCharacter* Owner = SpawnNexilWireHPCharacter(Fixture.GetWorld(), 0);
    AWTBRCharacter* Enemy = SpawnNexilWireHPCharacter(Fixture.GetWorld(), 1);
    AWTBRNexilWireActor* Wire = SpawnNexilWireHPWire(Fixture.GetWorld(), Owner, /*WireHP=*/1);
    if (!Enemy || !Wire) return false;

    Wire->OnWireOverlapBeginForTest(Enemy);
    TestFalse(TEXT("Enemy walkthrough still trips and destroys the wire"), IsValid(Wire));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRNexilWireHPOwnerIgnoredTest,
    "WTBR.Nexil.WireHP.OwnerStillIgnoredRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRNexilWireHPOwnerIgnoredTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRNexilWireHPWorldFixture Fixture(TEXT("WTBR_NexilWireHP_OwnerIgnored"));
    AWTBRCharacter* Owner = SpawnNexilWireHPCharacter(Fixture.GetWorld(), 0);
    AWTBRNexilWireActor* Wire = SpawnNexilWireHPWire(Fixture.GetWorld(), Owner, /*WireHP=*/1);
    if (!Owner || !Wire) return false;

    Wire->OnWireOverlapBeginForTest(Owner);
    TestTrue(TEXT("Owner walking through their own wire is still ignored"), IsValid(Wire));
    if (!IsValid(Wire)) return false;
    TestFalse(TEXT("Owner walkthrough does not trip the wire"), Wire->bIsTriggered);
    return true;
}

// Owner-found PIE bug 2026-07-19: a wire that expired (WireDuration) or was cut
// while someone was ziplining on it left the rider stuck in MOVE_Flying — floating
// in mid-air and still able to launch off a wire that no longer existed. The grab
// now subscribes to the wire's OnDestroyed and drops the rider when it dies.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRNexilZiplineWireDestroyedReleasesRiderTest,
    "WTBR.Nexil.Zipline.WireDestroyedReleasesRider",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRNexilZiplineWireDestroyedReleasesRiderTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRNexilWireHPWorldFixture Fixture(TEXT("WTBR_NexilZipline_WireDestroyed"));
    UWorld* World = Fixture.GetWorld();
    if (!World) return false;

    // Unlike the other tests in this file, this one needs a world that has actually
    // begun play: AActor::Destroy only routes Destroyed()/OnDestroyed for actors
    // that have begun play, and OnDestroyed is exactly the hook under test.
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();

    // The wire's own caster may grab it, so one character is enough here.
    AWTBRCharacter* Rider = SpawnNexilWireHPCharacter(World, 0);
    AWTBRNexilWireActor* Wire = SpawnNexilWireHPWire(World, Rider, /*WireHP=*/1);
    if (!Rider || !Wire) return false;

    // Call the RPC body directly — the fixture world has no net driver, so going
    // through the Server_ RPC wrapper does not dispatch here.
    Rider->Server_GrabNexilWire_Implementation(Wire);
    TestTrue(TEXT("Rider is hanging after grabbing the wire"), Rider->IsHangingOnNexilWire());

    Wire->Destroy();

    TestFalse(TEXT("Rider stops hanging once the wire is destroyed"),
        Rider->IsHangingOnNexilWire());
    if (UCharacterMovementComponent* MoveComp = Rider->GetCharacterMovement())
    {
        TestEqual(TEXT("Rider falls instead of staying stuck in flying mode"),
            static_cast<int32>(MoveComp->MovementMode.GetValue()),
            static_cast<int32>(MOVE_Falling));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
