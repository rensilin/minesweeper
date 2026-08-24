# Repository Guidelines

## Project Structure & Module Organization

`minesweeper.cpp` contains the command-line parser, terminal setup, game state, rendering, and gameplay loop. The `SColor/` and `args/` directories are Git submodules that provide terminal colors and argument parsing; initialize them before building. `completion.sh` defines Bash completion, while the lowercase `makefile` contains build and installation targets. User-facing setup and controls belong in `README.md`. There is currently no dedicated test or asset directory.

## Build, Test, and Development Commands

- `git submodule update --init --recursive` fetches the two header dependencies after cloning.
- `make` compiles the project as C++11 and creates `./minesweeper`.
- `make run` builds and launches an interactive game.
- `./minesweeper --help` or `./minesweeper --version` provides a quick non-interactive smoke check.
- `make clean` removes object files; remove the generated executable separately if needed.

`sudo make install`, `sudo make completion`, and `sudo make uninstall` modify `/usr`; use them only when testing system-wide installation.

## Coding Style & Naming Conventions

Match the existing C++ style: tabs for indentation, opening braces on the next line for functions and control blocks, and C++11-compatible features only. Use lower camel case for functions and variables (`refreshMap`, `mineNum`), an `m` prefix for board-state arrays (`mMine`, `mSight`), and uppercase names for macros (`MAXX`). Treat output as a stateful TUI: normal input should refresh only changed cells and counters; reserve full redraws for startup and terminal resize. Always restore cursor and terminal settings on exit. No formatter or linter is configured, so avoid unrelated whitespace-only changes.

## Testing Guidelines

There is no automated test framework or coverage requirement yet. Every change should compile cleanly with `make`. Smoke-test `--help`, `--version`, preset difficulty flags, and any changed argument handling. For gameplay changes, manually verify movement, flagging, sweeping, restart, win/loss output, and terminal restoration after quitting. Confirm ordinary movement does not clear the screen and `SIGWINCH` does. Add tests under `tests/` if introducing independently testable game logic.

## Commit & Pull Request Guidelines

History favors short subjects such as `add completion` and `change getch() -> getchar()`. Use a concise imperative subject that names the behavior changed; avoid vague messages such as `update`. Pull requests should explain the user-visible effect, list commands and manual scenarios tested, and link relevant issues. Include a terminal capture or screenshot when rendering, colors, or layout changes.
