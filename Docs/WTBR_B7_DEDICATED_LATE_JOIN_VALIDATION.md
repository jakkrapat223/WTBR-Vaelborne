# WTBR B7 Dedicated Server / Late Join Validation

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1 C++  
Baseline: `ae481d9` — Add B7 dedicated late join validation runbook  
Latest code baseline: `c3dd314` — Add S8B corpse container priority automation

## Purpose

B7 validates corpse/container behavior under dedicated server and late-join conditions before any corpse-container production default flip. This pass must not flip the production default.

## Hard Rules

- Gameplay remains server-authoritative.
- Client/UI must not mutate inventory, slots, loot, ground items, or corpse-container entries directly.
- Do not edit Blueprint/WBP/UMG/.uasset/.umap/binary assets for this validation unless a human explicitly approves.
- Do not stage `.claude/settings.local.json`, `Source/.claude/settings.local.json`, or the ThirdPersonMap external-actor `.uasset`.
- Do not use `git add .`, `git add ..`, or `git add -A`.
- Do not flip `WTBR.UseCorpseLootContainerOnDeath` production default in this pass.
- Do not remove the legacy dropped-trigger path.
- Do not hardcode physical key F in gameplay logic.
- Build and automation evidence must come from the current working tree.

## Current Automation Gate

Current automated coverage proves deterministic corpse/container behavior, server-authoritative mutation bodies, and priority routing in transient worlds:

- `WTBR.CorpseLoot` covers container entry filtering, consumed state, rollback, UI read-only model, lifetime CVar behavior, and S8B priority tests.
- `WTBR.Inventory` covers inventory and BR ground item pickup semantics.
- Full WTBR automation covers S7/S8 context-interact priority behavior.

Automation does not prove net driver behavior, client join replication, or late-join state. Those remain manual/dedicated validation items.

## Dedicated Server Smoke Command

Use this as the first machine-checkable validation. It proves the editor commandlet can load the project map as a dedicated server process and execute server-side debug commands, but it does not prove late join by itself.

```powershell
& 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'E:\World Trigger\WTBR-Vaelborne\WTBR.uproject' `
  '/Game/ThirdPerson/Maps/ThirdPersonMap' `
  -server `
  -Unattended `
  -NullRHI `
  -NoSound `
  -log `
  -ExecCmds='WTBRDebugPrintMatchState;Quit'
```

Expected server log evidence:

- map loads without fatal errors;
- `IpNetDriver` starts listening on port `7777`;
- `WTBRDebugPrintMatchState` prints authoritative match state, or clearly reports why GameMode/GameState is unavailable.

Observed Codex smoke result on 2026-07-04:

- `ThirdPersonMap` loaded as dedicated server.
- `GameNetDriver IpNetDriver_0` listened on port `7777`.
- No fatal errors were observed in the startup log excerpt.
- `ExecCmds='WTBRDebugPrintMatchState;Quit'` did not close the commandlet within the 5-minute timeout, so the process had to be stopped manually.

This is a partial startup/listen smoke PASS, not a late-join PASS and not a clean-exit PASS. If this smoke is repeated, stop the commandlet after collecting the startup evidence if `Quit` is ignored.

## Manual Dedicated / Late-Join Validation Checklist

This section is the human/manual remainder required by the workflow policy because UE5.1.1 commandlet automation in this repo does not currently provide a reliable multi-process late-join harness.

### 1. Start Dedicated Server

```powershell
& 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor.exe' `
  'E:\World Trigger\WTBR-Vaelborne\WTBR.uproject' `
  '/Game/ThirdPerson/Maps/ThirdPersonMap' `
  -server `
  -log `
  -port=7777 `
  -ExecCmds='WTBR.CorpseLootContainerLifetimeSeconds 0;WTBR.UseCorpseLootContainerOnDeath 1'
```

Notes:

- `WTBR.UseCorpseLootContainerOnDeath 1` is a runtime validation override only.
- Do not change the production default CVar value in source.
- Keep the server log open.

### 2. Join First Client

```powershell
& 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor.exe' `
  'E:\World Trigger\WTBR-Vaelborne\WTBR.uproject' `
  '127.0.0.1:7777' `
  -game `
  -log `
  -windowed `
  -ResX=1280 `
  -ResY=720
