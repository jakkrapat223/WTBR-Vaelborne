# WTBR Original Composite Bullets — Gameplay Design v1

Project: WTBR / Vaelborne: Dominion  
Status: Proposed design baseline; playtest tuning pending  
Scope: Five non-canon Gunner composite bullets  

## 1. Design goals

Each composite must preserve a readable trait from both source bullets while
creating a new decision, not merely a larger damage number.

- Solux / Asteroid: straight, fast, reliable, aim-rewarding.
- Fulgrix / Meteor: explosive pressure and area denial.
- Venyx / Hound: target pursuit with a one-lock, no-reacquire rule.
- Serpveil / Viper: authored waypoint paths with sharp turns.
- Composite damage values are a **total cast budget**, not damage copied to
  every spawned core or child projectile.
- A strong composite needs a visible merge telegraph and at least one clear
  answer: dodge timing, cover, route reading, or interrupting the merge.

## 2. Identity summary

| Composite | Formula | Combat role | Signature mechanic |
|---|---|---|---|
| **Solgrix** | Solux + Fulgrix | Precision breach | Straight penetrator creates a forward-shaped blast on impact |
| **Solhunt** | Solux + Venyx | Finisher / interceptor | Fast straight launch transitions once into limited terminal pursuit |
| **Coilvyn** | Serpveil + Venyx | Ambush / flank pressure | Follows a preset path, then homes from its final release node |
| **Catarix** | Fulgrix + Fulgrix | Siege / displacement | Same impact point detonates twice with different radii and timing |
| **Labyrn** | Serpveil + Serpveil | Pincer / route denial | Two simultaneous mirrored waypoint paths converge from opposite sides |

## 3. Solgrix — Breach Lance

### Role

A high-skill precision explosive. Solgrix rewards a direct hit more than firing
Meteor into the floor near a target.

### Fire sequence

1. Merge creates one dense core and fires it in a perfectly straight line.
2. On character, shield, or world impact, the core deals kinetic impact damage.
3. It then projects a short **forward cone blast** along its incoming travel
   direction. The blast begins at the impact point and extends behind it.
4. A world hit still creates the cone, allowing deliberate corner and doorway
   pressure, but does not gain the direct-hit portion.

### Damage model and counterplay

- Initial target: kinetic damage plus shaped-blast damage.
- Other targets in the cone: shaped-blast damage only, with distance falloff.
- Targets beside or behind the impact point take no blast damage.
- The projectile has no homing and no mid-flight correction; sidestep and hard
  cover remain reliable answers.

### Initial tuning range

- Total direct-hit budget: **95–110** damage.
- Suggested split: 55–65 kinetic + 40–45 blast.
- Cone length: 500–700 uu; half-angle: 18–25 degrees.
- Speed: 3,200–3,600 uu/s.
- Merge time: 0.65–0.85 s.

The directional explosion is the defining mechanic. Solgrix should not be
implemented as a normal spherical Meteor with extra direct-hit damage.

## 4. Solhunt — Pursuit Bolt

### Role

A single-target interceptor that keeps Solux's fast, readable opening and gains
only one Hound-like pursuit phase. It is more decisive than Venyx, but much less
forgiving if the target breaks the intercept.

### Fire sequence

1. At merge completion, acquire the closest valid enemy inside a narrow aim
   cone. This lock is permanent for the cast; there is no reacquire.
2. Fire one bolt straight for a short boost phase.
3. After the boost delay, enable limited homing for the terminal phase.
4. Homing ends after its duration or when the bolt enters the terminal cutoff
   distance. The current heading is then locked and the bolt dashes straight.
5. If no target was acquired, the bolt remains a normal straight shot.

### Damage model and counterplay

- One hit deals the full cast damage; it does not split into a tracking swarm.
- Late lateral movement can defeat the final straight dash.
- Breaking line of sight does not retarget the projectile. Hard cover destroys
  it normally.
- Its turn rate must be visibly weaker than Venyx/Acervyn so that Solhunt does
  not replace the dedicated homing weapons.

### Initial tuning range

- Damage: **80–95**.
- Speed: 3,600–4,200 uu/s; terminal dash multiplier: 1.20–1.35x.
- Boost delay: 0.12–0.20 s.
- Homing window: 0.40–0.65 s.
- Aim cone: 8–12 degrees; no-reacquire.
- Merge time: 0.55–0.75 s.

## 5. Coilvyn — Coil Ambush

### Role

A planned off-angle attack. Serpveil decides **where the shot approaches from**;
Venyx decides **how it closes the final gap**.

### Fire sequence

1. Lock one valid target at merge completion. No target means the projectiles
   still complete their authored route and continue straight from the exit.
2. Spawn three small bolts in a short stagger.
3. All bolts travel through the selected Serpveil preset with 3–4 sharp turns.
   Small formation offsets keep them readable without creating three unrelated
   routes.
4. At the final waypoint, each bolt enters a short homing phase toward the
   original target.
5. A blocked, expired, or missed bolt is spent; it never selects another target.

### Damage model and counterplay

- Total cast budget is divided evenly between the three bolts.
- Suggested total: **75–90** damage (25–30 each).
- Cover placed near the route exit is especially effective.
- The waypoint trail and the release node must be visible to both teams; the
  defender should be able to read which side the attack will emerge from.
- Homing begins only after the last waypoint. Enabling it early would erase the
  Serpveil half of the weapon.

### Initial tuning range

