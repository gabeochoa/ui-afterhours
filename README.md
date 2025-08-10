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