```

In the server console or an authoritative validation path, set the match to in-match:

```text
WTBRDebugSetMatchPhase InMatch
WTBRDebugPrintMatchState
```

On client 1, run:

```text
WTBRDebugCharacterPrintMatchState
WTBRDebugCharacterPrintTriggerSlots
```

Pass criteria:

- client connects to the dedicated server;
- match phase is `InMatch`;
- corpse loot and trigger pickup rules are enabled for the validation mode;
- the player has installed trigger snapshots before attempting a container spawn/death-drop.

### 3. Create Corpse Container On Server

Preferred validation path:

1. Use the normal elimination/death flow with `WTBR.UseCorpseLootContainerOnDeath 1`.
2. Confirm the server log contains:

```text
WTBR corpse loot death spawn: corpse loot container path selected.
WTBR corpse loot container path completed
WTBR corpse loot container initialized
```

Fallback debug path if the full elimination setup is blocked:

```text
WTBRDebugCharacterSpawnCorpseLootContainer
```

Pass criteria:

- container spawns on authority only;
- spawned container has `Entries > 0`;
- lifetime is disabled for validation via `WTBR.CorpseLootContainerLifetimeSeconds 0`;
- no legacy dropped-trigger death-drop log appears for this validation spawn.

### 4. Client 1 Interaction / Server Authority

On client 1:

```text
WTBRDebugCharacterPrintFocusedInteractionPrompt
WTBRDebugCharacterLootNearestCorpseContainer 0 0
WTBRDebugCharacterPrintTriggerSlots
```

Pass criteria:

- focused prompt sees the corpse container when aimed/in range;
- pickup request is accepted or rejected by server logs, not by client-side mutation;
- if accepted, server log contains `WTBR corpse loot pickup`;
- if target slot replacement fails, container entry is rolled back and remains available;
- no client-only inventory/slot mutation occurs without corresponding server log.

### 5. Late Join Client

After the corpse container exists and before consuming all entries, join client 2:

```powershell
& 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor.exe' `
  'E:\World Trigger\WTBR-Vaelborne\WTBR.uproject' `
  '127.0.0.1:7777' `
  -game `
  -log `
  -windowed `
  -ResX=1280 `
  -ResY=720