- Bolts: 3; stagger: 0.06–0.10 s.
- Path speed: 2,600–3,000 uu/s.
- Terminal homing: 0.45–0.70 s at 4,500–5,500 acceleration.
- Maximum turns: 4.
- Merge time: 0.75–0.95 s.

## 6. Catarix — Twin Cataclysm

### Role

A slow siege round that forces a second movement decision. Its power is area
control over time, not an unavoidable one-frame double hit.

### Fire sequence

1. Fire one large, slow, straight explosive core.
2. On impact, create the **Break blast**: a smaller, higher-damage inner
   explosion.
3. Leave a bright unstable core at the impact point for a short, fixed delay.
4. The core then creates the **Aftershock blast**: a wider, lower-damage
   explosion with strong edge falloff.
5. The second blast occurs even if the first impact hits world geometry. It is
   canceled only by explicit anti-projectile/anti-trigger rules, not by the
   original projectile actor being destroyed during its first explosion.

### Damage model and counterplay

- Inner Break radius: 180–250 uu, **50–60** damage.
- Aftershock radius: 400–550 uu, **45–60** damage at center with falloff.
- Maximum center total: **105–120** damage.
- Aftershock delay: 0.35–0.55 s.
- Speed: 2,000–2,400 uu/s; merge time: 0.9–1.2 s.
- The delay and radius must be clearly telegraphed. A player caught by the first
  blast should normally have time to leave the second radius.

Catarix must not apply two full Meteor explosions on the same frame. That has
poor readability and turns the composite into a pure damage multiplier.

## 7. Labyrn — Mirror Labyrinth

### Role

A pincer projectile that attacks conventional side-dodging. The locked
mechanic is **dual mirror path**; no homing is added.

### Fire sequence

1. Build Path A from the selected Serpveil preset.
2. Reflect every lateral waypoint offset to build Path B.
3. Launch both groups simultaneously. Path A passes one side of cover while
   Path B passes the opposite side.
4. Both paths turn inward at their final segment and cross/converge at the
   aimed endpoint.
5. After the endpoint, surviving bolts travel straight for a short cleanup
   distance; they do not home or reacquire.

### Projectile and damage rules

- Recommended formation: 3 bolts on Path A + 3 bolts on Path B.
- Total cast budget: **90–110** damage, divided across all six bolts
  (15–18.3 each).
- Mirrored groups from the same owner never clash with each other.
- Carrier/core actors, if used to spawn the six visible bolts, deal **zero**
  contact damage. Only the six final attack bolts consume the damage budget.
- Maximum 4 turns per path. Both groups use the same speed and reach the
  convergence segment within 0.10 s of each other.

### Counterplay

- Forward/back movement escapes the closing pincer better than side-dodging.
- Central hard cover can block both final segments.
- Both routes must be telegraphed during merge; a hidden second path would make
  the hit feel arbitrary.

### Initial tuning range

- Speed: 2,600–2,900 uu/s.
- Path width from centerline: 350–650 uu.
- Straight cleanup distance: 300–500 uu.
- Merge time: 0.85–1.05 s.

## 8. Shared targeting and networking rules

- Target acquisition, projectile spawning, path construction, explosions, and
  damage are server-authoritative.
- Target selection is snapshotted once at merge completion. Solhunt and Coilvyn
  never reacquire after a miss, block, death, or invalid target.
- A child projectile may damage a given actor at most once.
- Merge interruption retains the current non-refundable Vael rule unless the
  wider composite system changes that rule for every composite.
- Replicated state should carry the chosen target and deterministic path/fuse
  inputs; clients render telegraphs and VFX from that state.

## 9. DataAsset requirements

The existing class/cost/time/damage maps can remain the common lookup, but they
are not enough to describe these mechanics. Add per-composite parameter structs
or one instanced composite behavior config containing at least:

- projectile count, total damage budget, speed, range;
- acquisition cone, homing duration/acceleration, no-reacquire flag;
- path preset, turn cap, lateral width, stagger, terminal segment distance;
- explosion shape/radii, damage split, falloff, and aftershock delay;
- projectile class and VFX/telegraph hooks.

`CompositeDamages` should explicitly mean **total cast damage budget**. Spawn
code must divide it before initializing children; it must never pass the full
map value into every child.

## 10. Current implementation gap

At the time of this design:

- `FireComposite` has a Labyrn-specific branch, while the other requested types
  fall through to one generic straight, non-explosive, non-homing projectile.
- The current Labyrn branch initializes two damaging cores with the full mapped
  damage and also calls `SpawnCubeSplits()` for each core. That does not match
  the shared total-budget rule above and risks multiplying cast damage.
- `SpawnCubeSplits()` derives its fan direction from `ProjectileMovement`
  velocity, but Labyrn deactivates that component when path movement starts.
  Labyrn should spawn path-following child bolts directly (or pass an explicit
  split direction/path), rather than relying on the generic clash split helper.

These are implementation notes only; code and DataAssets should be changed in a
separate implementation pass after the gameplay identities are approved.

## 11. Acceptance criteria for the first playtest

- A tester can identify each composite from its flight behavior without seeing
  its name.
- Every composite exposes its intended counterplay before damage lands.
- No cast exceeds its configured total damage budget when every child hits one
  target.
- Solhunt and Coilvyn never reacquire.
- Coilvyn does not home before its final waypoint.
- Catarix's two blasts are separated by a visible dodge window.
- Labyrn uses two geometrically mirrored paths and synchronizes their final
  convergence within 0.10 s.
