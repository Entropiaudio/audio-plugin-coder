# Plan: Mod-matrix polarity (BI/UNI) — B60

**For:** implementation session (Opus). **State:** plan only, nothing implemented.
**Repo:** worktree `claude/eager-carson-9a852f`, plugin `plugins/Entropan/`. Current build stamp **B59** (bump to **B60**).

## Design (agreed)

A route's polarity follows its **source band's existing BI/UNI toggle** (`b*_uni`). No new params, no per-route toggle.

| Source band | Route source value `s` | Destination motion | Depth clamp (total ≤ 100 % rule) |
|---|---|---|---|
| **BI** (uni=false) | raw post-slew `m.value` ∈ −1..+1 | swings **± depth around** the knob | `|depth| ≤ min(pos, 1−pos)·100` — neither rail clips |
| **UNI** (uni=true) | `(m.value+1)/2` ∈ 0..+1 | pushes **one-way from** the knob (sign of depth = direction) | unchanged B57 clamp: `+depth ≤ (1−pos)·100`, `−depth ≥ −pos·100` |

Ring on the destination shows the live FINAL value under the same math. Toggling BI/UNI re-clamps that band's routes.

## DSP — `Source/PluginProcessor.cpp`

**1. Matrix source transform.** In the mod-matrix block at the top of `processBlock` (search `"── mod matrix"`), the accumulation currently reads:
```cpp
acc[rt.dst] += mods[…rt.src…].value * rt.depth * 0.01f;
```
The band's uni flag is NOT yet read at this point (it's read later in the gather loop via `pp.uni`). Read it directly here:
```cpp
const bool srcUni = bandParams[(size_t) src].uni->load() > 0.5f;
const float s = srcUni ? (mods[src].value + 1.0f) * 0.5f : mods[src].value;
acc[rt.dst] += s * rt.depth * 0.01f;
```
(`src` = clamped `rt.src` as now.) Nothing else in the DSP changes — the normalized-domain apply + clamp already handles the rest.

**2. Telemetry.** `modSrcVal[i]` currently stores raw `m.value` (set in the per-block telemetry loop, search `modSrcVal`). Keep it RAW — the UI applies the uni transform itself (it knows `P.bands[i].uni`); this keeps one source of truth for the transform per side and the scope/other consumers unaffected.

## UI — `Source/ui/public/index.html`

**3. Source helper.** Next to `modSrcVals` (search `const modSrcVals`), add:
```js
const routeSrcVal = r => P.bands[r.src].uni.get() ? (modSrcVals[r.src] + 1) / 2 : modSrcVals[r.src];
```

**4. Ring** (`drawModRings`): replace `modSrcVals[r.src]` with `routeSrcVal(r)`. Formula stays `fin = clamp(base + routeSrcVal(r)*r.depth/100, 0, 1)`. Optional polish (do it if cheap): before the live head, stroke the REACHABLE span faintly — BI: `base−|d|..base+|d|`; UNI: `base..base+d` (or `base+d..base` for negative) — instead of the current full-270° faint track. Keeps the ring honest about where the value can go.

**5. Headroom** (`destHeadroom(dst)` — search it): needs the SOURCE's polarity now, so change signature to `destHeadroom(dst, src)`:
```js
const destHeadroom = (dst, src) => {
  const n = destNorm(dst);
  if (P.bands[src].uni.get()) return { min: -n * 100, max: (1 - n) * 100 };   // one-way (B57 rule)
  const m = Math.min(n, 1 - n) * 100;                                          // bipolar: fit both sides
  return { min: -m, max: m };
};
```
Callers (two): the dial drag handler in the `modEditor mousedown` listener, and the default-depth clamp in the drop `mouseup` handler — both already have the route (`rt.dst`, `rt.src` / `routeDrag.src`).

**6. Re-clamp on polarity flip.** In the `polToggle` onclick (search `polToggle`), after `PB().uni.set(...)`, re-clamp every route whose `src === ui.selected`:
```js
modRoutes.forEach(r => { if (r.src === ui.selected) { const l = destHeadroom(r.dst, r.src); r.depth = clamp(r.depth, l.min, l.max); } });
pushRoutes();
```
(`pushRoutes` persists + refreshes; keep the existing `refreshPanels()` call or let pushRoutes cover it.)

**7. Browser sim parity.** The sim writes raw bipolar `b.mod` into `modSrcVals` (search `modSrcVals[i] = b.mod`) — correct as-is since the transform now lives in `routeSrcVal`. No change.

**8. Tooltip.** In `meDraw`, append the polarity so users see why the range differs: `(P.bands[meRoute.src].uni.get() ? " · UNI" : " · BI")` into `modEditor.dataset.tip`.

## Tests — `Source/tests/NullTest.cpp`

**T25 (new): UNI route is one-sided.** Copy T24's shape (`runOut` lambda: B1 sine, lift 0, route src 0 → dst 63 OUTPUT). Set `b1_uni = 1`, `b1_rate` fast-ish (e.g. 2 Hz), depth +100, and measure **per-block** L-channel energy min/max (like T7's balance extremes, but energy vs the dry baseline). Assert: max ≈ +12 dB region (mod peaks push level up) and **min never drops below −0.5 dB** (UNI never goes below the knob's own 0 dB). Then same with `b1_uni = 0` (BI): min must drop well below (mod's negative half pulls level down; assert min < −6 dB). Two runs, one check each.

**T24 must stay green** (its band is BI by default and pinned at +1 → same +12 dB).

## Gate + ship (same sequence as every build)

1. `perl -pi -e 's/ENTROPAN_BUILD = "B59"/ENTROPAN_BUILD = "B60"/' plugins/Entropan/Source/ui/public/index.html`
2. Build `Entropan_NullTest`, run — **25/25** required.
3. Browser verify on `http://localhost:8932` (preview config "Entropan UI"; `window.__entropan` exports P/ui/refreshPanels):
   - BI band, knob 50 %, drop route → dial cannot exceed ±50 (drag far both ways).
   - Knob at 100 %, BI → dial pinned to 0; flip band to UNI → dial reaches −100/0.
   - UNI + sim mod pinned (`ui.bands[0].mod = ui.bands[0].target = 1`): ring head at base+depth; pinned −1 → head AT base (not below).
   - Flip BI→UNI with an oversized route → depth visibly re-clamps.
4. Build VST3 + AU, `rm -rf` + `cp -R` into `~/Library/Audio/Plug-Ins/{VST3,Components}/`, grep binaries for `B60`, `auval -v aumf Etpn Etpa` → PASS.
5. Commit `feat(Entropan): B60 — mod routes honor source BI/UNI polarity` with the what/why and test evidence. Co-author trailer per repo convention.

## Traps (learned the hard way in this codebase)

- **TDZ**: anything called during initial script execution must not touch `let`s declared later (`loadRoutes` bug, B52). New helpers here are only called at runtime — keep it that way.
- Don't call `refreshPanels()` per mousemove — use the existing per-frame loop / `schedulePanels()` (stutter, B57/B59).
- The browser pane throttles rAF; verify with direct calls + pinned sim values, not by waiting frames.
- `destNorm` reads live param values — fine at runtime, do NOT call at parse time.
- Param IDs/undo/persistence need no changes — routes JSON schema is unchanged.
