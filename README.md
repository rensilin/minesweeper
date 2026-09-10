# cpp-minesweeper-on-shell

![minesweeper v2.0 terminal demo](minesweeper.gif)

## install

	git clone https://github.com/kkkeQAQ/minesweeper --recursive
	cd minesweeper
	make
	sudo make install

## completion      //test

	sudo make completion

## uninstall
	
	sudo make uninstall

or

	sudo rm /usr/bin/minesweeper

## update

	git pull
	git submodule update --init --recursive
	sudo make install

## more

	minesweeper --help

## board generation

Both modes use one generator when you first open a cell. The mine count is
exact, and that cell and all its neighbors are safe, so the first clue is zero.
The default `--mode=no-guess` checks and repairs the board until it can be
solved without guessing, within the generation budget. Use `--mode=random`
to return the initial shuffled board immediately without that check.

```sh
./minesweeper 20 20 100
./minesweeper 20 20 200 --mode=random
```

Both modes reject unsupported mine counts. At least four cells must remain
safe for a corner opening, six for an edge, and nine for an interior opening.
An interactive click with insufficient space shows the supported range and
lets you choose another cell.

### no-guess mode

```sh
./minesweeper 20 20 100 --mode=no-guess
./minesweeper 20 20 100 --mode=no-guess --show --first 11,11 --seed 0
```

The board is generated when you first open a cell. That cell and all its
neighbors are safe, so the first clue is zero. Every returned board has been
solved from that opening using logical deductions. The solver uses visible
clues, subset differences and the remaining mine count; it has no NG2/NG3 or
difficulty restriction. A solver that gets stuck causes another generation
attempt, without claiming that the candidate is logically impossible to solve.

The generator shuffles a complete board, relocates mines near a stalled clue,
and progressively redraws the last few mine placements when repairs do not
succeed. Every edit is checked again from the first opening. Your chosen mine
count is preserved, and ordinary random mode remains available explicitly.

Generation is bounded by 512 board checks, 3000 milliseconds and an internal
work limit. Change the first two with `--max-attempts COUNT` and
`--generation-timeout MILLISECONDS`; both require positive integers and work
in the default no-guess mode. The first limit reached ends the attempt. Dense
boards may exhaust the budget. In interactive play, Space retries, `r` restarts and `q`
quits; while generation is running, `r` cancels and `q` quits. `--show` reports
failure on stderr and exits unsuccessfully without printing an unchecked board.

See [the generation algorithm](docs/no-guess.md) for the shared generator,
solver and retry policy.

## print a board and exit

`--show` prints the complete board as plain text and exits without changing
terminal settings. It also works with redirected stdin/stdout. `*` is a mine,
`.` is a zero, and `1`–`8` are clues.

```sh
./minesweeper 20 20 100 --show --first 11,11 --seed 0
./minesweeper 20 20 120 --mode=random --show --seed 1 > board.txt
```

`--first ROW,COLUMN` is 1-based. With `--show`, it defaults to the center
(`11,11` for 20x20). In interactive play, it positions the initial cursor;
the cell you actually open receives first-click protection. `--seed` accepts
an unsigned 32-bit integer. Using the same dimensions, mine count, seed and
first opening reproduces the initial board on the same platform, including
between `--show` and interactive play. Restart creates a new random board.
Random-mode seeds now use the shared shuffle and produce different layouts
from versions that used C `rand()` and first-click mine relocation.
Elapsed time in the output is not expected to be reproducible.
No-guess mode also requires the same generation limits; the wall-clock limit
can stop generation sooner on a slower machine. `--show` reports its mode and
number of board checks in the header when no-guess generation succeeds.

## tests

```sh
make test
```

Tests check mine counts, first-click safety, printed clues, seeded reproduction
and plain output through the main executable. Python 3 PTY tests exercise
presets, keyboard and mouse controls, wins/losses, panel alignment, restart,
resize, incremental rendering and terminal restoration under `LC_ALL=C` and
`NO_COLOR=1`.

## terminal support

UTF-8 terminals use continuous box-drawing borders, and interactive terminals use color. Non-UTF-8 locales fall back to ASCII automatically. Set `NO_COLOR=1` to keep the TUI monochrome.

Move with WASD or the arrow keys. Press `j` or `f` to flag a cell, Space to sweep, `r` to restart, and `q` to quit.

In terminals supporting SGR mouse reporting, left-click a cell to sweep and
right-click to toggle its flag. All three character columns of a cell are clickable.
Clicking an opened number uses the same chord behavior as Space. When the board
is clipped, left-click its top/bottom/left/right border to pan one row/column
in that direction where more cells exist; corner clicks can pan both axes.
Border clicks never open cells. Mouse release, drag, wheel and help-panel clicks
are ignored. The help panel labels these actions LMB (left) and RMB (right).
After a win or loss, left-click `[y]` to start a new game or `[q]` to quit.
Mouse tracking and terminal settings are restored on normal exit, Ctrl-C, and
SIGTERM; keyboard controls remain available.
The protocol follows the [xterm SGR mouse specification](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Extended-coordinates).

The side panel shows both the logical board `size` and the terminal-limited `view`, as rows by columns. Command-line dimensions are limited to 9-100 by default; pass `--no-max-size` to allow values above 100 while keeping the 9x9 minimum. The 100-cell limit never controls the viewport, which is always clipped to the current terminal dimensions.

If the full board does not fit, the TUI keeps the help panel visible and shows a scrolling, cursor-following board viewport with `warning:clipped`. Alternating `^`, `v`, `<`, and `>` border marks indicate hidden cells in each direction. Resizing only redraws this viewport: it never restarts the game, and the warning disappears whenever the complete board fits. Terminals need at least 19 rows and enough columns for a 9-column viewport plus the help panel (normally 51 columns).
