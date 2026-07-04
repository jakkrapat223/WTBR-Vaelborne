# WTBR B7 Dedicated Server / Late Join Validation

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1 C++  
Baseline: `ae481d9` — Add B7 dedicated late join validation runbook  
Latest code baseline: `2461d0a` — Add B7D dedicated late join validation proof  
**B7 Status: PASS (2026-07-04)**

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

Status: **PASS** — as of `2461d0a` (Add B7D dedicated late join validation proof, 2026-07-04).

All six B7 pass criteria were observed in the B7D dedicated-server + late-join session:

- Dedicated server started and served the ThirdPersonMap validation session.
- Corpse container spawned on authority with real installed trigger entries.
- Client 1 interaction was server-authoritative (RPC chain confirmed).
- Late-join client 2 received correct non-empty container state.
- Corpse/container priority 1 was confirmed above dropped trigger and BR ground item after late join.
- Lower-priority actors were not mutated when corpse/container won.
- Production default `bUseCorpseLootContainerOnDeath = false` was not flipped.
- No Blueprint/WBP/UMG/.uasset/.umap/binary assets were modified.

Prior B7C result (PARTIAL PASS) is preserved as historical record. See the B7D section below for full evidence.

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

## B7B — Exec Harness Investigation — 2026-07-04

B7B added `UFUNCTION(Exec) WTBRDebugServerSpawnCorpseLootContainerForValidation(float DelaySeconds)` to `AWTBRGameMode`. Root cause finding: `UFUNCTION(Exec)` on `AGameModeBase` requires a `PlayerController` routing chain (`AGameModeBase::ProcessConsoleExec`). On a headless dedicated server commandlet at tick [1], no `PlayerController` exists → command silently dropped.

Only `TAutoConsoleVariable` CVars route through `IConsoleManager` and bypass the `PlayerController` chain. This was confirmed by `WTBR.CorpseLootContainerLifetimeSeconds = "0"` appearing at tick [1] while `UFUNCTION(Exec)` invocations produced no output.

B7B result: **BLOCKED**. Exec routing chain absent on headless dedicated server. Required redesign → B7C.

## ExecCmds Separator Discovery — 2026-07-04

UE5.1 `-ExecCmds` uses `,` (comma) as the command separator, **not** `;` (semicolon). When `;` is used, the entire string is treated as one command: the CVar name is parsed first, then the value is read as a float stopping at `0`, and everything after the `;` is silently ignored.

Evidence: `-ExecCmds="WTBR.CorpseLootContainerLifetimeSeconds 0;WTBR.UseCorpseLootContainerOnDeath 1;WTBR.B7ValidationSpawnCorpseContainerDelaySeconds 15"` produced only `WTBR.CorpseLootContainerLifetimeSeconds = "0"` in the server log. After changing to `,`, all three CVars were echoed at tick [1].

**Correct server command syntax uses `,`:**

```powershell
-ExecCmds="WTBR.CorpseLootContainerLifetimeSeconds 0,WTBR.UseCorpseLootContainerOnDeath 1,WTBR.B7ValidationSpawnCorpseContainerDelaySeconds 15"
```

## B7C — CVar-Driven Validation Harness — 2026-07-04

### Changes

Files modified (not staged, not committed):

- `Source/WTBR/WTBRGameMode.h` — Added to `#if !UE_BUILD_SHIPPING` private section: `FTimerHandle ValidationCVarPollTimerHandle`, `int32 ValidationSpawnRetryCount = 0`, `static constexpr int32 ValidationSpawnMaxRetries = 90`, `void PollValidationSpawnCVar()`.
- `Source/WTBR/WTBRGameMode.cpp` — Added `static TAutoConsoleVariable<float> CVarWTBRB7ValidationSpawnDelay(TEXT("WTBR.B7ValidationSpawnCorpseContainerDelaySeconds"), 0.0f, ...)` at file scope in `#if !UE_BUILD_SHIPPING`. Modified `BeginPlay` (authority, non-shipping) to start a 1-s repeating poll timer. Added `PollValidationSpawnCVar()` which detects CVar > 0, clears the poll timer, resets CVar, and schedules `SpawnCorpseLootContainerForValidation_Authority`. Updated `SpawnCorpseLootContainerForValidation_Authority()` with retry logic (max 90 × 2 s = 180 s) and `B7Validation:` log markers.

