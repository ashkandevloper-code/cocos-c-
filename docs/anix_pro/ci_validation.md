# ANIX PRO CI validation

All executable validation is intended to run in GitHub Actions. The reusable workflows are coordinated by `full_release_validation.yml`; each can also be started with `workflow_dispatch` for diagnosis.

| Workflow | What it gates |
| --- | --- |
| `flutter_quality_gate.yml` | Flutter 3.50.0 stable analysis with locked dependency resolution. |
| `flutter_unit_widget_tests.yml` | Core, feature, and general app tests outside the game-specific suites. |
| `game_logic_tests.yml` | All existing tests under `test/games`, including game rules, bot/AI, physics, and game-module widget/layout coverage. |
| `game_integration_tests.yml` | The existing app navigation smoke test and Finger Soccer render/intro screenshot tests. |
| `backend_validation.yml` | Backend checks only when an isolated test database/service can be provisioned; see that workflow's job summary for configured coverage. |
| `security_audit.yml` | OSV dependency checks and committed-secret scanning. |
| `android_release_build.yml` | Calls the repository Android build workflow after release validation; it is restricted to `main`. |
| `full_release_validation.yml` | Runs the required validation workflows and starts the Android release build only after all required checks succeed. |

The repository currently has no `integration_test/` directory, no Android emulator integration suite, and no provisioned Supabase test project or CI credentials documented in the repository. The game-integration workflow therefore runs the existing Flutter widget-level journeys and screenshot tests; it does not label those as device, backend, multiplayer-network, reconnection, or crash tests. Those checks must be added only alongside a safe isolated test environment. The checked-in migrations are source artifacts; database migration execution is not claimed unless the backend workflow provisions and tears down its own test database.

Analyzer, test, scanner, and build commands fail their jobs on failure. Artifacts include command logs or scanner reports where supported. A green workflow is limited to the test scopes listed above; it is not a claim that every game and external service has been exhaustively validated.
