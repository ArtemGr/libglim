#define _GLIM_EXCEPTION_CODE
#include "rethinkdb.hpp"
#include "gstring.hpp"
#include "ql2.pb.h"
#include "ql2.pb.cc"
#include "exception.hpp"
#include <assert.h>
#include <iostream>
#include <string>
using std::cout; using std::endl; using std::flush;
#include <boost/algorithm/string/predicate.hpp>

/**
 * Testing if we can work with the RethinkDB from C++.\n
 * RethinkDB protocol: https://github.com/rethinkdb/rethinkdb/blob/next/src/rdb_protocol/ql2.proto; see "make src/ql2.pb.h".
 * Client example (Python): https://github.com/neumino/rethinkdb-driver-development
 *   pro1453: python /root/work/rethinkdb-driver-development/test.py
 */
void firstTest() {
  using std::string; using std::cout; using std::endl; using boost::asio::ip::tcp;
  asio::io_service io; tcp::resolver resolver (io);
  tcp::socket sock (io);
  sock.connect (asio::ip::basic_endpoint<tcp> (asio::ip::address_v4::from_string ("127.0.0.1"), 28015));

  GSTRING_ON_STACK (outBuf, 64);
  {int32_t version = VersionDummy::Version_MAX; outBuf.append ((const char*) &version, 4);}
  {int32_t authLen = 0; outBuf.append ((const char*) &authLen, 4);}
  size_t rc = asio::write (sock, asio::buffer (outBuf.data(), outBuf.length())); assert (rc == outBuf.length());

  asio::streambuf inBuf;  // http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/basic_streambuf.html
  std::istream inStream (&inBuf); boost::system::error_code ec;
  rc = asio::read_until (sock, inBuf, (char) 0, ec); if (ec.value()) GTHROW ("!read_until: " + ec.message());
  string inGather; inStream >> inGather; if (!boost::starts_with (inGather, "SUCCESS")) GTHROW ("Can't connect to RethinkDB: " + inGather);

  // r.db_create("img2"); query {type: DB_CREATE; args {type: DATUM; datum: {type: R_STR: r_str: "glimTest"}}}
  int64_t token = 0;  // Response will have the same token.
  Query query; query.set_type (Query::START); query.set_token (++token);
  Term* term = query.mutable_query(); Term* args = term->add_args();
  term->set_type (Term::DB_CREATE); args->set_type (Term::DATUM);
  args->mutable_datum()->set_type (Datum::R_STR); args->mutable_datum()->set_r_str ("glimTest");
  outBuf.clear().append ("\x0\x0\x0\x0", 4); {glim::gstring_stream gs (outBuf); std::ostream os (&gs); query.SerializeToOstream (&os);}
  *(int32_t*)outBuf._buf = outBuf.length() - 4;
  rc = asio::write (sock, asio::buffer (outBuf.data(), outBuf.length())); assert (rc == outBuf.length());
  uint32_t responseSize = 0;
  rc = asio::read (sock, asio::buffer (&responseSize, 4), ec); if (ec.value()) GTHROW ("!read: " + ec.message()); assert (rc == 4);
  char responseBuf[responseSize];
  rc = asio::read (sock, asio::buffer (responseBuf, responseSize), ec); if (ec.value()) GTHROW ("!read: " + ec.message()); assert (rc == responseSize);
  Response response; rc = response.ParseFromArray (responseBuf, responseSize); assert (rc == 1);
  if (response.type() == Response::CLIENT_ERROR || response.type() == Response::RUNTIME_ERROR) {
    cout << "RethinkDB error: " << response.response().Get (0) .r_str() << endl;
  } else cout << response.Utf8DebugString() << endl;
}

int main () {
  cout << "Testing rethinkdb.hpp ... " << flush;
  firstTest();
  cout << "pass." << endl;
  return 0;
}