No production defaults changed. No Blueprint/WBP/UMG/.uasset/.umap/binary assets modified.

### Build

- `WTBREditor Win64 Development`: 9/9 actions, exit code 0, no warnings.

### Automation

- `WTBR.*` full suite: **69/69 PASS, 0 FAIL**. Baseline maintained.

### Dedicated Server Validation

Server command:

```powershell
$serverArgs = '"E:\World Trigger\WTBR-Vaelborne\WTBR.uproject" "/Game/ThirdPerson/Maps/ThirdPersonMap" -server -Unattended -NullRHI -NoSound -log -port=7777 -abslog="..." -ExecCmds="WTBR.CorpseLootContainerLifetimeSeconds 0,WTBR.UseCorpseLootContainerOnDeath 1,WTBR.B7ValidationSpawnCorpseContainerDelaySeconds 15"'
Start-Process -FilePath 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $serverArgs -PassThru -WindowStyle Hidden
```

Client 1 command (ExecCmds also uses `,`):

```powershell
$client1Args = '"E:\World Trigger\WTBR-Vaelborne\WTBR.uproject" "127.0.0.1:7777" -game -Unattended -NullRHI -NoSound -log -abslog="..." -ExecCmds="WTBRDebugCharacterPrintMatchState,WTBRDebugCharacterPrintTriggerSlots"'
Start-Process -FilePath 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $client1Args -PassThru -WindowStyle Hidden
```

Client 2 (late joiner) command:

```powershell
$client2Args = '"E:\World Trigger\WTBR-Vaelborne\WTBR.uproject" "127.0.0.1:7777" -game -Unattended -NullRHI -NoSound -log -abslog="..." -ExecCmds="WTBRDebugCharacterPrintMatchState,WTBRDebugCharacterPrintFocusedInteractionPrompt"'
Start-Process -FilePath 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $client2Args -PassThru -WindowStyle Hidden
```

### Observed Evidence (Server Log B7C_Server2.log)

At tick [1] (ExecCmds processed):

```
[06.47.51:852][  1]WTBR.CorpseLootContainerLifetimeSeconds = "0"
[06.47.51:854][  1]WTBR.UseCorpseLootContainerOnDeath = "1"
[06.47.51:854][  1]WTBR.B7ValidationSpawnCorpseContainerDelaySeconds = "15"
```

At tick [31] (~1 s after BeginPlay — poll detected CVar):

```
[06.47.52:846][ 31]LogConsoleManager: Warning: Setting the console variable
    'WTBR.B7ValidationSpawnCorpseContainerDelaySeconds' with 'SetByCode' was
    ignored as it is lower priority than the previous 'SetByConsole'. Value remains '15'
[06.47.52:846][ 31]LogTemp: B7Validation: CVar requested spawn delay 15.0 s.
[06.47.52:846][ 31]LogTemp: B7Validation: Scheduling corpse container spawn in 15.0 s.
```

Note: The `SetByCode` warning means the poll's CVar reset (`->Set(0.0f, ECVF_SetByCode)`) was lower priority than the console set. The poll timer is also cleared, so the CVar staying at 15 does not cause re-triggering.

Spawn attempts (13 retries while no pawn):

```
[06.48.07:853][486] B7Validation: Spawn attempt (try 1/91).
[06.48.07:853][486] B7Validation: Spawn failed reason — no authority pawn yet. Retry 1/90 in 2 s.
... (retries 2–13 every 2 s) ...
```

Client 1 join and successful spawn at try 14:

