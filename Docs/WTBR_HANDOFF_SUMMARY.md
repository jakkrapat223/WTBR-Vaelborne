# WTBR Handoff Summary

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1 C++  
Confirmed baseline: 34bf3ec Add positive corpse loot lifetime automation test  
Automation confirmed: WTBR.CorpseLoot PASS 11/11

`WTBR.CorpseLoot.ContainerLifetimeCVarPositiveDestroysActor` is committed and pushed at 34bf3ec. Competitive multiplayer gameplay must remain server-authoritative. `Source/.claude/settings.local.json` may exist as a local-only untracked file and must never be staged or committed.

## Repo Paths

- PC: `E:\World Trigger\WTBR-Vaelborne`
- Notebook: `C:\Trigger\WTBR-Vaelborne`

## Module And Build Layout

- Module: `WTBR`
- Build.cs: `Source/WTBR/WTBR.Build.cs`
- Tests: `Source/WTBR/Tests/`

## Done

- Corpse loot automation foundation.
- Corpse loot container tests.
- Positive lifetime test committed and pushed at 34bf3ec.
- B1-B6 validation PASS.
- Nexil cleanup fix PASS.

## Remaining

- Pass F smoke audit.
- B7 dedicated server / late join.
- Real interact input.
- Loot UI.
- Match flow.
- Composite input/UI.
- Aegorn completion.
- Nexil destructible wire.
- Shrinking zone.
- Capture-zone Black Trigger objective.
- Down/Revive/Respawn.
- Loadout preset feature.
- Cube split multi-projectile model.

## Testability Map

Automation-testable pure logic:

- Corpse loot struct/predicate behavior.
- Corpse loot container entry filtering, ordering, consumed state, rollback, prompt/availability, all-consumed state, zero lifetime, and positive lifetime CVar behavior.
- Deterministic rule helpers and pure data transformations.

S2/S3 PIE/network required items:

- Full `AWTBRCharacter::Server_RequestPickupCorpseLootEntry` path.
- Invalid target slot through RPC.
- `ReplaceTriggerSlotFromDataAsset` end-to-end.
- Destroy-after-last-pickup through character flow.
- Focused trace end-to-end.
- Real interact input.
- Replication correctness.
- Dedicated server / late join.
- Match rules, travel, reset, and production defaults.

## Still Unknown Or Placeholder

- Respawn point final spec.
- Energy cube split rounding rule.
- Final Vael costs, rewards, and max pool.
- Black Trigger pool, balance, and final indicator.
- Matchmaking and persistence details.
- Dedicated server production rollout details.

## Hard Gate

B7 dedicated server / late join must pass before flipping the corpse-container production default.

## Human Test Gate

Before requesting any Human PIE/manual test, apply Human Test Gate v1.1 from `Docs/WTBR_WORKFLOW_POLICY.md`:

- Gate 1: changed path filter using `git diff --name-only <last-validated-baseline> --` and `git status --short -uall`.
- Gate 2: relevant automation against the current working tree.
- Gate 3: human-only remainder for changed paths not provable by automation.

The assistant/AI must restate this gate before asking for manual PIE. Human checklists must show which cases were removed by Gate 1 and Gate 2.

## Clear Before New Chat

Run:

```powershell
git status --short -uall
git log --oneline --decorate -5
```

Expected latest baseline:

```text
34bf3ec Add positive corpse loot lifetime automation test
```

`Source/.claude/settings.local.json` may exist as a local-only untracked file.

## Safe Commit And Push Pattern

```powershell
git status --short -uall
git diff -- <specific files>
git add <specific files only>
git diff --cached --stat
git diff --cached
git commit -m "<reviewed message>"
```

Push only after explicit human instruction:

```powershell
git push
```
