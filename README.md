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

Boards use the original random mine placement. The mine count is exact; if the
first opened cell contains a mine, that mine moves to a randomly chosen safe
cell. The first opening is always safe, but may be a number, and later moves
may require guessing. Mine counts are clamped to 1 through board area minus 1.

```sh
./minesweeper 20 20 200
```

## print a board and exit

`--show` prints the complete board as plain text and exits without changing
terminal settings. It also works with redirected stdin/stdout. `*` is a mine,
`.` is a zero, and `1`–`8` are clues.

```sh
./minesweeper 20 20 200 --show --first 11,11 --seed 0
./minesweeper 20 20 120 --show --seed 1 > board.txt
```

`--first ROW,COLUMN` is 1-based. With `--show`, it defaults to the center
(`11,11` for 20x20). In interactive play, it positions the initial cursor;
the cell you actually open receives first-click protection. `--seed` accepts
an unsigned 32-bit integer. Using the same dimensions, mine count, seed and
first opening reproduces the initial board on the same platform, including
between `--show` and interactive play. Restart creates a new random board.
Elapsed time in the output is not expected to be reproducible.

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
