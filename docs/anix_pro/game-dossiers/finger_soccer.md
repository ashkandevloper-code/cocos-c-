# ANIX PRO Game Dossier — Finger Soccer

Baseline audit: 2026-11-21. Status: **in progress; not completion-certified**. This file records source-level evidence and the work required before this game can be accepted as release-ready.

## Current implementation

- Module: `lib/games/finger_soccer/`
- Existing tests: `test/games/finger_soccer*_test.dart`, `test_render/finger_soccer*_test.dart`
- Observed scope: Physics engine, Painter UI, setup/shop, bot, and online seats. Local match setup is 1v1; online room support is 2, 4, or 6 active seats. The server migration and room validation now enforce even team sizes.

## Open visual and technical work

- **Art / UX:** Large custom pitch/disc set and separate team art. Review asset licensing/consistency, collision alignment, layout snapshots, and rendered states across sizes.
- **Rules / physics / AI:** Audit the rules and edge cases named below; current test presence alone does not prove completeness.
- **Online / rooms:** Review server-side move legality, deterministic snapshot reconciliation, reconnect behavior, and result disputes; online consensus currently validates agreement, not full physics replay.
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

