# WTBR Black Trigger — Gameplay Design (all 5 types) — Rev 1.1 LOCKED

Project: WTBR / Vaelborne: Dominion
Status: **DESIGN LOCKED (Rev 1.1, 2026-07-15)** — owner-approved. Ventryx +
Fulgorn locked earlier (with Codex); Solvarn/Kaldrix/Nyxveil from the GDD-locked
designs (GDD v6.3 §12), adapted to the WTBR LMB/RMB/R input convention. Balance
numbers are ⚠ playtest-pending unless the GDD marks them "Lock". Next step =
implementation per §8 build order (Kaldrix vertical slice first).

Rev 1.1 tightening fixes (owner review 2026-07-15, all applied): (1) Fulgorn LMB
hold = zoom→precision only, Shield-pierce consolidated onto R Black Art; (2)
Solvarn LMB tap=hip-fire / hold=scope (accuracy only, no charge/damage), Last
Stand post-interrupt grace 1.5s→0.2-0.3s; (3) Kaldrix Disintegration blanket
"-50% effectiveness" decomposed into 4 named per-axis DA values (Accuracy axis =
`SpreadMultiplier = 1.25`); (4) Nyxveil Shadow Bind "pierce Shield" = status-
through-shield-but-no-damage, Invisibility = hides from radar AND visual with
defined tells, Action Ping fires exactly once per shot / per Vael-release event.

---

## 0. Canon ↔ IP name map

| WTBR IP name | Canon (GDD) | Role | Class stub (exists) |
|---|---|---|---|
| **Ventryx** | Fujin — Wind Sword | Zoning / Area Control | `UWTBRVentryxTrigger` |
| **Fulgorn** | Artemis — Bow | Main DPS (long range) | `UWTBRFulgornTrigger` |
| **Solvarn** | Suiren — Sniper | Sub DPS + Debuff (tanky) | `UWTBRSolvarnTrigger` |
| **Kaldrix** | Kazan — Volcano | Debuff + Team Support | `UWTBRKaldrixTrigger` |
| **Nyxveil** | Yomi — Shadow | Assassin (single-timing) | `UWTBRNyxveilTrigger` |

⚠ **CODE-vs-GDD MISMATCH — Codex must rewrite, not extend, these stubs.** The
existing class header comments describe placeholder behavior that does NOT match
the GDD-locked designs:
- `WTBRSolvarnTrigger` stub says "Defense Field (projectile-destroying sphere)"
  — WRONG. Suiren is a tanky sniper (see §3).
- `WTBRNyxveilTrigger` stub says "Scan (radar ping)" — WRONG. Yomi is an
  assassin (see §5).
- `WTBRKaldrixTrigger` stub says "Explosion Zone" — partial; Kazan is a
  debuff/support kit, not just a blast (see §4).

---

## 1. Shared Black Trigger rules (GDD-locked, apply to all 5)

- Exactly **1 Black Trigger type spawns per match** (random of the 5).
- Item healing **reduced to 30%** while holding a Black Trigger.
- **Memory Loadout**: dropping/unequipping restores the previous normal loadout
  instantly.
- **Drop**: a Black Trigger drops as a contestable **black cube** on death.
- **Warning**: appears 1-2 minutes before its Capture Point spawns.
- The **R** ("signature") button is remappable per the custom key-binding system.

## 1a. Input convention (shared)

- **LMB = Main action, RMB = Sub action, R = the Black Trigger's signature/ultimate.**
- **R semantics rule (owner decision 2026-07-15):** R is always the signature
  move. Whether it is `R tap` (instant) or `R hold → release` (channeled /
  telegraphed) is chosen by the ability's nature — do NOT force all 5 to the
  same gesture. Channeled/telegraphed ults use hold→release for a readable
  commit window; instant escape/engage tools use tap. Per-type gesture is noted
  in each section.

---

## 2. Ventryx = Fujin — Wind Sword (Zoning / Area Control) — OWNER-LOCKED

| Button | Behavior |
|---|---|
| **LMB** | Melee Fujin slash (close range). |
| **RMB tap** | Send a wind-slash ribbon crawling along surfaces. |
| **RMB hold → release** | Same ribbon, but pin its far end in place as a **Trap**. |
| **R** | Command an existing ribbon/Trap to fire. On press, scan for enemies near the ribbon's END: enemy in radius → the blade launches from the tip toward the target; no enemy → the blade fires straight along the ribbon's direction. A **Trap** self-fires by the SAME rule when an enemy enters its radius. |