```

On client 2:

```text
WTBRDebugCharacterPrintMatchState
WTBRDebugCharacterPrintFocusedInteractionPrompt
```

Pass criteria:

- client 2 joins after the container already exists;
- client 2 can see/focus the existing corpse container;
- prompt is non-empty when the container has available entries;
- client 2 does not see stale empty state while the server container still has loot;
- any pickup request from client 2 is validated on the server.

### 6. Priority And Lower-Priority Non-Mutation

Create or keep a lower-priority dropped trigger / BR ground item near the same interaction area. With the corpse container focused:

```text
WTBRDebugCharacterPrintFocusedInteractionPrompt
WTBRDebugCharacterListNearbyDroppedTriggers
WTBRDebugCharacterLootNearestCorpseContainer 0 0
```

Pass criteria:

- corpse/container priority wins over dropped trigger and BR ground item;
- dropped trigger is not consumed when corpse/container wins;
- BR ground item is not consumed/removed when corpse/container wins;
- corpse/container interaction remains priority 1 after late join.

## B7 Pass / Fail Criteria

B7 can be marked PASS only when all of these are true:

- dedicated server starts and runs the ThirdPersonMap validation session;
- at least one corpse container is spawned/maintained on the dedicated server;
- first client interaction is server-authoritative;
- late-join client sees the correct non-empty container state;
- corpse/container priority remains above dropped trigger and BR ground item after late join;
- lower-priority actors are not mutated when corpse/container wins;
- no production default is flipped;
- no Blueprint/WBP/UMG/.uasset/.umap/binary assets are modified.

## Current B7 Status

Status: **PARTIAL / manual corpse-container late-join validation required**.

The code inspection and existing automation show the expected replication and authority hooks:

- `AWTBRCorpseLootContainerActor` replicates and replicates `LootEntries`.
- `OnRep_LootEntries` broadcasts read-only UI state refresh.
- `Server_RequestPickupCorpseLootEntry_Implementation` re-validates authority, alive state, match rules, range, entry validity, target replacement, consume, and rollback.
- S8B automation proves priority 1 beats dropped trigger and BR ground item candidates in deterministic headless tests.

This document does not mark B7 PASS by itself because actual dedicated server plus late-join client replication must still be observed.

## B7A Codex Validation Attempt — 2026-07-04

Environment:

- Current baseline: `ae481d9` — Add B7 dedicated late join validation runbook.
- Working tree validation was run from `E:\World Trigger\WTBR-Vaelborne\Source`.
- No gameplay code, tests, Blueprint/WBP/UMG/.uasset/.umap/binary assets, or production defaults were modified.

Commands used:

```powershell
$ArgString = '"E:\World Trigger\WTBR-Vaelborne\WTBR.uproject" "/Game/ThirdPerson/Maps/ThirdPersonMap" -server -Unattended -NullRHI -NoSound -log -port=7777 -ExecCmds="WTBR.CorpseLootContainerLifetimeSeconds 0;WTBR.UseCorpseLootContainerOnDeath 1"'
Start-Process -FilePath 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $ArgString -PassThru -WindowStyle Hidden
```

```powershell
$ArgString = '"E:\World Trigger\WTBR-Vaelborne\WTBR.uproject" "127.0.0.1:7777" -game -Unattended -NullRHI -NoSound -log -abslog="E:\World Trigger\WTBR-Vaelborne\Source\B7_Client1.log" -ExecCmds="WTBRDebugCharacterPrintMatchState;WTBRDebugCharacterPrintTriggerSlots;Quit"'
Start-Process -FilePath 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $ArgString -PassThru -WindowStyle Hidden
```

```powershell
$ArgString = '"E:\World Trigger\WTBR-Vaelborne\WTBR.uproject" "127.0.0.1:7777" -game -Unattended -NullRHI -NoSound -log -abslog="E:\World Trigger\WTBR-Vaelborne\Source\B7_Client2.log" -ExecCmds="WTBRDebugCharacterPrintMatchState;WTBRDebugCharacterPrintFocusedInteractionPrompt;Quit"'
Start-Process -FilePath 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $ArgString -PassThru -WindowStyle Hidden
```

Cleanup:

```powershell
Stop-Process -Id <server/client commandlet pids> -Force
```

Observed evidence:

- Dedicated server startup: **PASS**.
- `ThirdPersonMap` load: **PASS**.
- Server GameMode: **PASS** (`BP_WTBRGameMode_C`).
- `IpNetDriver` listen on port `7777`: **PASS**.
- Client 1 connect: **PASS** (`Welcomed by server`; server log `Join succeeded`).
- Client 2 late join after client 1: **PASS** (`Welcomed by server`; server log second `Join succeeded`).
- Runtime validation override `WTBR.CorpseLootContainerLifetimeSeconds 0`: **observed in server log**.
- Runtime validation override `WTBR.UseCorpseLootContainerOnDeath 1`: **passed on command line**, but no corpse-container death/drop path was executed in this commandlet-only attempt.
- Clean auto-exit: **FAIL / reproduced**. Server and clients did not exit on `Quit` in `-ExecCmds`; processes had to be stopped manually after evidence collection.

Blocked / not proven:

- Corpse/container state was not created on the dedicated server in this attempt.
- Client 1 corpse/container observation and interaction were not proven.
- Late-join client 2 corpse/container replication was not proven.
- Late-join stale/empty-state behavior was not proven.
- Priority over dropped trigger / BR ground item after late join was not proven.
- Lower-priority dropped trigger / ground item non-mutation after late join was not proven.

Reason for block:

- The current repo/runbook provides Exec debug commands for server authority and character-local validation, but this commandlet/background setup has no reliable way to send a new authoritative server console command after clients have joined.
- `WTBRDebugCharacterSpawnCorpseLootContainer` requires an authority character with installed trigger snapshots. Running it from a commandlet client would be client-local and rejected by authority checks; running it from startup `-ExecCmds` happens before a joined player pawn is available.
- A true B7 PASS still needs either an interactive dedicated server console/manual validation session or a small approved validation harness/automation seam that can create corpse-container state on the server after a client has joined.

B7A result: **PARTIAL**. Dedicated server startup, map load, net listen, client 1 join, and client 2 late join were validated. Corpse/container late-join replication and priority behavior remain **PENDING/BLOCKED**.
