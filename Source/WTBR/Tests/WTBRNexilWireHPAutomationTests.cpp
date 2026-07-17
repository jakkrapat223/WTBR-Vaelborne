// Copyright Vaelborne: Dominion Project. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Actors/WTBRNexilWireActor.h"
#include "Actors/WTBRProjectileBase.h"
#include "WTBRCharacter.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
