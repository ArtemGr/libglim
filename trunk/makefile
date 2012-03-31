
PREFIX = /usr/local
INSTALL2 = ${PREFIX}/include/glim

all: test

help:
	@echo "make test\nmake install\nmake uninstall\nmake clean"

test: test_sqlite

test_sqlite: bin/test_sqlite
	cp bin/test_sqlite /tmp/libglim_test_sqlite && chmod +x /tmp/libglim_test_sqlite && /tmp/libglim_test_sqlite && rm -f /tmp/libglim_test_sqlite

bin/test_sqlite: test_sqlite.cc
	mkdir -p bin
	g++ -std=c++0x -Wall -Ofast -g3 test_sqlite.cc -o bin/test_sqlite -lsqlite3

install:
	mkdir -p ${INSTALL2}/
	cp sqlite.hpp ${INSTALL2}/
	cp NsecTimer.hpp ${INSTALL2}/
	cp TscTimer.hpp ${INSTALL2}/

uninstall:
	rm -f ${INSTALL2}/sqlite.hpp
	rm -f ${INSTALL2}/NsecTimer.hpp
	rm -f ${INSTALL2}/TscTimer.hpp

clean:
	rm -rf bin/