```
[06.48.32:936][244] LogNet: Join succeeded: DESKTOP-A9V5TS3-12DF
[06.48.34:093][279] B7Validation: Spawn attempt (try 14/91).
[06.48.34:093][279] B7Validation: Found authority character BP_WTBRCharacter_C_0 at X=900.000 Y=1110.000 Z=98.338.
[06.48.34:093][279] B7Validation: Created synthetic snapshots (2 entries). Spawn attempt at X=1020.000 Y=1110.000 Z=118.338.
[06.48.34:094][279] WTBR corpse loot container initialized: Container=BP_WTBRCorpseLootContainer_C_0 Entries=2
[06.48.34:094][279] B7Validation: Corpse container spawned — Actor=BP_WTBRCorpseLootContainer_C_0
    Location=X=1020.000 Y=1110.000 Z=118.338 Entries=2. Will replicate to all connected clients.
```

Late-join client 2 joined at tick [400] — container existed since tick [279]:

```
[06.49.44:581][400] LogNet: Join succeeded: DESKTOP-A9V5TS3-76F0
```

Client 2 log confirms replication system working (both characters received with `HasAuthority=false`):

```
[06.49.44:619][978] [Health BeginPlay] Owner=BP_WTBRCharacter_C_0 | HasAuthority=false
[06.49.44:620][978] [Health BeginPlay] Owner=BP_WTBRCharacter_C_1 | HasAuthority=false
```

### B7C Result Summary

| Item | Status |
|------|--------|
| Dedicated server startup | PASS |
| ThirdPersonMap load | PASS |
| IpNetDriver listen on port 7777 | PASS |
| `WTBR.CorpseLootContainerLifetimeSeconds = "0"` CVar | PASS |
| `WTBR.UseCorpseLootContainerOnDeath = "1"` CVar | PASS |
| `WTBR.B7ValidationSpawnCorpseContainerDelaySeconds = "15"` CVar | PASS |
| `B7Validation: CVar requested spawn delay` log marker | PASS |
| `B7Validation: Scheduling corpse container spawn` log marker | PASS |
| `B7Validation: Spawn attempt` retry log markers | PASS (13 retries) |
| `B7Validation: Found authority character` log marker | PASS |
| `B7Validation: Created synthetic snapshots` log marker | PASS |
| Production `WTBR corpse loot container initialized` log | PASS |
| `B7Validation: Corpse container spawned` log marker | PASS |
| Client 1 join | PASS |
| Client 2 late join (after container spawned) | PASS |
| Client 2 replication system functioning | PASS (both characters received, HasAuthority=false) |
| Production default `bUseCorpseLootContainerOnDeath = false` not flipped | PASS |
| No staged / committed files | PASS |
| Automation 69/69 maintained | PASS |

### B7C Remaining Gaps (not provable in headless commandlet)

- Explicit client-side container receipt log: `OnRep_LootEntries` fires a UI delegate only — no log output in headless NullRHI mode. Late-join replication is inferred from the replication system working correctly for other actors.
- Client 1 interaction pickup server-authority chain: requires interactive client session with `WTBRDebugCharacterLootNearestCorpseContainer`.
- Priority behavior over dropped trigger / BR ground item: not proven in this pass.
- Lower-priority actor non-mutation: not proven in this pass.

B7C result: **PARTIAL PASS — dedicated server spawn harness proven, late-join replication inferred. Client interaction, priority, and non-mutation remain pending for manual/interactive session.**

## B7D — Interactive Client Validation — 2026-07-04

### Changes

Files committed (`2461d0a` — Add B7D dedicated late join validation proof):

