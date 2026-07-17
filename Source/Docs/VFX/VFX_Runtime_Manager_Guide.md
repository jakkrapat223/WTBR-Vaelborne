# WTBR Runtime VFX Manager

`UWTBRVFXManagerSubsystem` is now the cosmetic runtime path for built-in
projectile impact effects. It runs locally on each replicated client and never
spawns effects on a dedicated server.

`FWTBRProjectileVFXConfig` controls its behavior from a Trigger Data Asset:

- Niagara impact/trail and physical-surface overrides;
- cull distance, pooled Niagara spawn, and opt-in debug point/normal overlay;
- sound, decal, decal lifespan, and local camera shake;
- `ImpactAssetParameters` for template-defined `User.*` object inputs such as
  materials, textures, curves, meshes, and ribbon profiles.

The Sniper auto-bind manifest accepts `assetOverrides` in addition to its
existing `surfaceOverrides`. Each entry contains a compatible Niagara user
parameter and a content asset path. Ensure the Niagara template exposes and
wires that user parameter before binding it.
