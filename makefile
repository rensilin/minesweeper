minesweeper:minesweeper.o getch.o
	g++ -o $@ $^
minesweeper.o:minesweeper.cpp SColor/SColor.h
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
