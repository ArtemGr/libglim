
PREFIX = /usr/local
INSTALL2 = ${PREFIX}/include/glim
CXXFLAGS = -std=c++0x -Wall -O2 -ggdb
CXXFLAGS += -I/usr/local/libevent/include -L/usr/local/libevent/lib

all: test

help:
	@echo "make test\nmake install\nmake uninstall\nmake clean"

test: test_sqlite test_gstring test_runner

test_sqlite: bin/test_sqlite
	cp bin/test_sqlite /tmp/libglim_test_sqlite && chmod +x /tmp/libglim_test_sqlite && /tmp/libglim_test_sqlite && rm -f /tmp/libglim_test_sqlite

bin/test_sqlite: test_sqlite.cc
	mkdir -p bin
	g++ $(CXXFLAGS) test_sqlite.cc -o bin/test_sqlite -lsqlite3

test_memcache: bin/test_memcache
	cp bin/test_memcache /tmp/libglim_test_memcache && chmod +x /tmp/libglim_test_memcache && /tmp/libglim_test_memcache && rm -f /tmp/libglim_test_memcache

bin/test_memcache: test_memcache.cc memcache.hpp
	mkdir -p bin
	g++ $(CXXFLAGS) test_memcache.cc -o bin/test_memcache -lmemcache

bin/test_gstring: test_gstring.cc gstring.hpp
	mkdir -p bin
	g++ $(CXXFLAGS) test_gstring.cc -o bin/test_gstring

test_gstring: bin/test_gstring
	cp bin/test_gstring /tmp/libglim_test_gstring
	chmod +x /tmp/libglim_test_gstring
	/tmp/libglim_test_gstring
	rm -f /tmp/libglim_test_gstring

bin/test_runner: test_runner.cc runner.hpp curl.hpp
	mkdir -p bin
	g++ $(CXXFLAGS) -D_GLIM_EXCEPTION_CODE test_runner.cc -o bin/test_runner -levent -levent_pthreads -lcurl

test_runner: bin/test_runner
	valgrind -q --leak-check=yes bin/test_runner

bin/test_exception: test_exception.cc exception.hpp
	mkdir -p bin
	g++ $(CXXFLAGS) -D_GLIM_EXCEPTION_CODE test_exception.cc -o bin/test_exception -ldl -rdynamic

test_exception: bin/test_exception
	valgrind -q --leak-check=yes bin/test_exception

test_ldb: test_ldb.cc ldb.hpp
	mkdir -p bin
	g++ $(CXXFLAGS) -D_GLIM_EXCEPTION_CODE test_ldb.cc -o bin/test_ldb \
	  -lleveldb -lboost_serialization-mt -lboost_filesystem-mt -lboost_system-mt
	valgrind -q --leak-check=yes bin/test_ldb

install:
	mkdir -p ${INSTALL2}/
	cp sqlite.hpp ${INSTALL2}/
	cp NsecTimer.hpp ${INSTALL2}/
	cp TscTimer.hpp ${INSTALL2}/
	cp memcache.hpp ${INSTALL2}/
	cp gstring.hpp ${INSTALL2}/
	cp runner.hpp ${INSTALL2}/
	cp hget.hpp ${INSTALL2}/
	cp curl.hpp ${INSTALL2}/
	cp mdb.hpp ${INSTALL2}/
	cp ldb.hpp ${INSTALL2}/
	cp exception.hpp ${INSTALL2}/

uninstall:
	rm -rf ${INSTALL2}

clean:
	rm -rf bin/*
	rm -f /tmp/libglim_test_*
	rm -f *.exe.stackdump
