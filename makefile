minesweeper:minesweeper.o getch.o SColor/SColor.h
	g++ -o $@ $^
minesweeper.o:minesweeper.cpp
	g++ -c -o $@ $<
getch.o:getch.cpp getch.h
	g++ -c -o $@ $<

SColor/SColor.h:
	git submodule init
	git submodule update

run:minesweeper
	./minesweeper
install:minesweeper
	mv minesweeper /usr/bin
uninstall:
	rm /usr/bin/minesweeper
clean:
	rm *.o