Notes for implementation: reuse a Surface-Crawling ribbon path (GDD "Surface-
Crawling Pathfinder"). Ribbon quota / regen / detonate hitbox numbers per GDD
§12.1 (11 ribbons, 60 dmg/ribbon, 4s/ribbon regen, capsule 150u×400u,
truncated at static geometry). ⚠ Stack-decay curve playtest-pending.

---

## 3. Fulgorn = Artemis — Bow (Main DPS, long range) — OWNER-LOCKED

| Button | Behavior |
|---|---|
| **LMB tap** | Normal arrow. |
| **LMB hold → release** | **Zoom → precision shot.** Hold enters a sniper zoom (aim only); release fires one accurate arrow. **No charge tier, no pierce scaling, no Shield-pierce here** — hold only tightens aim, it does NOT grow damage. (Revised at owner lock 2026-07-15: the old 0-3s charge / full-charge one-shot / Shield-pierce was removed from LMB and consolidated onto R so the power ladder is unambiguous.) |
| **RMB** | Rain-of-arrows AoE from the sky, with a warning ring (flushes enemies out of cover). Shield reduces its damage 50%. |
| **R hold → release** | **Black Art (Ultimate):** sweep-lock 2-3 targets (GDD allows up to 5), release fires **Shield-piercing homing** arrows — one lock per target, no reacquire. **This is the ONLY Fulgorn move that pierces Shield** — the whole "pierce Shield" identity lives here now, not on LMB. |

Ultimate homing rules (GDD §12.2): Limited Turn Rate (right-angle dodge
possible), 5s expiry, breaks after 1.5s LoS loss, Forced Interrupt before
release cancels everything. ⚠ Forced-interrupt cooldown playtest-pending.

---

## 4. Solvarn = Suiren — Sniper (Sub DPS + Debuff, tanky) — PROPOSED

Identity: a durable "raid boss" that applies sustained pressure — NOT a burst
sniper like Fulgorn. Rewards holding an angle and staying alive.

| Button | Behavior |
|---|---|
| **Passive: Steadfast Aim** | HP Max **+50%** (→450 total), **20% DR** at all times, no movement penalty while scoped. |
| **LMB tap** | **Piercing Round (hip-fire)** — fire immediately from the hip, wider spread / lower accuracy. Same hitscan, same damage, chips Shield HP **35%**, range 0-8000u with falloff. |
| **LMB hold → release** | **Piercing Round (scoped)** — hold to scope in, release to fire. Hold **only increases accuracy** (tighter spread) — it does **NOT** add damage or a charge tier. Same shot, just precise. |
| **RMB tap** | **Suppressing Mark** — tag 1 enemy (range 6000u); temporarily lowers their damage and accuracy. |
| **R tap** | **Last Stand (Ultimate)** — root 3s, DR rises to **50%**. Interrupt cancels it, and the 50% DR lingers only a short **~0.2-0.3s grace** (revised down from 1.5s at owner lock 2026-07-15 — a 1.5s post-interrupt window removed too much counterplay; 0.2-0.3s just prevents a one-frame interrupt from feeling unfair). `R tap` (not hold) because it's a defensive stance the user wants up instantly under fire. |

Piercing-round falloff (GDD §12.3): 0-3000u full, 3000-6000u -30%, 6000-8000u
-60%, 8000u+ = 0. ⚠ Base damage + mark duration/penalty% playtest-pending.

---

## 5. Kaldrix = Kazan — Volcano (Debuff + Team Support) — PROPOSED

Identity: lowest damage of the five, but weakens the entire enemy team at once.
A team-support disruptor, not a killer.

| Button | Behavior |
|---|---|
| **Passive: Entropy Field** | 500u aura that follows the user; enemies inside get **-5~10% speed & vision**. |
| **LMB tap** | **Discord Wave** — 600u expanding ring, **20-30 dmg** + **Stagger** to all hit. |
| **RMB tap** | **Magma Shard** — fire 3 shards at once (**25-30 dmg each**, range 1500u), manually aimed, no homing. Immune to Kazan's own debuff (fixed damage). |
| **R hold → release** | **Disintegration (Ultimate)** — hold ~2s (can walk, not rooted) → deploy a 2000u following field for **8s**. Kazan gains **+30% speed**; enemies inside get the debuff SET below (NOT a single blanket "-50%"). `R hold→release` because it's a telegraphed team-wide commit. |

**Disintegration debuff set (replaces GDD's blanket "-50% effectiveness" — each axis is its own DA-driven, individually-tunable value; ⚠ all playtest-pending):**

| Axis | Debuff | Note |
|---|---|---|
| Damage dealt | **-25%** | enemy outgoing damage |
| Movement speed | **-20%** | multiplicative, stacks with Entropy Field's -5~10% |
| Projectile / ability speed | **-20%** | their shots/dashes are slower, easier to dodge |
| Accuracy (aim spread) | **+25% spread** | reticle bloom / recoil widened, ≈ -20% effective accuracy. Define in code as `SpreadMultiplier = 1.25` (a multiplier on the enemy's base spread cone), NOT a subtractive "accuracy %". |

These four are the concrete decomposition — do NOT implement a single "-50%"
multiplier. Balance each axis independently in playtest. The four together are
intentionally strong (it's the ultimate of a support kit whose own damage is the
lowest of the five), but each stays readable and separately tunable.

Synergy gimmick (GDD §12.4): an enemy caught by BOTH Entropy Field +
Disintegration turns the smoke bright red (max-weakened). 1.5s grace after
leaving the 2000u radius. Knockback/launch (Grasshopper/Escudo) cancels the
Disintegration channel. ⚠ Magma Shard damage playtest-pending.

---

## 6. Nyxveil = Yomi — Shadow (Assassin, single-timing) — PROPOSED

Identity: not a great escaper — an assassin who picks exact entry/exit timing
and eats the consequence of a bad entry.

| Button | Behavior |
|---|---|
| **LMB tap** | **Shadow Strike** — close melee. From behind while invisible = **+100% backstab damage**. |
| **RMB tap** | **Phantom Step** — very fast dash. **No i-frame** (deliberate GDD nerf). 2s cooldown. |
| **RMB hold → release** | **Shadow Bind** — mark 1 enemy (range 500u), **Slow 20% for 3s**. **"Pierces Shield Trion" means: the mark + slow STATUS applies even if the target is behind a Shield — the debuff is not blocked. It does NOT deal or pass any damage through the Shield** (Shield still blocks all damage normally). Bind is a pure status, so there's no damage to block; the point is the status can't be shield-walled. Forced-interrupt cancels. |
| **R tap** | **Invisibility (Ultimate)** — 3-4s, limited **2-3 uses/match**. **Hides from BOTH radar and visual**: no radar/Pulse-Scan ping AND the model is not rendered to enemies. Two intentional tells only: (1) firing / any Trion-leaves-hitbox attack raises **exactly ONE Action Ping per shot / per Vael-release-out-of-body event** — the ping fires once at the moment Trion leaves the body, NOT once per projectile or per tick, so a single multi-cube/multi-hit shot still pings only once (prevents a rapid ping-spam that would over-reveal the assassin); it briefly reveals; (2) ⚠ *optional, playtest-decide* — a faint visual shimmer within very close range (~300u) so a point-blank standing enemy isn't a guaranteed free kill. `R tap` (NOT hold) — an assassin's engage/escape must be instant and not telegraphed by a hold wind-up. This is the one intentional exception to "big ults use hold→release." |

Core combo (GDD §12.5): Shadow Bind (slow) → Phantom Step (close the gap) →
Shadow Strike (backstab +100%). ⚠ Phantom Step's no-i-frame is playtest-flagged
(may be too weak vs multiple enemies).

---

## 7. Engineering notes (shared components to reuse — GDD §16)

- Fujin ribbon: `Surface-Crawling Pathfinder`, `Sequential Cooldown Queue`,
  `Stack Damage Decay Calculator`, `Capsule Hitbox Truncation`.
- Artemis ult + (optionally) Ventryx tip-launch: `Homing Projectile
  (Limited Turn + Expiry + LoS)`.
- Kazan: `Proximity Debuff Field`, `Kazan Visual Synergy Controller`.
- All 5 share: the black-cube drop/contest actor, Memory Loadout swap, the
  30%-item-heal modifier, and the per-match random-type selector.

## 8. Build order suggestion (once Codex locks the design)

1. Shared Black Trigger base (`UWTBRBlackTrigger`): random-type-per-match,
   memory loadout, 30% heal modifier, black-cube drop/contest, R-remap binding.
2. Rewrite the 3 mismatched stubs (Solvarn/Nyxveil/Kaldrix) to the roles above
   — do NOT keep the old defense-sphere / radar-scan / blast-only behavior.
3. Implement one vertical slice first (recommend **Kaldrix/Kazan** — it exercises
   the Proximity Debuff Field shared component + passive aura + a channeled ult,
   a good template) then fan out to the rest.
4. Automation per type (fire/cost/cooldown/passive), then PIE gate for the
   networked/feel parts (invis visibility, homing, debuff fields).

## Hard rules that still apply

DA-driven balance values (⚠ playtest), server-authoritative, no AI edits to
.uasset/Blueprint/UMG. All damage/duration/radius numbers above must live on a
`FWTBR<Type>Params` DataAsset struct, not hardcoded.
