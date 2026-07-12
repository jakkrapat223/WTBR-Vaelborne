# Serpveil — Conjure-Cube Telegraph VFX Spec v0.2

Status: DRAFT — beat structure **locked by design owner 2026-07-12**
(mockup review, in-chat): FORM static → SPLIT static minis → LAUNCH.
No bob, no rotation, anywhere. Remaining opens are tuning numbers only (§8).
Written against baseline `b37db6d` (S1 tap volley).

Scope: **visual only** — restyle the S1 windup telegraph from the legacy
`NS_Serpveil_ChargeGlow` into the conjure-cube language. Zero gameplay/C++
changes; the replication bridge this rides on already exists and is verified.

Parent specs:
- `Docs/Design/Serpveil_TapHold_CubeSplit_Spec.md` §5 (S1 conjure asset) and
  §7 — the "1 big cube → N small cubes" split-beat visual that §7 deferred is
  **pulled forward into this spec** (owner decision 2026-07-12): the windup
  telegraph now shows the split happening, cosmetically, before the real
  volley exists.
- `Docs/VFX/Serpveil_EnergyCubeLaunch_VFX_Spec.md` §3/§4 (palette +
  clean-split language).

---

## 1. What exists (all verified in code/git — do not rebuild)

| Piece | State |
|---|---|
| `bSerpveilChargeTelegraphActive` (RepNotify) + `OnSerpveilChargeTelegraphChanged(bool)` | `WTBRCharacter.h:467-476` — server-authoritative, reaches ALL clients |
| Flag ON at windup start, OFF at fire **and every cancel/abort path** | `WTBRSerpveilTrigger.cpp` (WindupStarted/WindupCanceled/VolleyAborted/VolleyComplete all set it) — VFX side needs no cancel handling |
| BP wiring: `BP_WTBRCharacter` event → drives `NS_Serpveil_ChargeGlow` | bound in `fdd4cf2`, verified state saved `ebf5ddb` |
| Projectile spawn origin = `hand_r` socket | `e09fa8d` — telegraph cubes and the volley originate from the same point |
| Real volley formation | S1 (`b37db6d`): saw-tooth pattern, DA-driven. **Tuned in PIE 2026-07-13**: count 5, spacing 15 uu, stagger 40 uu, windup (`SerpveilSplitDelay`) 0.3 s |
| Per-projectile launch VFX (`NS_Base_EnergyCubeLaunch` Beat A+B) | attached to `BP_SerpveilProjectile`, plays automatically per split cube |

**The only production changes:** (a) a new Niagara template + generated
variant, (b) swapping the asset reference inside `BP_WTBRCharacter`'s existing
telegraph handler.

---

## 2. Timeline & beats (locked 2026-07-12)

```
telegraph ON (t=0)   t=FormTime    t=SplitTime       windup end (DA-driven 0.3–0.5s) = telegraph OFF
|--- FORM ------------|-- big cube --|--- HOLD minis (static, loops) ---|-- VANISH ≤0.08s --|
     scale-in 0→1        static           N small cubes, static            real volley spawns
                                                                            same frame, same socket
```

- **FORM (0 → `FormTime` ≈ 0.12s):** one solid cube scales/fades in at the
  `hand_r` socket. Clean single shape — no fragments, no noise cloud.
