#include "ql2.pb.h"
#include "gstring.hpp"
#include <memory>
#include <atomic>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp> // http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference.html
namespace asio = boost::asio;
namespace glim {

/**
 * Header-only RethinkDB client.
 * Requires Boost ASIO and Protobuf (link with -lboost_system-mt -lprotobuf).\n
 * Patches welcome! (To submit a patch please file an issue at http://code.google.com/p/libglim/issues/entry)
 */
class RethinkDB {
protected:
  using tcp = asio::ip::tcp;
  std::shared_ptr<asio::io_service> _io;
  tcp::socket _sock;
  std::atomic<int64_t> _token; ///< Token used for the last Query. Incremented when making a new Query.
public:
  RethinkDB (RethinkDB&&) = default;
  RethinkDB (const RethinkDB&) = delete;
  RethinkDB (std::shared_ptr<asio::io_service>&& io, tcp::socket&& sock): _io (std::move (io)), _sock (std::move (sock)) {
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
  static RethinkDB create (const char* ip = "127.0.0.1", int port = 28015, std::shared_ptr<asio::io_service> io = std::make_shared<asio::io_service>()) {
    tcp::socket sock (*io);
    sock.connect (asio::ip::basic_endpoint<tcp> (asio::ip::address_v4::from_string (ip), port));
    return RethinkDB (std::move (io), std::move (sock));
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
    return false;
  }

};

}
