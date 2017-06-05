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
