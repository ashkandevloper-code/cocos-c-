# ANIX PRO Game Dossier — Obstacle Race

Baseline audit: 2026-11-21. Status: **in progress; not completion-certified**. This file records source-level evidence and the work required before this game can be accepted as release-ready.

## Current implementation

- Module: `lib/games/obstacle_race/`
- Existing tests: `test/games/obstacle_race/obstacle_core_test.dart`, `test/games/obstacle_race/obstacle_layout_test.dart`
- Observed scope: Runner simulation, obstacle layouts, ranking, and 2–4 seat support.

## Open visual and technical work

- **Art / UX:** Audit hazard contrast, safe margins, scrolling motion, reduced-motion behavior, and finish/result states.
- **Rules / physics / AI:** Audit the rules and edge cases named below; current test presence alone does not prove completeness.
- **Online / rooms:** Validate deterministic obstacle seeds, ranking/DNF/forfeit, hitboxes, reconnect, and server checks against fabricated placements.
- **Ranking / progression:** Verify that the settled server result is the only source used for ranked stats, profile history, achievements, and rewards. Document any unsupported seasonal/friends/regional views.
- **Performance / lifecycle:** Measure on CI/device when suitable runners exist; review disposal, resume, and reduced-motion paths.

## Completion plan

1. Reproduce and document the current behaviors and known edge cases from the listed core/UI modules.
2. Add focused rule and UI coverage for the named gaps without removing existing tests.
3. Complete visual review and asset checks across Persian/English, RTL/LTR, low-motion, and compact screen sizes.
4. Validate online room limits, reconnection, result settlement, ranking, and persistence against an isolated backend test environment.
5. Accept only after the relevant GitHub Actions quality, game, backend, and release gates pass.

## Evidence and remaining limitations

No local build or test was run. GitHub Actions has not validated this dossier or its game yet. Any functionality not evidenced by the module and test inventory above remains unverified; external services, missing test infrastructure, and unresolved bugs must be recorded before release.