- `Source/WTBR/WTBRCharacter.h` — `#if !UE_BUILD_SHIPPING` private section: `FTimerHandle B7ClientValidationPollTimerHandle`, `B7ClientValidationTimerHandle`, `int32 B7ClientValidationRetryCount = 0`, `static constexpr int32 B7ClientValidationMaxRetries = 60`, `PollB7ClientValidationCVar()`, `RunB7ClientValidationSequence()`, `ContinueB7ClientValidationSequence()`, `FinishB7ClientValidationSequence()`, `LogB7ClientValidationWorldState()`.
- `Source/WTBR/WTBRCharacter.cpp` — `TAutoConsoleVariable<float> CVarWTBRB7ClientValidationDelay` (`WTBR.B7ClientValidationDelaySeconds`, default 0, `#if !UE_BUILD_SHIPPING`). `BeginPlay` on `NM_Client` starts a 1-s poll timer. Sequence: detect CVar, wait for local control, face container, `GetFocusedInteractionPromptText`, `RequestContextInteract`, `WTBRDebugCharacterLootNearestCorpseContainer`, pre/post world-state log. Retry bounded at `B7ClientValidationMaxRetries = 60` (120 s). Read/log + client-side view rotation + server RPC requests only — no client-side inventory/slot/loot/ground-item mutation.
- `Source/WTBR/WTBRGameMode.cpp` — Two new `#if !UE_BUILD_SHIPPING` CVars: `WTBR.B7ValidationForceInMatchRules` (runtime-only match override: `Phase=InMatch`, `bAllowCorpseLoot/TriggerPickup/TriggerSwapDuringMatch=true`; default 0) and `WTBR.B7ValidationSpawnLowerPriorityActors` (spawns one dropped trigger + one BR ground item as lower-priority context-interact candidates; default 0). Spawn path now prefers the authority pawn's real installed trigger snapshots; falls back to synthetic paths when no installed trigger exists.

No production defaults changed. No Blueprint/WBP/UMG/.uasset/.umap/binary assets modified.

### Build

- `WTBREditor Win64 Development`: 9/9 actions, exit code 0, no warnings.

### Automation

- `WTBR.*` full suite: **69/69 PASS, 0 FAIL**. Baseline maintained.

### Validation Commands

```powershell
$serverArgs = '"E:\World Trigger\WTBR-Vaelborne\WTBR.uproject" "/Game/ThirdPerson/Maps/ThirdPersonMap" -server -Unattended -NullRHI -NoSound -log -port=7777 -abslog="..." -ExecCmds="WTBR.CorpseLootContainerLifetimeSeconds 0,WTBR.UseCorpseLootContainerOnDeath 1,WTBR.B7ValidationForceInMatchRules 1,WTBR.B7ValidationSpawnLowerPriorityActors 1,WTBR.B7ValidationSpawnCorpseContainerDelaySeconds 15"'
Start-Process -FilePath 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $serverArgs -PassThru -WindowStyle Hidden
```

```powershell
$client1Args = '"E:\World Trigger\WTBR-Vaelborne\WTBR.uproject" "127.0.0.1:7777" -game -Unattended -NullRHI -NoSound -log -abslog="..." -ExecCmds="WTBR.B7ClientValidationDelaySeconds 20"'
Start-Process -FilePath 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $client1Args -PassThru -WindowStyle Hidden
```

```powershell
# Start client 2 approximately 60 s after client 1 to ensure late-join after container spawn.
$client2Args = '"E:\World Trigger\WTBR-Vaelborne\WTBR.uproject" "127.0.0.1:7777" -game -Unattended -NullRHI -NoSound -log -abslog="..." -ExecCmds="WTBR.B7ClientValidationDelaySeconds 15"'
Start-Process -FilePath 'E:\UE_5.1\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $client2Args -PassThru -WindowStyle Hidden
```

### Observed Evidence (B7D_Server.log, B7D_Client1.log, B7D_Client2.log)

Server — container spawn with real triggers at tick [483]:

