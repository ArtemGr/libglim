#include "ql2.pb.h"
#include "gstring.hpp"
#include <memory>
#include <atomic>
#include <list>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp> // http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference.html
namespace asio = boost::asio;
namespace glim {

/**
 * Header-only [RethinkDB](http://rethinkdb.com/) client.
 * Requires Boost ASIO and Protobuf (link with -lboost_system-mt -lprotobuf).\n
 * Patches welcome! (To submit a patch please file an issue at http://code.google.com/p/libglim/issues/entry)
 */
class RethinkDB {
protected:
  using tcp = asio::ip::tcp;
  std::shared_ptr<asio::io_service> _io;  ///< `io_service` used to communicate with Rethinkdb.
  tcp::socket _sock;  ///< Socket connected to Rethinkdb.
  volatile int64_t _token;  ///< Token to use when creating a new Query.
  std::list<Response> _responses;  ///< Database responses whose tokens didn't immediately match the expected one. See RethinkDB::waitForResponse.

  static bool isError (const Response& response) {
    return response.type() == Response::RUNTIME_ERROR || response.type() == Response::CLIENT_ERROR || response.type() == Response::COMPILE_ERROR;}
  static std::string getError (const Response& response) {  // Must be `isError (response)`.
    return response.response().Get (0) .r_str();}
public:
  RethinkDB (RethinkDB&&) = default;
  RethinkDB (const RethinkDB&) = delete;
  RethinkDB (std::shared_ptr<asio::io_service>&& io, tcp::socket&& sock): _io (std::move (io)), _sock (std::move (sock)), _token (0) {
    GSTRING_ON_STACK (buf, 64);
    {int32_t version = VersionDummy::Version_MAX; buf.append ((const char*) &version, 4);}
    {int32_t authLen = 0; buf.append ((const char*) &authLen, 4);}
    size_t rc = asio::write (_sock, asio::buffer (buf.data(), buf.length()));
    if (rc != buf.length()) GTHROW ("Error sending negotiation to RethinkDB socket");

    boost::system::error_code ec; asio::streambuf in;  // http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/basic_streambuf.html
    rc = asio::read_until (_sock, in, (char) 0, ec); if (ec.value()) GTHROW ("!read_until: " + ec.message());
    buf.length (in.sgetn ((char*) buf._buf, buf.capacity()));
    if (!boost::starts_with (buf, "SUCCESS")) GTHROW ("Can't connect to RethinkDB: " + buf.str());
  }
  /** Connect to the given `ip` and `port` (127.0.0.1:28015 by default).
   * @param io If given then this `io_service` is used, otherwise a new one is allocated. */
  static RethinkDB create (const char* ip = "127.0.0.1", int port = 28015, std::shared_ptr<asio::io_service> io = std::make_shared<asio::io_service>()) {
    tcp::socket sock (*io);
    sock.connect (asio::ip::basic_endpoint<tcp> (asio::ip::address_v4::from_string (ip), port));
    return RethinkDB (std::move (io), std::move (sock));
  }

  /** Serialize and send the given query into the TCP/IP socket. */
  void sendQuery (const Query& query) {
    GSTRING_ON_STACK (buf, 512); glim::gstring_stream gs (buf); std::ostream os (&gs);
    buf.append ("\x0\x0\x0\x0", 4); // Length placeholder.
    query.SerializeToOstream (&os);
    uint32_t bufLength = buf.length() - 4;
    ((uint8_t*)buf._buf)[0] = bufLength & 0xFF; // Length header, little-endian.
    ((uint8_t*)buf._buf)[1] = (bufLength >> 8) & 0xFF;
    ((uint8_t*)buf._buf)[2] = (bufLength >> 16) & 0xFF;
    ((uint8_t*)buf._buf)[3] = bufLength >> 24;
    boost::system::error_code ec;
    size_t rc = asio::write (_sock, asio::buffer (buf.data(), buf.size()), ec);
    if (ec.value()) GTHROW ("Error writing " + std::to_string (buf.size()) + " bytes to RethinkDB socket: " + ec.message());
    if (rc != buf.size()) GTHROW ("Error writing " + std::to_string (buf.size()) + " bytes to RethinkDB socket: rc = " + std::to_string (rc));
  }

