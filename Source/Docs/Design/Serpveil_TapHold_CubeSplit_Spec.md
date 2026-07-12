# Serpveil — Tap/Hold Cube-Split Rework, Gameplay Spec v0.1

Status: DRAFT — decisions captured from design owner 2026-07-12 (in-chat),
not yet implemented. Written against baseline `de416ad`.
Scope: **gameplay** (input flow, split, damage, presets). VFX beats are
referenced but authored separately per `Docs/VFX/Serpveil_EnergyCubeLaunch_VFX_Spec.md`.

This spec supersedes two earlier locks, deliberately:
1. "Serpveil = single cube, split is a no-op (N=1)" — Serpveil now splits like
   every other energy projectile trigger. `SerpveilCubeSplitCount` already
   exists in the DataAsset (currently 3).
2. VFX spec §1 hard constraint "no windup exists / VFX must never delay fire" —
   a real, server-authoritative **windup now exists by design** (see §3). The
   constraint becomes: *VFX must never delay fire beyond the designed windup.*

---

## 1. Design summary (locked by design owner)

| Aspect | Decision |
|---|---|
| Fire modes | **Tap** and **Hold** (fits the project-wide Q/E tap+hold input lock) |
| Tap behavior | Conjure → short delay → split into N cubes → all launch **flat/straight** (Solux-like volley, no turns) |
| Hold behavior | Conjure → short delay → split into N cubes → launch along the **lobby-configured preset** paths |
| Split timing | Both modes: conjure happens first, split+launch after a **0.3–0.5 s delay** (exact value TBD via playtest) |
| Damage | **Per-cube fixed value** from DataAsset (example: 15/cube; 3 cubes hit = 45). Not divided from a total. |
| Presets | Configured **only in the Lobby / pre-match screen**. No free-draw during a match. A preset assigns cubes to paths (e.g. 5 cubes: 2 left, 3 right) and each path may turn at most **3–4 times** (cap TBD). |
| Path style | **Straight segments with sharp-angle turns** (polyline/zigzag), NOT smooth curves — confirmed against the canon Viper reference (anime, Yuma vs Miwa: volley bolts fly straight, bend sharply 3–4 times, converge on the target from multiple directions). Maps 1:1 onto `InitializePathMovement` waypoints; turn count = intermediate waypoint count. The existing smooth presets (Curve/S-Curve/Hook/Spiral) are retired for Serpveil along with the charge mechanic. |
| Identity note | The old "hold-to-charge (proportional cost) + single curved path" Serpveil is retired by this rework. The conjure→split beat is this project's stylization (from the Hound fan-game reference) — the anime fires bolts directly; we keep our cube language. |

## 2. What already exists to build on

- `AWTBRProjectileBase::SpawnCubeSplits()` (`WTBRProjectileBase.cpp:588`) —
  authority-side split spawning with damage assignment and a `OnCubeSplitVFX()`
  BP hook. Currently fans 90° straight and divides `BaseDamage`; needs a
  per-cube-damage variant and (for Hold) path-following splits.
- `InitializePathMovement(Points, Speed, Instigator)` — multi-waypoint path
  movement already drives today's Serpveil; a preset path = exactly such a
  point list. Turn cap = waypoint-count cap.
- `SerpveilCubeSplitCount` (DA, =3), `FWTBRSerpveilParams` (charge fields will
  be deprecated/repurposed by this spec).
- Replicated telegraph pattern (`SerpveilChargeTelegraph` → BP event) — reuse
  as the **windup telegraph** (cube conjured at hand = enemy-readable warning).
- VFX: `NS_Base_EnergyCubeLaunch` (Beat A materialize + Beat B trail) attaches
  to the projectile class — **each split cube automatically gets its own
  materialize+trail** with zero extra VFX wiring, because splits spawn
  `GetClass()`.

## 3. Fire flow (both modes)

