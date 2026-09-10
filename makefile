CC=g++
CPPFLAGS=-std=c++11
CXXFLAGS?=-O2
minesweeper:minesweeper.o
	${CC} ${CPPFLAGS} ${CXXFLAGS} -o $@ $^
minesweeper.o:minesweeper.cpp terminal_input.h
	${CC} ${CPPFLAGS} ${CXXFLAGS} -c -o $@ $<
run:minesweeper
	./minesweeper
install:minesweeper
	mv minesweeper /usr/bin
uninstall:
	rm /usr/share/bash-completion/completions/minesweeper
	rm /usr/bin/minesweeper
clean:
	rm -f *.o tests/terminal_input_test
completion:
	cp completion.sh /usr/share/bash-completion/completions/minesweeper

tests/terminal_input_test:tests/terminal_input_test.cpp terminal_input.h
	${CC} ${CPPFLAGS} ${CXXFLAGS} -I. -o $@ tests/terminal_input_test.cpp
test:minesweeper tests/terminal_input_test
	./tests/terminal_input_test
	python3 tests/test_cli.py
	python3 tests/test_show.py
.PHONY: run install uninstall clean completion test
