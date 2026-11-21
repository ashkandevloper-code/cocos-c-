#!/usr/bin/env python3
"""Validate the checked-in ANIX PRO game scope before Flutter analysis.

This is intentionally static: it does not build the app, contact a backend, or
claim that a registered game is production-complete. It catches catalog drift,
duplicate IDs, missing entry modules, and missing dossier/test evidence.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGISTRIES = [ROOT / "lib/games/game_registry.dart", ROOT / "lib/games/arcade_registry.dart"]
DOSSIER_DIR = ROOT / "docs/anix_pro/game-dossiers"
TEST_DIR = ROOT / "test/games"

EXPECTED = {
    "finger_soccer": "finger_soccer/finger_soccer_entry.dart",
    "carrom": "carrom/carrom_entry.dart",
    "billiards": "billiards/billiards_entry.dart",
    "backgammon": "backgammon/backgammon_entry.dart",
    "ludo": "ludo/ludo_entry.dart",
    "snakes_ladders": "snakes_ladders/snakes_ladders_entry.dart",
    "uno": "uno/uno_entry.dart",
    "quiz": "quiz/quiz_entry.dart",
    "mafia": "mafia/mafia_entry.dart",
    "kart_racing": "kart_racing/kart_racing_entry.dart",
    "obstacle_race": "obstacle_race/obstacle_race_entry.dart",
    "bomb_arena": "bomb_arena/bomb_arena_entry.dart",
    "pool8": "pool8/pool8_entry.dart",
    "mini_golf": "mini_golf/mini_golf_entry.dart",
    "basketball_battle": "basketball_battle/basketball_battle_entry.dart",
    "spy_mafia": "spy_mafia/spy_mafia_entry.dart",
}


def fail(message: str) -> None:
    print(f"::error file=tool/validate_anix_scope.py::{message}")
    raise SystemExit(1)


def main() -> int:
    missing = [str(p.relative_to(ROOT)) for p in REGISTRIES if not p.is_file()]
    if missing:
        fail("missing registry file(s): " + ", ".join(missing))

    source = "\n".join(p.read_text(encoding="utf-8") for p in REGISTRIES)
    ids = re.findall(r"\bid:\s*'([^']+)'", source)
    duplicates = sorted({game_id for game_id in ids if ids.count(game_id) > 1})
    if duplicates:
        fail("duplicate registered game id(s): " + ", ".join(duplicates))

    expected_ids = set(EXPECTED)
    actual_ids = set(ids)
    if actual_ids != expected_ids:
        fail(
            "registry scope drift; missing={} unexpected={}".format(
                sorted(expected_ids - actual_ids), sorted(actual_ids - expected_ids)
            )
        )

    for game_id, entry in EXPECTED.items():
        entry_path = ROOT / "lib/games" / entry
        if not entry_path.is_file():
            fail(f"{game_id}: missing entry module {entry_path.relative_to(ROOT)}")
        dossier = DOSSIER_DIR / f"{game_id}.md"
        if not dossier.is_file():
            fail(f"{game_id}: missing implementation dossier {dossier.relative_to(ROOT)}")

    if not TEST_DIR.is_dir() or not any(TEST_DIR.rglob("*.dart")):
        fail("no game test evidence found under test/games")

    print(
        "ANIX scope validation passed: {} registered games, entry modules present, "
        "dossiers present, and game test evidence found.".format(len(ids))
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
