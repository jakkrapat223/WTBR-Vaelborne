# Serpveil — Energy Cube Launch VFX Spec v0.1

Status: DRAFT — not locked. Written against code as of baseline `74523d3`.
Scope: visual design only, first prototype (Serpveil, `CubeCount = 1`) of the
shared **Energy Cube Launch** language that will later extend to every
energy-based projectile trigger (Astria/Solux, Hound/Venyx, Meteo/Fulgrix,
Viper is Serpveil itself). See project memory `energy-cube-launch-vfx-decision`
for the full cross-weapon design lock this doc implements the first slice of.

This doc replaces the earlier "Serpveil FireFlash" plan — a separate one-shot
muzzle flash was judged unnecessary once this sequence was designed; the
launch beat below **is** the release flash.

---

## 1. Identity

Every energy projectile trigger fires the same way, visually: a cube of
condensed energy forms, then launches as a comet with a self-drawing light
trail. For Serpveil specifically there is only one cube (`CubeCount = 1`), so
the "split into N cubes" beat that multi-cube weapons (Hound = 6, Astria = 5,
Meteo = 4) will need is a no-op here — this prototype only has to prove
**materialize → launch**, not the split/fan-out step. That step is deferred
to whichever weapon is implemented next (see open questions).

Reference mechanics (`WTBRSerpveilTrigger.h/.cpp`, `FWTBRSerpveilParams`):

| Param | Behavior | Constraint on VFX |
|---|---|---|
| Press (`OnTriggerActivated`) | starts charge tracking, client-side | drives existing `ChargeGlow` — untouched by this doc |
| Release (`OnTriggerDeactivated`) | sends `Server_FireSerpveil` RPC | — |
| Server fire (`ExecuteServerFire`, `WTBRSerpveilTrigger.cpp:280-354`) | spawns `AWTBRProjectileBase` via `SpawnActorDeferred`, calls `InitializePathMovement` **immediately**, then `OnSerpveilFired(Shape)` | **no windup exists in code today** — the projectile is already moving the instant it exists |

**Hard constraint (per project rule, do not violate):** this VFX must be a
pure cosmetic overlay and must **never gate or delay** the actual fire RPC or
projectile spawn. There is currently no windup phase to "fill" with a
materialize animation — the cube-conjure visual has to be layered on top of
an already-instantaneous, already-authoritative shot, not played *before* it.
Resolution used below: the materialize beat plays at the projectile's spawn
transform for its first ~0.15s while the projectile is (cosmetically) treated
as still "condensing" even though it is already physically moving under
server authority — the player only perceives the cube because it is small
and the projectile's initial travel distance in 0.15s is short. Do not add
any `Delay` node or timer that holds `ExecuteServerFire` or the projectile's
`InitializePathMovement` call to wait for this beat to finish.

---

## 2. Timeline & VFX beats

```
t=0 (projectile spawns, already moving)      t≈0.15s              t≈0.6s (trail settles)
|--- MATERIALIZE (cosmetic only) ------------|--- LAUNCH / TRAIL --------------------|
```

**Beat A — Materialize (t=0→0.15s)**
- A small cube (single glowing shape, not a burst of fragments — see §4 for
  why "clean split" language matters even at N=1) fades/scales in at the
  projectile's current transform.
- Very short — this is not a charge-up, it's a "the shot already happened,
  here's the cube catching up visually" beat. Keep it under 0.2s or it will
  read as input lag once tested at real match pace, not in a slow demo loop.

