# Lacern (Kogetsu) — VFX Design Spec v0.1

Status: DRAFT — not locked. Written against code as of baseline `d0800d0`.
Scope: visual design only. No C++/Blueprint hook points added by this doc — see
"Recommended hook points" at the end for what an implementer will need to add.

---

## 1. Identity

Lacern = Kogetsu, the extending straight-blade sword. CQC-range Trigger
(`03-gameplay-triggers.md` §CQC 0–800u). Distinguishing read at a glance: the
blade is NOT a fixed-length sword swing — it telescopes out from the hilt and
snaps back. VFX must sell "extend" and "retract" as two visually distinct
beats, not one generic slash.

Reference mechanics (`WTBRLacernTrigger.cpp`, `FWTBRLacernParams`,
`FWTBRMeleeHitboxParams`):

| Param | Value | Derived timing |
|---|---|---|
| ExtendLength | 400u | — |
| ExtendSpeed | 1200 u/s | Extend phase ≈ **0.333s** |
| RetractSpeed | 1800 u/s | Retract phase ≈ **0.222s** |
| CapsuleRadius / HalfHeight | 30u / 50u | blade+hitbox thickness reference |
| DualWieldLateralOffset | 40u | gap between L/R blades when dual wielding |
| SwingCooldown | 0.5s | post-retract lockout |

Full single-swing cycle: **~0.56s extend+retract**, then 0.5s cooldown before
next Activate is accepted (`IsOnCooldown()` gate).

No "hold at full extension" phase exists in code — `TickExtend` calls
`StartRetract()` the instant `CurrentExtendDist >= ExtendLength`. VFX should
NOT imply a sustained hold at max reach; the snap-back is immediate.

---

## 2. Timeline & VFX beats

```
t=0            t≈0.33s (full extend)         t≈0.56s (fully retracted)   t=1.06s
|--- EXTEND ------------------|--- RETRACT ------------|--- COOLDOWN (0.5s) ---|
```

**Beat A — Extend start (t=0, on `StartExtend`)**
- Small Vael-flare burst at the hilt/hand — energy "forming" the blade edge.
- Duration: 1–2 frames worth of punch, not a lingering glow.

**Beat B — Extending (t=0→0.33s, every `TickExtend`)**
- A **ribbon/trail** from hilt to current blade tip, growing in real time with
  `CurrentExtendDist`. This is the primary read — the trail length should
  visually track distance, not just play a fixed-length swipe.
- Faint motion-blur streak along the sweep arc (reuses `SweepCapsuleFromTo`
  start→end positions already computed for hit detection — same geometry,
  reused for VFX alignment, not recomputed).
- Tip has a slightly brighter core than the shaft (reads as "cutting edge").

**Beat C — Hit (on any entry added to `HitActorsThisSwing`)**
- Impact spark/flash at `FHitResult.ImpactPoint`, oriented to `ImpactNormal`.
- One spark per unique actor hit this swing (dedup already guaranteed by
  `HitActorsThisSwing` — VFX does not need its own dedup logic).
- Keep it short (~0.15s) — swings happen fast and repeated hits in a single
  extend/retract must not visually stack into a blob.

**Beat D — Full extend peak (t≈0.33s)**
- One-frame brighter flash at the tip the instant `CurrentExtendDist` caps at
  `ExtendLength` — sells the "snap taut" moment right before retract begins.

**Beat E — Retract (t≈0.33→0.56s)**
- Trail **shrinks back toward the hilt** (not just fades out) — same ribbon
  asset, reverse-driven by shrinking `CurrentExtendDist`. Retract is 1.5x
  faster than extend (1800 vs 1200 u/s) — the shrink should read as a snappy
  recoil, distinctly faster than the extend growth.
- Trail opacity can taper slightly as it shrinks so it doesn't end in a hard
  pop at `OnRetractComplete`.

**Beat F — Cooldown (t≈0.56→1.06s)**
- No active VFX required. Optional: a very faint hilt idle-glow that reads as
  "not ready" vs the Beat-A flare reads as "ready to swing again" — nice-to-have,
  not required for v0.1.

---

## 3. Dual wield variant

When `bIsDualWieldSwing` is true, `TickExtend` sweeps twice per tick — once at
`RightOffset=0`, once at `RightOffset=DualWieldLateralOffset` (40u). Two
independent blades extend in parallel, offset 40u apart.

- Run Beats A–E **on both blades simultaneously**, same timings (dual wield
  does not change ExtendSpeed/RetractSpeed, only adds the second sweep).
- The two trails should read as parallel, not crossed/X-shaped — Senku's
  planned X-cross visual (`wtbr-project-state` memory, Dual Senku) is a
  **different, not-yet-implemented Trigger option** and must not be reused
  here. Lacern dual wield ≠ Senku dual wield.
- Damage multiplier on dual-wield hits is cosmetic-adjacent: consider a
  slightly hotter/brighter impact spark (Beat C) when `DmgMult > 1.0` so
  players get passive feedback that dual wield hit harder, without needing UI.

---

## 4. Color & style

No locked VFX color convention exists yet in the GDD. Proposing we anchor to
the **already-established Vael-core accent color `#27d8ff`** (cyan) used for
the chest Vael core and belt accent in `vaelborne_character_concept.svg` —
reusing it keeps character art and Trigger VFX visually unified as "the same
energy" rather than inventing a second unrelated palette. Needs confirmation
before treating as locked.

- Base trail/edge: `#27d8ff` cyan, high emissive.
- Impact spark (Beat C): shift warmer/whiter at the core (`~#f2f5ff`, matching
  the concept art's highlight color) with cyan falloff — reads as "impact"
  distinct from the ambient blade color.
- Do NOT use red/orange/black — those read as Black Trigger territory
  (`references/04-black-triggers.md` convention: Black Triggers get their own
  distinct palette, not yet spec'd — do not preempt it here).

---

## 5. Existing hook points (implemented in code — verified against current `WTBRLacernTrigger.h`)

`UWTBRSerpveilTrigger` established the pattern to follow
(`WTBRSerpveilTrigger.h`, `BlueprintImplementableEvent`, category
`"WTBR | Serpveil | VFX"`). `UWTBRLacernTrigger` **already implements the
matching hooks** (added after this doc's `d0800d0` baseline) — no further
C++/header work is needed, only Blueprint-side VFX binding:

- `OnLacernExtendStart(bool bIsDualWield)` — Beat A
- `OnLacernExtending(float CurrentDist, float MaxDist)` — Beat B (drives trail length)
- `OnLacernHit(FVector ImpactPoint, FVector ImpactNormal, bool bDualWieldHit)` — Beat C
- `OnLacernFullExtend()` — Beat D
- `OnLacernRetractComplete()` — Beat F transition

Binding these in `BP_WTBRLacernTrigger` is the remaining implementation task,
not part of this design doc.

---

## 6. Open questions before lock

1. Confirm `#27d8ff` as the canon Trigger-VFX base color (vs. designing a
   separate palette per Trigger family).
2. Confirm whether Beat F's idle hilt-glow is wanted for v0.1 or deferred.
3. GDD §5.4 locks Senku (Kogetsu option) damage numbers but says nothing about
   Kogetsu's own base VFX — confirm no existing external concept art/reference
   this should match before art production starts.
