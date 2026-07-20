// Copyright Vaelborne: Dominion Project. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "WTBRCharacter.h"
#include "Components/WTBRHealthComponent.h"

// ═════════════════════════════════════════════════════════════════════════════
// Mark/Ping (MMB by convention) — team-only ground/enemy marker.
//
// Server_RequestMarkPing re-traces authoritatively and only ever writes into the
// COND_OwnerOnly IncomingMarkPing property of the instigator's own teammates —
// see WTBRCharacter.h's "Mark/Ping" section. Headless fixtures have no physics
// scene (line traces always find zero hits, see the project's Automation gotchas
// note), so the hit/enemy-branch logic is exercised through the extracted pure
// helpers (ShouldTreatMarkPingHitAsEnemy, GetMarkPingRecipients,
// CanRequestMarkPingNow) rather than through a real trace — the same "pull the
// interesting logic out of anything that touches a sweep/trace" pattern used
// throughout this codebase's other trace-adjacent tests.
// ═════════════════════════════════════════════════════════════════════════════

namespace
{
    class FWTBRMarkPingWorldFixture
    {
    public:
        explicit FWTBRMarkPingWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRMarkPingWorldFixture()
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

    AWTBRCharacter* MarkPingTest_SpawnCharacter(UWorld* World, int32 TeamId, const FVector& Location = FVector::ZeroVector)
    {
        if (!World)
        {
            return nullptr;
        }

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AWTBRCharacter* Character = World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
        if (Character && TeamId != INDEX_NONE)
        {
            // The fixture's bare UWorld has no NetDriver, so every spawned actor is
            // its own local authority (NM_Standalone) — SetTeamId's HasAuthority()
            // gate passes here the same way it does in the stance/composite tests.
            Character->SetTeamId(TeamId);
        }
        return Character;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMarkPingCooldownTest,
    "WTBR.Character.MarkPing.CooldownGatesRepeatRequests",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMarkPingCooldownTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRMarkPingWorldFixture Fixture(TEXT("WTBR_MarkPing_Cooldown"));
    AWTBRCharacter* Char = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), INDEX_NONE);
    if (!Char) return false;

    TestTrue(TEXT("Ready to ping immediately after spawn"), Char->CanRequestMarkPingNow());

    Char->Server_RequestMarkPing_Implementation(
        Char->GetActorLocation(), FVector_NetQuantizeNormal(FVector::ForwardVector));

