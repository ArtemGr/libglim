#include "rethinkdb.hpp"
#include "gstring.hpp"
#include "ql2.pb.cc"
#include "exception.hpp"
#include <assert.h>
#include <iostream>
#include <string>
using std::cout; using std::endl; using std::flush;
#include <boost/algorithm/string/predicate.hpp>

/**
 * Testing if we can work with the RethinkDB from C++.\n
 * This method will connect to 127.0.0.1:28015 creating a "glimTest" database. \n
 * RethinkDB protocol: https://github.com/rethinkdb/rethinkdb/blob/next/src/rdb_protocol/ql2.proto; see "make src/ql2.pb.h".\n
 * Client example (Python): https://github.com/neumino/rethinkdb-driver-development\n
 *   python rethinkdb-driver-development/test.py
 */
int main () {
  cout << "Testing rethinkdb.hpp ... " << flush;
  using glim::RethinkDB;
  auto rdb = RethinkDB::create();
  rdb.dbCreate ("glimTest");
  rdb.db ("glimTest") .tableCreate ("test", "id", "soft", 1);
  Term johnDoe; johnDoe.set_type (Term::MAKE_OBJ);
  RethinkDB::setDatumS (RethinkDB::addOptArg (&johnDoe, "id"), "JohnDoe");
  RethinkDB::setDatumS (RethinkDB::addOptArg (&johnDoe, "hero"), "John Doe");
  rdb.db ("glimTest") .table ("test") .insert (johnDoe);
  rdb.db ("glimTest") .table ("test") .get ("JohnDoe") .erase();
  assert (rdb.dbDrop ("glimTest") == true);
  assert (rdb.dbDrop ("glimTest") == false);
  cout << "pass." << endl;
  return 0;
}