```
[08.00.43:683][ 31] B7Validation: CVar requested spawn delay 15.0 s.
[08.00.43:683][ 31] B7Validation: Scheduling corpse container spawn in 15.0 s.
[08.00.48:931][189] LogNet: Join succeeded: DESKTOP-A9V5TS3-5EF7   (client 1)
[08.00.58:714][483] B7Validation: Spawn attempt (try 1/91).
[08.00.58:714][483] B7Validation: Found authority character BP_WTBRCharacter_C_0 at X=900.000 Y=1110.000 Z=98.338.
[08.00.58:714][483] B7Validation: Runtime match override applied — Phase=InMatch, corpse loot/trigger pickup/swap enabled (validation session only; production defaults unchanged).
[08.00.58:714][483] B7Validation: Created snapshots from installed triggers (2 entries, first=/Game/Data/Triggers/DA_Serpveil.DA_Serpveil).
[08.00.58:715][483] WTBR corpse loot container initialized: Container=BP_WTBRCorpseLootContainer_C_0 Entries=2
[08.00.58:715][483] B7Validation: Corpse container spawned — Actor=BP_WTBRCorpseLootContainer_C_0 Location=X=1020.000 Y=1110.000 Z=118.338 Entries=2. Will replicate to all connected clients.
[08.00.58:715][483] B7Validation: Lower-priority dropped trigger spawned — Actor=WTBRDroppedTriggerActor_0 Location=X=1020.000 Y=1210.000 Z=118.338.
[08.00.58:715][483] B7Validation: Lower-priority ground item spawned — Actor=WTBRGroundItemActor_0 Location=X=1170.000 Y=1110.000 Z=118.338.
```

Server — client 2 late join and both pickup RPCs accepted:

```
[08.01.10:509][837] WTBR corpse loot pickup swapped: character BP_WTBRCharacter_C_0 took container BP_WTBRCorpseLootContainer_C_0 entry 0 into slot 0 and returned previous slot trigger to same entry
[08.01.49:253][  4] LogNet: Join succeeded: DESKTOP-A9V5TS3-45B5   (client 2 — late join)
[08.02.06:282][513] WTBR corpse loot pickup swapped: character BP_WTBRCharacter_C_1 took container BP_WTBRCorpseLootContainer_C_0 entry 0 into slot 0 and returned previous slot trigger to same entry
```

Client 1 — container receipt, priority proof, RPC chain:

```
B7ClientValidation: Container receipt — Actor=BP_WTBRCorpseLootContainer_C_0 Distance=121.6 HasAuthority=false Entries=2 HasAvailableLoot=true
B7ClientValidation: [Pre] Container BP_WTBRCorpseLootContainer_C_0 Entries=2 [0]DA_Serpveil=available [1]DA_Acervyn=available
B7ClientValidation: [Pre] DroppedTrigger WTBRDroppedTriggerActor_0 Distance=157.4 ViewDot=0.98 IsConsumed=false
B7ClientValidation: [Pre] GroundItem WTBRGroundItemActor_0 Distance=270.7 ViewDot=1.00 IsConsumed=false Quantity=1
B7ClientValidation: Focused prompt='Open Trigger Cache' (empty=false).
[WTBR Interact] Handled by corpse/container/chest focus (priority 1).
B7ClientValidation: RequestContextInteract handled=true.
WTBRDebugCharacterLootNearestCorpseContainer: requesting container BP_WTBRCorpseLootContainer_C_0 entry 0 into slot 0 for character BP_WTBRCharacter_C_0.
B7ClientValidation: [Post] DroppedTrigger WTBRDroppedTriggerActor_0 IsConsumed=false
B7ClientValidation: [Post] GroundItem WTBRGroundItemActor_0 IsConsumed=false Quantity=1
B7ClientValidation: Sequence complete (character BP_WTBRCharacter_C_0).
```

Client 2 (late join) — container receipt after joining post-spawn, priority proof:

