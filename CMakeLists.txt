cmake_minimum_required(VERSION 3.3)
project(libglim)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    cbcoro.hpp
    channel.hpp
    curl.hpp
    exception.hpp
    gstring.hpp
    hget.hpp
    ldb.hpp
    mdb.hpp
    NsecTimer.hpp
    ql2.pb.cc
    ql2.pb.h
    raii.hpp
    runner.hpp
    SerializablePool.hpp
    sqlite.hpp
    test_cbcoro.cc
    test_exception.cc
    test_gstring.cc
    test_ldb.cc
    test_memcache.cc
    test_runner.cc
    test_sqlite.cc
    TscTimer.hpp)

add_library(libglim ${SOURCE_FILES})
