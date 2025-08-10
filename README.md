## ui-afterhours

### Build

Prereqs: raylib available via `pkg-config` (macOS: `brew install raylib`), a C++ compiler, Node.js for the test runner.

```sh
make build
```

### Run all tests (actions-based UI snapshots)

Tests live under `actions/<scenario>/` as a pair of files:
- One `.toml` with input actions playback
- One `.json` with the expected UI tree subset

Optional per-scenario `meta.json` can be present for tagging/coverage; it is ignored by the runner when locating the expected `.json` file.

Use the Node script to run all scenarios and validate output:

```sh
node scripts/run_actions.js
```

Filter by substring (directory name contains the filter):

```sh
node scripts/run_actions.js single_button
```

Optional environment variables:
- `UI_POS_TOL` (float): position/size tolerance when matching rects (default `0.5`)
- `REQUIRE_COVERAGE` ("1" to fail the run if coverage requirements aren’t met)

### Run a single scenario manually

You can run the app directly with a specific actions file. The app produces `ui_tree.json` in the repo root which you can inspect or compare.

```sh
./ui.exe --actions=actions/single_button/single_button.toml --no-window --delay=125
```

Flags:
- `--actions=</absolute/or/relative/path/to>.toml`: load playback actions
- `--no-window` (alias: `--headless`): run with a hidden window
- `--delay=<ms>`: add a delay between playback steps (default `0`)

You can also provide the actions file via env var:

```sh
AH_ACTIONS=actions/single_button/single_button.toml ./ui.exe --no-window
```

### Button variant coverage and generator

We track coverage for button variants across three dimensions: label presence, color usage, and disabled state. Scenarios under `actions/button_*` are auto-generated to cover:
- hasLabel: `true`, `false`
- color: `Primary`, `Secondary`, `Accent`, `Error`, `Background`
- disabled: `true`, `false`

Generate scenarios:

```sh
node scripts/generate_button_variants.js
```

Run only button scenarios and see coverage:

```sh
node scripts/run_actions.js button_
```

If any combinations are missing, the runner prints them. Set `REQUIRE_COVERAGE=1` to make missing coverage fail the run.

### Parametrizing demos via TOML

Some demos can be parameterized via extra tables in the actions TOML. For the button demo, use a `[button]` table:

```toml
[button]
hasLabel = true
color = "Primary"    # Primary | Secondary | Accent | Error | Background
disabled = false
```

These parameters control the rendered button in the example overlay during the test run.