- **SPLIT (t = `SplitTime` ≈ 0.18s):** the big cube vanishes with a small
  brightness pop and `CubeCount` mini cubes appear in its place, arranged in
  the same vertical pattern as the real volley (`MiniSpacing` matches the
  DA's `SerpveilFormationSpacing`). **This split is cosmetic** — the real
  projectiles do not exist until windup end; the minis are a preview of the
  volley, which is exactly what makes the telegraph honest and readable.
- **HOLD (SplitTime → telegraph OFF):** the minis hover **completely static**
  — no bob, no rotation, no drift (owner-locked). A subtle emissive pulse
  (`PulseAmount`, ~±10% brightness, zero position change) keeps them reading
  as live energy instead of a frozen screenshot. **Must loop indefinitely** —
  `SerpveilSplitDelay` is DA-tunable, do not author to an exact 0.4s.
- **VANISH (telegraph OFF):** all minis die fast (≤0.08s) as the real volley
  spawns at the same socket in the same formation on the same frame — the
  cosmetic minis appear to *become* the projectiles.
- Cancel paths need nothing extra: same VANISH plays, no volley appears.
- Timing sanity: with the PIE-tuned 0.3s windup and `SplitTime` 0.18, the
  minis are on screen only ~0.12s. The formation read ("cubes = volley
  incoming") is instant so this works, but if the split beat feels invisible
  at match pace, the knob is lowering `SplitTime` toward ~0.12–0.15 via JSON
  re-import (big cube shorter, minis longer) — not lengthening the windup.

---

## 3. Color & readability

Locked Vael palette only (identical to launch + Lacern specs, no new colors):
core `#f2f5ff` → mid `#7fe9ff` → outer glow `#27d8ff`. No red/orange/black.

Readability target: this is an **enemy-facing warning**, not an owner
flourish. Big cube edge ~15 uu (= 15 cm, softball-sized against a 180 cm
character) + additive glow halo ~2× cube size; minis ~8 uu each. Exact
numbers are template user params → tuned by JSON re-import + PIE eyeball
(open Q2).

---

## 4. Technical approach — template + JSON variant

Per `NiagaraJsonGenerator` v1 scope locks (template-only, no graph edits from
automation):

1. **Human, once:** build `/Game/VFX/Templates/NS_Template_ConjureCube` (§5)
   with the user params below *wired* (a param that drives nothing violates
   the dangling-param rule).
2. **Claude:** generate `/Game/VFX/Serpveil/NS_Serpveil_ConjureCube` via
   `WTBR.Niagara.ImportJson` from
   `Plugins/NiagaraJsonGenerator/Examples/VFX/NS_Serpveil_ConjureCube.json`;
   iterate values by re-import (update-in-place).
3. Future multi-cube weapons (Venyx/Solux/Fulgrix) reuse the same template
   with their own JSON — this is the shared conjure point flagged in launch
   spec §6-Q2, now including the split preview.

| User param | Type | Drives | Serpveil v1 value |
|---|---|---|---|
| `Color` | color | glow halo + cube outer tint | `[0.153, 0.847, 1.0, 1.0]` (#27d8ff) |
| `CubeSize` | float | big cube mesh scale (uu) | `15.0` |
| `FormTime` | float | FORM scale-in duration (s) | `0.12` |
| `SplitTime` | float | big cube dies / minis appear (s from activate) | `0.18` |
| `CubeCount` | int | mini cube count — **must match DA `SerpveilCubeSplitCount`** | `5` |
| `MiniCubeSize` | float | mini cube mesh scale (uu) | `8.0` |
| `MiniSpacing` | float | vertical gap between minis (uu) — match DA `SerpveilFormationSpacing` | `15.0` |
| `GlowSize` | float | halo sprite size on the big cube (uu) | `30.0` |
| `PulseAmount` | float | HOLD emissive pulse strength (0 = off; never position) | `0.1` |

Keep the param list exactly this — every one wired. If the DA's cube count or
formation spacing changes later, the fix is a JSON value + re-import, not a
template edit.

---

## 5. Template build walkthrough (human, in Editor, ~20 min)

Contract the template must satisfy (internals flexible):
**Activate → big cube forms over `FormTime`, at `SplitTime` it swaps to
`CubeCount` static minis that hold forever, Deactivate → gone ≤0.08s.**

1. `Content/VFX/Templates` → new Niagara System (empty or minimal emitter) →
   `NS_Template_ConjureCube`.
2. **Emitter 1 — CubeCore (big cube):** Spawn Burst Instantaneous ×1,
   **Lifetime = `SplitTime`** (the particle dying IS the split moment),
   Emitter State Loop = `Once`. **Mesh Renderer** with the engine cube mesh +
   `M_EnergyCube` (reuse if it reads as a solid energy cube; otherwise a
   simple emissive gradient per §3). Scale-in: Scale Mesh Size curve indexed
   by `Age / FormTime` clamped 0–1, 0 → `CubeSize`. Optional: brief emissive
   spike over the last ~0.03s of life to sell the split pop. **No orientation
   /rotation/velocity modules — the cube sits perfectly still.**
3. **Emitter 2 — MiniCubes:** Spawn Burst Instantaneous, count = `CubeCount`,
   **burst delay = `SplitTime`**; Lifetime long (e.g. `60.0` — windup is
   ≤0.5s, they just need to outlive any realistic hold), Loop = `Once`. Same
   mesh/material at `MiniCubeSize`. Position: vertical line formation —
   Z offset = `(ParticleIndex - (CubeCount-1)/2) * MiniSpacing` (Scratch Pad
   or Location module). Quick scale-in ~0.05s so they pop cleanly out of the
   split. HOLD pulse: emissive × `1 + PulseAmount * sin(...)` — **emissive
   only, zero position/rotation change** (owner-locked: static hover).
4. **Emitter 3 — GlowHalo:** Spawn Burst ×1, additive soft-glow sprite
   (`M_Trion_Emissive` is already in `Content/VFX/Materials`), Sprite Size =
   `GlowSize`, Color = `Color`, Lifetime = `SplitTime` (halo belongs to the
   big cube; minis carry their own smaller glow via material emissive).
5. **Vanish behavior:** System State → **Inactive Response = `Kill`** so
   `Deactivate()` removes everything within a frame. If the 1-frame pop looks
   harsh in PIE, switch to `Complete` + ~0.08s fade — decide by eye.
6. Add the 9 User Parameters (§4 names, exact spelling, no `User.` prefix
   when typing) and link each into the modules above.
7. Sanity-check in the template preview: change each user param → visual
   reacts (especially `CubeCount` 3 vs 5 and `SplitTime`); restore defaults;
   save.

## 6. Wiring change (human, in Editor, ~2 min)

In `BP_WTBRCharacter`'s existing `OnSerpveilChargeTelegraphChanged` handler,
repoint the Niagara asset from `NS_Serpveil_ChargeGlow` to
`NS_Serpveil_ConjureCube`. While there, confirm:
- attachment socket is `hand_r` (same as projectile spawn origin),
- the OFF path calls **Deactivate** (not Destroy Component / not nothing) so
  §5.5's vanish behavior is what actually runs,
- Auto Activate is off (the event drives activation).

No other Blueprint or C++ change. `NS_Serpveil_ChargeGlow` stays in place
until the swap passes §7, then gets **deleted** together with any remaining
references in `BP_WTBRSerpveilTrigger_VFX` (owner approved disposal
2026-07-12) — as its own cleanup commit, same pattern as the Lacern
debug/prototype cleanup.

---

## 7. Verification (2-player PIE, after Editor-viewport pass)

- [ ] Windup: both host and client see FORM → SPLIT → static minis at the
      *other* character's hand for the full windup; owner sees their own.
- [ ] Mini count matches DA `SerpveilCubeSplitCount` (currently 5) and the
      formation visually matches where the real volley then spawns.
- [ ] Fire: minis vanish exactly as the volley appears — one
      continuous "minis became projectiles" read, no gap, no lingering cubes.
- [ ] Cancel: slot switch / unequip / death during windup → everything
      vanishes, no volley, no orphaned effect.
- [ ] Blocked shot (cooldown / insufficient Vael): **no cube ever appears**
      (flag already guarantees this — confirm visually).
- [ ] No duplicates on host (single RepNotify-driven component, low risk —
      still confirm).
- [ ] Rapid re-fire after cooldown: telegraph re-activates cleanly from FORM
      (component reuse across Activate/Deactivate cycles — burst-delay
      emitters must re-arm on reactivation; if minis fail to reappear on the
      2nd shot, reset the component instead of reusing it).

## 8. Open questions before lock

1. ~~HOLD motion~~ — **RESOLVED 2026-07-12**: static hover in every phase.
   No bob, no rotation. `PulseAmount` (emissive only) is the sole life cue.
2. `CubeSize`/`MiniCubeSize`/`GlowSize`/`SplitTime` numbers after PIE
   readability check at 15–30 m (starting: 15 / 8 / 30 uu, split at 0.18s).
3. ~~Retire `NS_Serpveil_ChargeGlow`?~~ — **RESOLVED 2026-07-12**: delete
   after §7 passes, disposal delegated to implementer (see §6).
4. Owner's first-person-ish view: cubes at `hand_r` sit near screen center
   when aiming — acceptable (it's ≤0.5s), or dim/hide for the owning client?
   Default: keep identical for all viewers (server-authoritative fairness).