```
input → server validates cost/cooldown
      → CONJURE: telegraph ON (cube at hand, replicated)     t=0
      → WINDUP: fixed delay 0.3–0.5 s (server timer)          t=delay
      → SPLIT+LAUNCH: spawn N projectiles (per-cube damage),
        telegraph OFF                                         t=delay
```

- Windup is server-authoritative; input release/extra presses during windup do
  nothing (no charge scaling anymore).
- TBD: is the windup interruptible (owner staggered/killed → cancel + refund?).
- Tap: N launch directions = flat horizontal spread around aim (spread angle
  TBD, DA-driven).
- Hold: N paths from the selected preset, relative to aim direction at launch.

## 4. Data model

New/changed `FWTBRSerpveilParams` fields (DA-driven per the project's
DataAsset rule):

- `SerpveilPerCubeDamage` (float, e.g. 15) — replaces total-damage semantics.
- `SerpveilCubeSplitCount` (int, exists) — cubes per shot.
- `SerpveilSplitDelay` (float, 0.3–0.5 TBD).
- `SerpveilTapSpreadDeg` (float, TBD).
- `SerpveilVaelCost` — TBD: flat per shot? per cube? (old VaelPerSecond charge
  drain no longer applies).
- Preset: `FSerpveilPathPreset { Paths: [ { Waypoints ≤ (TurnCap+1), CubeCount } ] }`
  stored per player profile; edited in Lobby (see §6). Selection UI in lobby;
  the match only reads the chosen preset.

## 5. Phased implementation plan (each phase playable + committable)

- **S1 — Tap mode (smallest slice):** conjure telegraph → fixed delay → split
  into N straight cubes with per-cube damage. No presets, no lobby. C++ work:
  windup timer in `WTBRSerpveilTrigger`, per-cube damage in split spawn, flat
  spread. VFX: conjure asset (short-hold variant of Beat A) slots into the
  existing telegraph hook.
- **S2 — Hold mode with dev presets:** 2–3 hardcoded presets (e.g. FanLeft,
  SplitLR, Spiral) selectable by key, driving `InitializePathMovement` per
  cube. Proves multi-path split end-to-end without any UI work.
- **S3 — Lobby preset editor:** UMG editor (drag path points, assign cube
  counts, enforce turn cap), persistence (SaveGame/profile), preset selection
  pre-match. Largest chunk; UMGJsonGenerator pipeline available for widget
  scaffolding. Keep strictly separate from S1/S2 commits.
- **S4 — Balance pass:** delay value, per-cube damage, spread, Vael cost —
  numbers only, after S1–S3 are mechanically verified.

## 6. Open TBDs (decide before the phase that needs them)

1. Exact windup delay (0.3 / 0.4 / 0.5) — S1 playtest.
2. Vael cost model (flat vs per-cube) — S1.
3. Windup interruption/refund rules — S1.
4. Tap spread angle + whether tap and hold share cooldown — S1/S2.
5. Turn cap 3 vs 4, waypoint spacing/length limits — S2.
6. Default cube count (DA says 3; design example used 5) — S2.
7. Preset storage location (SaveGame vs profile service) — S3.
8. Whether the retired charge-scaling returns in any form (e.g. hold longer =
   wider spread) — explicitly out of scope for S1–S4 unless re-raised.

## 7. Interaction with other systems (flag, don't solve here)

- **Clash splits:** `SpawnCubeSplits()` is also the bullet-clash mechanic. A
  Serpveil cube that clashes mid-air would split *again* (cube of a cube) —
  decide whether `bIsCubeSplit` blocks re-splitting (it likely should).
- **Venyx/Hound, Solux/Astria, Fulgrix/Meteo:** this spec's split+per-cube
  damage machinery is the shared foundation those weapons will reuse with
  their own CubeCounts; their designs (Hound staggered batches etc.) come as
  separate specs.
- **VFX split beat:** the "1 big cube → N small cubes" visual at the hand
  during windup gets its own short VFX spec once S1 is playable (the old
  Serpveil VFX spec's open question #2).
