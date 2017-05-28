CC=g++
CPPFLAGS=-std=c++11
minesweeper:minesweeper.o getch.o
	${CC} ${CPPFLAGS} -o $@ $^
minesweeper.o:minesweeper.cpp SColor/SColor.h
	${CC} ${CPPFLAGS} -c -o $@ $<
getch.o:getch.cpp getch.h
	${CC} ${CPPFLAGS} -c -o $@ $<

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
