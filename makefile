CC=g++
CPPFLAGS=-std=c++11
minesweeper:minesweeper.o getch.o
	${CC} ${CPPFLAGS} -o $@ $^
minesweeper.o:minesweeper.cpp
	${CC} ${CPPFLAGS} -c -o $@ $<
getch.o:getch.cpp getch.h
	${CC} ${CPPFLAGS} -c -o $@ $<
run:minesweeper
	./minesweeper
install:minesweeper
	mv minesweeper /usr/bin
uninstall:
	rm /usr/bin/minesweeper
clean:
	rm *.o
