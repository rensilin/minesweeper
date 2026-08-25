# cpp-minesweeper-on-shell
![game.gif](https://github.com/kkkeQAQ/minesweeper/blob/markdown/game.gif)
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

## terminal support

UTF-8 terminals use continuous box-drawing borders, and interactive terminals use color. Non-UTF-8 locales fall back to ASCII automatically. Set `NO_COLOR=1` to keep the TUI monochrome.

The side panel shows both the logical board `size` and the terminal-limited `view`, as rows by columns. Command-line dimensions are limited to 9-100 by default; pass `--no-max-size` to allow values above 100 while keeping the 9x9 minimum. The 100-cell limit never controls the viewport, which is always clipped to the current terminal dimensions.

If the full board does not fit, the TUI keeps the help panel visible and shows a scrolling, cursor-following board viewport with `warning:clipped`. Alternating `^`, `v`, `<`, and `>` border marks indicate hidden cells in each direction. Resizing only redraws this viewport: it never restarts the game, and the warning disappears whenever the complete board fits. Terminals need at least 19 rows and enough columns for a 9-column viewport plus the help panel (normally 47 columns).
