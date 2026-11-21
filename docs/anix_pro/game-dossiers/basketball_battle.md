# ANIX PRO Game Dossier — Basketball Battle

Baseline audit: 2026-11-21. Status: **in progress; not completion-certified**. This file records source-level evidence and the work required before this game can be accepted as release-ready.

## Current implementation

- Module: `lib/games/basketball_battle/`
- Existing tests: `test/games/basketball_battle/basket_core_test.dart`, `test/games/basketball_battle/basket_widget_test.dart`, `test/games/arcade/basket_test.dart`
- Observed scope: Shot simulation, moving hoops, streak/scoring, layout, and up to four seats in metadata.

## Open visual and technical work

- **Art / UX:** Review trajectory feedback, rim/swish scoring cues, timer visibility, and safe-area layouts.
- **Rules / physics / AI:** Audit the rules and edge cases named below; current test presence alone does not prove completeness.
- **Online / rooms:** Audit deterministic input/seed behavior, simultaneous scoring fairness, time expiry, reconnect, and score validation.
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