**Beat B — Launch (t≈0.12s→0.6s, overlapping the tail of Beat A)**
- The cube's core flashes brighter, then a light trail begins **drawing
  itself** from the projectile's position outward along its actual path of
  travel (not a pre-baked direction — it must track wherever
  `InitializePathMovement`'s points actually send the projectile, including
  Serpveil's Curve/S-Curve/Hook/Spiral/Boomerang presets).
- Trail head (comet) stays bright; the tail lingers slightly (~0.3–0.4s)
  behind the current position before fading, rather than popping out
  instantly — this is the same "the light line should still be there" note
  from the in-chat mockup review.
- This is functionally the same technique as Beat B in
  `Lacern_Kogetsu_VFX_Spec.md` §7 (moving-point-history Ribbon), except the
  "tip marker" here is the projectile itself, not a hand-tracked scene
  component.

**Beat C — Hit (deferred, not this pass)**
- Already covered by planned Serpveil Hit Impact work (next item after this
  one in the roadmap) — do not build it as part of this doc's scope.

---

## 3. Color & style

Reuses the already-locked Vael palette, same as Lacern's spec — do not invent
new colors:
- Core / cube faces, hottest point: `#f2f5ff` (white-hot).
- Mid glow / cube shading: `#7fe9ff`.
- Outer glow / trail body: `#27d8ff` (the canon Vael cyan).
- No red/orange/black — reserved for Black Trigger territory, same ban as
  Lacern's spec §4.

---

## 4. Why "clean split" language matters even though N=1 here

The cross-weapon design (`energy-cube-launch-vfx-decision` memory) explicitly
rejected an earlier "crack and shatter into debris fragments" version of this
mockup in favor of a **clean split** — every resulting piece stays a whole
small cube, no jagged debris. Even though Serpveil never shows the split
itself (N=1), the *materialize* beat (Beat A) should still read as "a solid
cube condensing," not "a burst forming out of chaos," so that when Hound/
Astria/Meteo are built later and genuinely split one cube into several, the
single-cube version doesn't look like a different visual language. Concretely:
build Beat A as a clean scale/fade-in of one solid shape, not a particle-burst
noise cloud.

---

## 5. Existing hook points — no new C++ needed

`OnSerpveilFired(EWTBRSerpveilShape Shape)` already exists
(`WTBRSerpveilTrigger.h:48`) but fires on the **Trigger UObject**, which per
the project's replication rules cannot be assumed to reach clients — it is
not the hook this doc uses.

Decision (see conversation that produced `energy-cube-launch-vfx-decision`):
**do not add a `Multicast_SerpveilFired` hook.** Instead, attach the entire
Beat A + Beat B Niagara System directly to the projectile actor
(`Params.SerpveilProjectileClass`, a Blueprint subclass of
`AWTBRProjectileBase`) as a component with its own internal timing (System
Age driving Beat A→B), added in that projectile Blueprint's Editor viewport.
Because the projectile actor's spawn is already a proven remote-safe,
replicated event (Serpveil path projectile movement already verified
Host/Client per `lacern-slash-vfx-decision`'s sibling work), attaching cosmetic
components to it at spawn requires **zero new C++/RPC work** — this is purely
a Blueprint/Niagara-side task for the human implementer.

`AWTBRProjectileBase` currently has no Niagara/mesh component at all
(checked `WTBRProjectileBase.h` — only movement-related members) — the
Niagara System Component itself is new, added at the Blueprint level, not a
new C++ property.

---

## 6a. Implementation walkthrough (for Human, in Editor)

Target Blueprint: **`Content/Blueprints/Triggers/BP_SerpveilProjectile.uasset`**
(confirmed to exist — this is the class `Params.SerpveilProjectileClass`
points to). Do this in the UE5.1.1 Editor; no AI edits to this `.uasset`.

1. Create a new Niagara System, e.g.
   `/Game/VFX/Templates/NS_Base_EnergyCubeLaunch` (matches the
   `NS_Base_SlashTrail` / `NS_Base_HitBurst` naming already planned in
   `Plugins/NiagaraJsonGenerator/README.md`).
2. **Beat A emitter (materialize, 0–0.15s):** a short-lived Sprite or Mesh
   emitter, one burst spawn at System Age 0, scale curve 0→1 over the beat,
   opacity curve 1→0 fading out by the end of the beat. Use §3's colors —
   `#f2f5ff` core, `#7fe9ff`/`#27d8ff` outer — as three stacked
   particles/materials or one gradient material, matching §4's "clean solid
   shape" note (no noise/turbulence modules, keep it crisp).
3. **Beat B emitter (launch trail):** reuse the same moving-point-history
   Ribbon technique as `Lacern_Kogetsu_VFX_Spec.md` §7 — a continuous
   low-cost Spawn Rate emitter recording the System's current world position
   every tick (zero particle velocity, they just mark where the system has
   been), Ribbon Renderer connecting them by spawn order, UV0 Mode = Age,
   material gradient `#f2f5ff` at age 0 → `#27d8ff` fading to alpha 0 by
   ~100% age. Gate this emitter to start a little into Beat A (~0.1s) via
   `Scratch Pad`/System Age condition so the trail doesn't start before the
   cube has visibly formed.
4. Add a **Niagara System Component** to `BP_SerpveilProjectile`'s
   Components panel, assign `NS_Base_EnergyCubeLaunch`, Auto Activate = true.
   Because the whole System's timing is internal (System Age), no Blueprint
   event graph wiring is needed on `BP_SerpveilProjectile` at all — the
   component just plays on spawn, and since spawn already happens at the
   `hand_r` socket (per §6, resolved), Beat A will visually form at the hand.
5. Verify in Editor viewport first (drop one instance, scrub System Age)
   before testing in PIE — cheaper iteration loop than relaunching PIE each
   tweak.
6. 2-player PIE check once it looks right in isolation: 1 cube per shot, no
   duplicate (Host should not see two overlapping systems — this is a single
   attached component on one replicated actor, so duplication risk here is
   much lower than the Character-side multicast pattern, but still confirm).

---

## 6. Open questions before lock

1. ~~Does Beat A's materialize point need to visually originate from the
   character's hand/muzzle first and then "hand off" to the projectile~~ —
   **RESOLVED 2026-07-11**: the actual projectile spawn origin itself was
   moved from eye-height to the `hand_r` socket (commit `e09fa8d`,
   `GetMuzzleLocation()` in `WTBRGunnerTrigger.cpp` covering Solux/Fulgrix/
   Venyx, plus a matching change in `WTBRSerpveilTrigger.cpp::ExecuteServerFire`),
   verified in 2-player PIE for all four triggers. Section 5's "attach to
   projectile at spawn" approach is now correct as originally written — no
   cosmetic hand→eye bridge is needed, because the projectile itself already
   spawns at the hand. Fallback to the old eye-based origin only fires if the
   `hand_r` socket lookup fails (logged as a warning), which was not observed
   during PIE testing.
2. When Hound/Astria/Meteo are built, the shared "one cube splits into N"
   moment happens once per shot but produces N separate projectile actors —
   that shared conjure point is **not** covered by this doc's
   attach-to-projectile approach and will need its own design pass (likely a
   short-lived, non-gameplay-authoritative cosmetic actor or Character-side
   one-shot, still to be decided). Out of scope here; flagged for later.
3. Confirm target weapon order after this prototype: Hound next (exercises
   the N-cube fan-out, matches the reference footage most directly) or
   Astria/Meteo first (simpler, no homing lock complicating the "which cube
   hit what" question)?