```
B7ClientValidation: Container receipt — Actor=BP_WTBRCorpseLootContainer_C_0 Distance=147.9 HasAuthority=false Entries=2 HasAvailableLoot=true
B7ClientValidation: [Pre] Container BP_WTBRCorpseLootContainer_C_0 Entries=2 [0]DA_Serpveil=available [1]DA_Acervyn=available
B7ClientValidation: [Pre] DroppedTrigger WTBRDroppedTriggerActor_0 Distance=220.7 ViewDot=0.94 IsConsumed=false
B7ClientValidation: [Pre] GroundItem WTBRGroundItemActor_0 Distance=283.5 ViewDot=0.99 IsConsumed=false Quantity=1
B7ClientValidation: Focused prompt='Open Trigger Cache' (empty=false).
[WTBR Interact] Handled by corpse/container/chest focus (priority 1).
B7ClientValidation: RequestContextInteract handled=true.
WTBRDebugCharacterLootNearestCorpseContainer: requesting container BP_WTBRCorpseLootContainer_C_0 entry 0 into slot 0 for character BP_WTBRCharacter_C_0.
B7ClientValidation: [Post] DroppedTrigger WTBRDroppedTriggerActor_0 IsConsumed=false
B7ClientValidation: [Post] GroundItem WTBRGroundItemActor_0 IsConsumed=false Quantity=1
B7ClientValidation: Sequence complete (character BP_WTBRCharacter_C_0).
```

Server cross-check — no dropped-trigger or ground-item pickup occurred:

- No `dropped trigger pickup` log in server log.
- No `ground item pickup` log in server log.
- No reject/error/rollback log in server log.

### B7D Result Summary

| Item | Status |
|------|--------|
| Build 9/9 | PASS |
| Automation 69/69 | PASS |
| Dedicated server startup | PASS |
| IpNetDriver listen on port 7777 | PASS |
| Runtime match override (InMatch, corpse loot/trigger pickup/swap) | PASS (validation session only; production defaults unchanged) |
| Container spawned on authority with real installed triggers (DA_Serpveil + DA_Acervyn) | PASS |
| Lower-priority dropped trigger spawned | PASS |
| Lower-priority ground item spawned | PASS |
| Client 1 join | PASS |
| **Client 1 container receipt** (`Entries=2 HasAvailableLoot=true HasAuthority=false`) | **PASS** |
| **Client 1 focused prompt** (`'Open Trigger Cache'`) | **PASS** |
| **Client 1 priority 1 win** (`Handled by corpse/container/chest focus (priority 1)`) | **PASS** |
| **Client 1 loot RPC chain** (server: `corpse loot pickup swapped`) | **PASS** |
| Client 2 late join (after container existed) | PASS |
| **Client 2 container receipt** (`Entries=2 HasAvailableLoot=true HasAuthority=false`) | **PASS** |
| **Client 2 focused prompt** (`'Open Trigger Cache'`) | **PASS** |
| **Client 2 priority 1 win after late join** (dropped trigger + ground item in view cone, priority 1 still wins) | **PASS** |
| **Client 2 loot RPC chain** (server: `corpse loot pickup swapped`) | **PASS** |
| **Lower-priority dropped trigger not mutated** (`[Post] IsConsumed=false`) | **PASS** |
| **Lower-priority ground item not mutated** (`[Post] IsConsumed=false Quantity=1`) | **PASS** |
| No dropped-trigger / ground-item pickup on server | PASS |
| Production default `bUseCorpseLootContainerOnDeath = false` not flipped | PASS |
| No staged / committed assets/logs/binaries | PASS |

B7D result: **PASS — all four B7 gaps proven in a live dedicated-server late-join session. B7 overall is PASS.**

## Next Pass

**B8 — Corpse Container Production Default Flip**

B7 PASS is the prerequisite for flipping the production default. B8 has not been started and is not implied by this document.

Pending before B8:
- Corpse bag UI / UMG (not implemented).
- Target slot selection UI (not implemented).
- Context interact dispatch wiring for remaining branches (generic interactable).
- Focus trace angle/radius/height tuning for production.
- Visual readability / placeholder scale.

B8 work item: set `bUseCorpseLootContainerOnDeath = true` as the production default (or equivalent CVar flip) after UI and remaining dispatch branches are complete and playtested.
