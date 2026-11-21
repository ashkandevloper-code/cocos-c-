# ANIX PRO Game Ecosystem Audit

Audit baseline: 2026-11-21. This document records the game modules registered in the Flutter app and the evidence present in source, tests, and assets. A registered game is not, by itself, evidence that every product requirement is complete. No local build or test was run for this audit.

## Registry Inventory

The active registry combines nine core entries from `lib/games/game_registry.dart` with seven arcade entries from `lib/games/arcade_registry.dart`.

| Game | Source module | Existing test evidence | Asset directory | Initial assessment |
| --- | --- | --- | --- | --- |
| Finger Soccer | `lib/games/finger_soccer/` | `test/games/finger_soccer*_test.dart`, `test_render/finger_soccer*_test.dart` | `assets/games/finger_soccer/`, `assets/games/bermuda/` | Flagship with substantial physics, setup, rendering, and online coverage. Online room seats now have an explicit 2–6 bound; local setup remains 1v1. |
| Carrom | `lib/games/carrom/` | `test/games/carrom_test.dart` | `assets/games/carrom/` | Dedicated physics, match, bot, controller, and painter modules; core is two-seat. Room limits are being aligned in the games database migration. Needs deeper rules, online replay, and visual review. |
| Billiards | `lib/games/billiards/` | `test/games/billiards*_test.dart` | `assets/games/billiards/` | Dedicated physics, aim, match, bot, painter, and HUD modules; needs deeper rules and visual review. |
| Backgammon | `lib/games/backgammon/` | `test/games/backgammon_test.dart` | `assets/games/backgammon/` | Dedicated rules, bot, geometry, and skin modules; needs full legal-move and online state review. |
| Ludo | `lib/games/ludo/` | `test/games/ludo_test.dart` | `assets/games/ludo_battle/` | Rules, bot, die, and board modules with unit coverage; verify asset naming and persisted/online state. |
| Snakes & Ladders | `lib/games/snakes_ladders/` | `test/games/snakes_ladders_test.dart` | No matching game-specific directory found in the declared asset inventory | Rules and board modules with deterministic tests; visual asset inventory needs review. |
| NX Cards (UNO-style) | `lib/games/uno/` | `test/games/uno_test.dart` | No matching game-specific directory found in the declared asset inventory | Engine, bot, card, and overlay modules with unit tests; verify hidden-information handling online. |
| ANIX Quiz | `lib/games/quiz/` | `test/games/quiz*_test.dart` | No matching game-specific directory found in the declared asset inventory | Question bank, engine, and UI with unit/widget tests; server-authoritative answer validation needs review. |
| Mafia | `lib/games/mafia/` | `test/games/mafia_test.dart` | No matching game-specific directory found in the declared asset inventory | Engine, bots, narrator, and online views exist; review role secrecy, room lifecycle, and online integration. |
| Kart Racing | `lib/games/kart_racing/` | `test/games/kart_racing/`, `test/games/arcade/kart_test.dart` | `assets/games/kart_racing/` | Dedicated simulation and layout tests; needs visual and multiplayer authority review. |
| Obstacle Race | `lib/games/obstacle_race/` | `test/games/obstacle_race/` | `assets/games/obstacle_race/` | Dedicated core and layout tests; needs visual and multiplayer authority review. |
| Bomb Arena | `lib/games/bomb_arena/` | `test/games/bomb_arena/` | `assets/games/bomb_arena/` | Dedicated core and layout tests; needs deterministic network simulation and visual review. |
| 8 Ball Pool | `lib/games/pool8/` | `test/games/arcade/pool8_golf_test.dart` | `assets/games/pool8/` | Dedicated core and UI modules; tests share a file with Mini Golf, so coverage should be separated and audited. |
| Mini Golf | `lib/games/mini_golf/` | `test/games/mini_golf/`, `test/games/arcade/pool8_golf_test.dart` | `assets/games/mini_golf/` | Dedicated physics, course, and scorecard tests; needs visual and online result review. |
| Basketball Battle | `lib/games/basketball_battle/` | `test/games/basketball_battle/`, `test/games/arcade/basket_test.dart` | `assets/games/basketball_battle/` | Dedicated simulation and widget tests; verify scoring validation and online timing. |
| Spy & Mafia | `lib/games/spy_mafia/` | `test/games/arcade/spy_test.dart` | `assets/games/spy_mafia/` | Dedicated role game and tests; core and player palette support 4–8 players. Server bounds are being aligned in the games database migration. |

## Requested Games Not Registered

The following examples from the requested roadmap have no matching active game registration in either registry at this audit baseline: Water Sort, Hokm, Yatzy, Domino, Bingo, Crazy Dots, Line & Disc, Penguin / Ice Shot, Parkour, Night Adventure, Road Escape, Two Drivers, Offroad, Orbit, Block, Ball Rolling, Color Catch, Brick Breaker, Slam Dunk, Chicken Crossing, Jump, Fruit Slice, Tower, Knife Hit, Puzzle Twist, Bubble Shooter, Submarine Battle, Commander, Chess Blitz, Checkers Arena, Connect Four, Tic-Tac-Toe Royale, Domino Clash, Word Duel, Draw & Guess, Memory Match, Battleship, Air Hockey, Table Tennis Duel, Dodgeball Arena, Rhythm Clash, Tower Defense Co-op, and Daily Puzzle Lab. The additional IDs `stickman_arena`, `ludo_battle`, `endless_runner`, `tetris_rush`, `helix_jump`, `fruit_ninja`, `geo_dash`, `traffic_racer`, `stack_tower`, `bubble_shooter`, `knife_hit`, and `game_2048` also occur in metadata, icon, or score-limit definitions but do not have registered playable game modules.

Asset folders, icon specs, score-limit rows, or reference images do not count as playable implementations. Requested games should be added only with a complete product scope and should not be represented as available until registered and validated. The games-bound migration stops room APIs from accepting stale/unregistered game IDs.

## Cross-Cutting Findings

- Game metadata now distinguishes local match seats from online seats. Talk-room capacity for active players derives from online bounds; spectators remain room members and are independent of game-seat capacity.
- Previous server bounds allowed Carrom six seats, Quiz 21, and Spy & Mafia 12 despite smaller game implementations. The new migration aligns these to active game cores; apply it to the Supabase test and production projects before claiming deployment.
- There is no `integration_test/` directory at this baseline. Existing game tests are unit and widget tests; they do not establish emulator-level navigation, reconnect, multiplayer, backend, or crash coverage.
- The current archive contains reusable quality, unit/widget, game-logic, game-integration, backend, security, Android-release, and full-release workflows. The older `.github/workflows/check.yml` quick-check description is stale and is not part of this archive. The quality gate now also validates registry IDs, entry modules, implementation dossiers, and game-test evidence before analysis.
- The Android workflow has release validation and artifact upload. Runtime backend endpoints and publishable Supabase anonymous keys are passed through GitHub Actions secrets; they are client-visible credentials rather than service-role secrets, but they must remain protected repository/environment configuration and must never be replaced with privileged keys.
- Supabase migration files include game rooms, matchmaking, tournaments, turn authority, and result integrity. The presence of migrations does not prove that every game has server-validated outcomes or reconnect semantics; each game/server contract needs a separate review.

## Delivery Status

This is a baseline inventory, not a completion certificate. No game is marked complete by this audit. Follow-up work should create implementation dossiers for each registered game, starting with contract and CI correctness, then game-specific rules/physics, visual review, online authority, and integration coverage. Any missing external prerequisites (GitHub secrets, backend test project, or service-side configuration) must be recorded against the affected delivery item.
