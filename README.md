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

The side panel always shows the active board size as rows by columns. Command-line dimensions outside 9-100 and boards that exceed the terminal are reduced to supported values and marked `warning:limited`. Resizing to a capacity that changes the board size starts a new game. Terminals smaller than 47 columns by 19 rows show a resize warning instead of the board.
