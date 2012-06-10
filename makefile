
PREFIX = /usr/local
INSTALL2 = ${PREFIX}/include/glim
CXXFLAGS = -std=c++0x -Wall -O2 -ggdb

all: test

help:
	@echo "make test\nmake install\nmake uninstall\nmake clean"

test: test_sqlite test_gstring

test_sqlite: bin/test_sqlite
	cp bin/test_sqlite /tmp/libglim_test_sqlite && chmod +x /tmp/libglim_test_sqlite && /tmp/libglim_test_sqlite && rm -f /tmp/libglim_test_sqlite

bin/test_sqlite: test_sqlite.cc
	mkdir -p bin
	g++ $(CXXFLAGS) test_sqlite.cc -o bin/test_sqlite -lsqlite3

test_couchbase: bin/test_couchbase
	cp bin/test_couchbase /tmp/libglim_test_couchbase && chmod +x /tmp/libglim_test_couchbase && /tmp/libglim_test_couchbase && rm -f /tmp/libglim_test_couchbase

test_memcache: bin/test_memcache
	cp bin/test_memcache /tmp/libglim_test_memcache && chmod +x /tmp/libglim_test_memcache && /tmp/libglim_test_memcache && rm -f /tmp/libglim_test_memcache

bin/test_couchbase: test_couchbase.cc couchbase.hpp couchbase.cpp
	mkdir -p bin
	g++ $(CXXFLAGS) test_couchbase.cc -o bin/test_couchbase -lcouchbase

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

install:
	mkdir -p ${INSTALL2}/
	cp sqlite.hpp ${INSTALL2}/
	cp NsecTimer.hpp ${INSTALL2}/
	cp TscTimer.hpp ${INSTALL2}/
	cp memcache.hpp ${INSTALL2}/
	cp gstring.hpp ${INSTALL2}/

install_couchbase:
	mkdir -p bin
	g++ $(CXXFLAGS) -fpic -shared -o bin/libglimcouchbase.so couchbase.cpp -lcouchbase
	rm -f /usr/local/lib/libglimcouchbase.so
	cp bin/libglimcouchbase.so /usr/local/lib/
	ldconfig
	cp couchbase.hpp ${INSTALL2}/

uninstall:
	rm -f ${INSTALL2}/sqlite.hpp
	rm -f ${INSTALL2}/NsecTimer.hpp
	rm -f ${INSTALL2}/TscTimer.hpp

clean:
	rm -rf bin/*
	rm -f /tmp/libglim_test_*
	rm -f *.exe.stackdump
