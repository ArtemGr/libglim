SQLite hearder-only C++ wrapper.

```
#include "glim/sqlite.hpp"
#define S(cstr) (std::pair<char const*, int> (cstr, sizeof (cstr) - 1))
using namespace glim;
#include <iostream>

int main () {
  Sqlite sqlite (":memory:");
  sqlite.exec ("PRAGMA page_size = 4096") .exec ("PRAGMA secure_delete = 1");
  SqliteSession sqs (&sqlite);
  sqs.query ("CREATE TABLE test (t TEXT, i INTEGER)") .ustep();
  sqs.query ("INSERT INTO test VALUES (?, ?)") .bind (1, S("foo")) .bind (2, 27) .ustep();
  std::cout << sqs.query ("SELECT t FROM test") .qstep() .stringAt (1) << std::endl; // foo
  std::cout << sqs.query ("SELECT i FROM test") .qstep() .intAt (1) << std::endl; // 27
  return 0;
}
```

Header-only C++ wrapper around the libevent HTTP client.

```
// compile with "g++ -std=c++0x hget.cc -o hget -levhtp -levent_extra -levent_pthreads"
#include <boost/assign/list_of.hpp>
#include <glim/hget.hpp>
using glim::hget; using glim::hgot;
int main () {
  std::shared_ptr<struct event_base> evbase (event_base_new(), event_base_free);
  std::shared_ptr<struct evdns_base> evdns (evdns_base_new (evbase.get(), 1),
    [](struct evdns_base* dnsbase) {evdns_base_free (dnsbase, 1);});
  // Try to send an HTTP request to either server1 or server2.
  // Stop trying after ten attempts.
  hget (evbase, evdns) .goUntilC (boost::assign::list_of ("http://server1/") ("http://server2/"),
    [](hgot& got, hget::uri_t uri, int32_t num)->float {
      std::cout << "hget handler; server: " << evhttp_uri_get_host (uri.get())
        << "; num: " << num << "; status: " << got.status << std::endl;
      if (got.status != 200 && num < 10) return 1.f; // Try different server in a sec.
      return -1.f; // Do not retry anymore.
  });
  evdns.reset();
  event_base_dispatch (evbase.get());
  return 0;
}
```

Documentation: http://glim.ru/open/libglim/html/annotated.html