  /** Read responses from the database socket until the Response with the given token is found.
   * Responses not matching the token are put into RethinkDB::_responses.
   * @returns the Response whose token matches the argument. */
  Response waitForResponse (int64_t token) {
    for (;;) {
      // See if the needed response is already there.
      for (std::list<Response>::iterator it = _responses.begin(), end = _responses.end(); it != end; ++it)
        if (it->token() == token) {Response tmp (std::move (*it)); _responses.erase (it); return tmp;}

      // Read a new response from the socket.
      uint8_t buf[4]; boost::system::error_code ec;
      size_t rc = asio::read (_sock, asio::buffer (buf, 4), ec);
      if (ec.value()) GTHROW ("Error reading 4 bytes from RethinkDB socket: " + ec.message());
      if (rc != 4) GTHROW ("Error reading 4 bytes from RethinkDB socket: rc = " + std::to_string (rc));
      uint32_t responseSize = 0;
      responseSize |= buf[0]; responseSize |= buf[1] << 8; responseSize |= buf[2] << 16; responseSize |= buf[3] << 24;
      char responseBuf[responseSize];
      rc = asio::read (_sock, asio::buffer (responseBuf, responseSize), ec);
      if (ec.value()) GTHROW ("Error reading " + std::to_string (responseSize) + " bytes from RethinkDB socket: " + ec.message());
      if (rc != responseSize) GTHROW ("Error reading " + std::to_string (responseSize) + " bytes from RethinkDB socket: rc = " + std::to_string (rc));
      Response response; rc = response.ParseFromArray (responseBuf, responseSize);
      if (rc != 1) GTHROW ("Error parsing RethinkDB response");
      if (response.token() != token) _responses.emplace_back (std::move (response));
      else return response;
    }
    return Response(); // Never happens.
  }

  struct Db {
    RethinkDB* _rdb; const char* _db;
    Db (RethinkDB* rdb, const char* db): _rdb (rdb), _db (db) {}
    struct Table {
      Db* _db; const char* _table; bool _useOutdated;
      Table (Db* db, const char* table, bool useOutdated): _db (db), _table (table), _useOutdated (useOutdated) {}
    };
    /** Reference a table. */
    Table table (const char* table, bool useOutdated = false) {return Table (this, table, useOutdated);}
  };
  /** <a href="http://www.rethinkdb.com/api/#js:selecting_data-db">Reference a database</a>.
   * Example: Before we can query a table we have to select the correct database.\n
   * `r.db("heroes").table("marvell")` */
  Db db (const char* db) {return Db (this, db);}
  /** <a href="http://www.rethinkdb.com/api/#js:manipulating_databases-db_create">Create a database</a>.
   * A RethinkDB database is a collection of tables, similar to relational databases.\n
   * If successful, the operation returns `true`. If a database with the same name already exists the operation returns `false`.\n
   * Note: that you can only use alphanumeric characters and underscores for the database name.\n
   * Example: Create a database named 'superheroes'.\n
   * `r.dbCreate("superheroes")`\n */
  bool dbCreate (const char* db) {
    const int64_t token = ++_token;
    Query query; query.set_type (Query::START); query.set_token (token);
    Term* term = query.mutable_query(); term->set_type (Term::DB_CREATE);
    Term* args = term->add_args(); args->set_type (Term::DATUM);
    Datum* datum = args->mutable_datum(); datum->set_type (Datum::R_STR); datum->set_r_str (db);
    sendQuery (query);
    Response response (waitForResponse (token));
    if (response.type() == Response::SUCCESS_ATOM) return true;
    if (response.type() == Response::RUNTIME_ERROR && getError (response) == (std::string ("Database `") + db + "` already exists.")) return false;
    if (isError (response)) GTHROW (std::string ("RethinkDB::createDb (") + db + "): " + getError (response));
    GTHROW (response.DebugString());
  }

};

}
