# WTBR Workflow Policy

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1 C++  
Confirmed baseline: 34bf3ec Add positive corpse loot lifetime automation test  
Automation confirmed: WTBR.CorpseLoot PASS 11/11

`WTBR.CorpseLoot.ContainerLifetimeCVarPositiveDestroysActor` is committed and pushed at 34bf3ec.

## Guardrails

- No `git add .`, `git add ..`, or `git add -A`.
- Stage specific files only.
- Commit only after human review.
- Push only after explicit human instruction.
- Never stage or commit `Source/.claude/settings.local.json`.
- AI/Codex/Claude/Gemini must not edit Blueprint, WBP, UMG, `.uasset`, or binary assets.
- Competitive multiplayer gameplay must remain server-authoritative.
- Do not mix unrelated systems in one pass.
- No Tick where timer/event works.
- No NetMulticast without review.
- Do not flip BR corpse-container production default until B7 dedicated server / late join passes.

## Workflow Policy v3

### S0 Inspect Only

Read, inspect, and report only. No edits, staging, commits, pushes, or asset changes.

### S1 Light Pass

Small, reviewable documentation, test, or low-risk code-only pass. Stage specific files only after review. Commit only after human approval. Push only after explicit instruction.

### S2 Gameplay/Network

Gameplay, authority, replication, input-to-server, RPC, inventory, combat, and state synchronization work. Requires tighter review and focused validation. Automation can support deterministic logic, but does not prove netcode safety.

### S3 Milestone/Production

Production defaults, feature gates, removal of fallback/legacy paths, multiplayer readiness, dedicated server, late join, and milestone integration. Requires explicit scope, validation, and human approval.

## Mental Gate

Before any pass, ask:

- Does this pass touch authority?
- Does this pass touch replication?
- Does this pass touch a server RPC?
- Does this pass change a production default?
- Does this pass remove fallback, legacy, or rollback behavior?

If yes, treat it as S2/S3 unless explicitly scoped otherwise.

## Automation Boundary

Automation validates deterministic logic only. Automation is not netcode-safe by itself and does not replace PIE, listen server, dedicated server, late join, or manual multiplayer validation.