    TestFalse(TEXT("Cooldown blocks an immediate repeat request"), Char->CanRequestMarkPingNow());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMarkPingEnemyDetectionTest,
    "WTBR.Character.MarkPing.EnemyDetection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMarkPingEnemyDetectionTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRMarkPingWorldFixture Fixture(TEXT("WTBR_MarkPing_EnemyDetection"));
    AWTBRCharacter* Instigator = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), 1);
    AWTBRCharacter* Teammate = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), 1);
    AWTBRCharacter* Enemy = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), 2);
    AWTBRCharacter* Teamless = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), INDEX_NONE);
    if (!Instigator || !Teammate || !Enemy || !Teamless) return false;

    TestFalse(TEXT("A teammate is never treated as an enemy ping"),
        Instigator->ShouldTreatMarkPingHitAsEnemy(Teammate));
    TestTrue(TEXT("A living character on a different team is an enemy ping"),
        Instigator->ShouldTreatMarkPingHitAsEnemy(Enemy));
    TestFalse(TEXT("Self is never treated as an enemy ping"),
        Instigator->ShouldTreatMarkPingHitAsEnemy(Instigator));
    // Teamless is hostile-by-default from a team-having instigator's perspective:
    // "friendly" requires proving a matching assigned team, not just failing to
    // prove hostility. (Owner-found PIE bug: an earlier version of this function
    // required the TARGET to HasTeam() before calling it an enemy, which broke
    // every enemy ping in a no-team/solo BR match — nobody there has a team.)
    TestTrue(TEXT("A teamless character IS an enemy ping to a team-having instigator"),
        Instigator->ShouldTreatMarkPingHitAsEnemy(Teamless));
    TestFalse(TEXT("A null hit actor (no trace hit) is a location ping, not an enemy ping"),
        Instigator->ShouldTreatMarkPingHitAsEnemy(nullptr));

    // The actual PIE-observed case: solo BR gives EVERYONE TeamId == INDEX_NONE,
    // instigator included — this is the pairing that was silently broken before.
    AWTBRCharacter* OtherTeamless = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), INDEX_NONE);
    if (!OtherTeamless) return false;
    TestTrue(TEXT("Two teamless characters (solo BR) are enemies to each other"),
        Teamless->ShouldTreatMarkPingHitAsEnemy(OtherTeamless));

    if (IsValid(Enemy->HealthComponent))
    {
        Enemy->HealthComponent->ApplyDamage(Enemy->HealthComponent->GetMaxHP() + 1000.0f, nullptr);
    }
    TestFalse(TEXT("A dead (non-Alive) enemy is a location ping, not an enemy ping"),
        Instigator->ShouldTreatMarkPingHitAsEnemy(Enemy));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMarkPingRecipientsAreTeamOnlyTest,
    "WTBR.Character.MarkPing.RecipientsAreTeamOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMarkPingRecipientsAreTeamOnlyTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRMarkPingWorldFixture Fixture(TEXT("WTBR_MarkPing_Recipients"));
    AWTBRCharacter* Instigator = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), 1);
    AWTBRCharacter* Teammate = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), 1);
    AWTBRCharacter* Enemy = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), 2);
    AWTBRCharacter* Teamless = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), INDEX_NONE);
    if (!Instigator || !Teammate || !Enemy || !Teamless) return false;

    const TArray<AWTBRCharacter*> Recipients = Instigator->GetMarkPingRecipients();

    TestTrue(TEXT("Instigator receives their own ping"), Recipients.Contains(Instigator));
    TestTrue(TEXT("Teammate receives the ping"), Recipients.Contains(Teammate));
    TestFalse(TEXT("Enemy never receives the ping"), Recipients.Contains(Enemy));
    TestFalse(TEXT("A teamless character never receives another player's ping"), Recipients.Contains(Teamless));
    TestEqual(TEXT("Exactly instigator + teammate, nobody else"), Recipients.Num(), 2);

    // A teamless instigator (FFA / no-team match rules) only ever reaches themselves.
    const TArray<AWTBRCharacter*> TeamlessRecipients = Teamless->GetMarkPingRecipients();
    TestEqual(TEXT("A teamless instigator's only recipient is themselves"), TeamlessRecipients.Num(), 1);
    TestTrue(TEXT("...and it is actually them"), TeamlessRecipients.Contains(Teamless));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMarkPingDeliveryPopulatesActivePingsTest,
    "WTBR.Character.MarkPing.DeliveryPopulatesRecipientActivePings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMarkPingDeliveryPopulatesActivePingsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRMarkPingWorldFixture Fixture(TEXT("WTBR_MarkPing_Delivery"));
    AWTBRCharacter* Instigator = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), 1, FVector::ZeroVector);
    AWTBRCharacter* Teammate = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), 1, FVector(500.0, 0.0, 0.0));
    AWTBRCharacter* Enemy = MarkPingTest_SpawnCharacter(Fixture.GetWorld(), 2, FVector(1000.0, 0.0, 0.0));
    if (!Instigator || !Teammate || !Enemy) return false;

    TestEqual(TEXT("No active pings before any request"), Instigator->GetActiveMarkPings().Num(), 0);

    // Headless has no physics scene, so this trace always misses (see the file
    // header comment) — it lands as a location ping at max range, not on Enemy.
    // That is exactly what this test needs: it isolates "did delivery reach the
    // right recipients" from "did the trace hit the right actor" (covered
    // separately by ShouldTreatMarkPingHitAsEnemy above).
    Instigator->Server_RequestMarkPing_Implementation(
        Instigator->GetActorLocation(), FVector_NetQuantizeNormal(FVector::ForwardVector));

    TestEqual(TEXT("Instigator gets exactly one active ping"), Instigator->GetActiveMarkPings().Num(), 1);
    TestEqual(TEXT("Teammate gets exactly one active ping"), Teammate->GetActiveMarkPings().Num(), 1);
    TestEqual(TEXT("Enemy receives no active ping at all"), Enemy->GetActiveMarkPings().Num(), 0);

    if (Instigator->GetActiveMarkPings().Num() == 1)
    {
        TestFalse(TEXT("A miss (no LOS hit in headless) resolves as a location ping"),
            Instigator->GetActiveMarkPings()[0].bIsEnemy);
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
