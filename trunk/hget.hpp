#ifndef _GLIM_HGET_INCLUDED
#define _GLIM_HGET_INCLUDED

#include <event2/event.h>
#include <event2/dns.h>
#include <evhttp.h> // http://stackoverflow.com/a/5237994; http://archives.seul.org/libevent/users/Sep-2010/msg00050.html

#include <memory>
#include <functional>
#include <stdexcept>
#include <iostream>

#include <stdint.h>
#include <string.h>
#include <errno.h>

namespace glim {

/** HTTP results */
struct hgot {
  int32_t status = 0;
  /** Uses errno codes. */
  int32_t error = 0;
  struct evbuffer* body = 0;
  struct evhttp_request* req = 0;
  size_t bodyLength() const {return evbuffer_get_length (body);}
  const char* bodyCStr() {return (const char*) evbuffer_pullup (body, -1);}
};

/** Used internally to pass both connection and handler into callback. */
struct hgetContext {
  struct evhttp_connection* conn;
  std::function<void(hgot&)> handler;
  hgetContext (struct evhttp_connection* conn, std::function<void(hgot&)> handler): conn (conn), handler (handler) {}
};

/** Invoked when evhttp finishes a request. */
inline void hgetCB (struct evhttp_request* req, void* ctx_){
  hgetContext* ctx = (hgetContext*) ctx_;

  hgot gt;
  if (req == NULL) gt.error = ETIMEDOUT;
  else if (req->response_code == 0) gt.error = ECONNREFUSED;
  else {
    gt.status = req->response_code;
    gt.body = req->input_buffer;
    gt.req = req;
  }

  try {
    ctx->handler (gt);
  } catch (const std::runtime_error& ex) { // Shouldn't normally happen:
    std::cerr << "glim::hget, handler exception: " << ex.what() << std::endl;
  }

  evhttp_connection_free ((struct evhttp_connection*) ctx->conn);
  //freed by libevent//if (req != NULL) evhttp_request_free (req);
  delete ctx;
}

/**
  C++ wrapper around libevent's http client.
  Example:
  hget (evbase, dnsbase) .setRequestBuilder ([](struct evhttp_request* req){
    evbuffer_add (req->output_buffer, "foo", 3);
    evhttp_add_header (req->output_headers, "Content-Length", "3");
  }) .go ("http://127.0.0.1:8080/test", [](hgot& got){
    if (got.error) log_warn ("127.0.0.1:8080 " << strerror (got.error));
    else if (got.status != 200) log_warn ("127.0.0.1:8080 != 200");
    else log_info ("got " << evbuffer_get_length (got.body) << " bytes from /test: " << evbuffer_pullup (got.body, -1));
  });
 */
class hget {
 protected:
  std::shared_ptr<struct event_base> _evbase;
  std::shared_ptr<struct evdns_base> _dnsbase;
  std::function<void(struct evhttp_request*)> _requestBuilder;
 public:
  hget (std::shared_ptr<struct event_base> evbase, std::shared_ptr<struct evdns_base> dnsbase): _evbase (evbase), _dnsbase (dnsbase) {}

  /** Modifies the request before its execution. */
  hget& setRequestBuilder (std::function<void(struct evhttp_request*)> rb) {
    _requestBuilder = rb;
    return *this;
  }

  struct evhttp_request* go (const char* url, int32_t timeoutSec, std::function<void(hgot&)> handler) {
    auto uriDeleter = [](struct evhttp_uri* uri){evhttp_uri_free (uri);};
    std::unique_ptr<struct evhttp_uri, decltype (uriDeleter)> uri (evhttp_uri_parse (url), uriDeleter);
    int port = evhttp_uri_get_port (uri.get());
    if (port == -1) port = 80;
    struct evhttp_connection* conn = evhttp_connection_base_new (_evbase.get(), _dnsbase.get(),
      evhttp_uri_get_host (uri.get()), port);
    evhttp_connection_set_timeout (conn, timeoutSec);
    struct evhttp_request *req = evhttp_request_new (hgetCB, new hgetContext(conn, handler));
    int ret = evhttp_add_header (req->output_headers, "Host", evhttp_uri_get_host (uri.get()));
    if (ret) throw std::runtime_error ("hget: evhttp_add_header(Host) != 0");
    if (_requestBuilder) _requestBuilder (req);
    const char* get = evhttp_uri_get_path (uri.get());
    const char* qs = evhttp_uri_get_query (uri.get());
    if (qs == NULL) {
      ret = evhttp_make_request (conn, req, EVHTTP_REQ_GET, get);
    } else {
      size_t getLen = strlen (get);
      size_t qsLen = strlen (qs);
      char buf[getLen + 1 + qsLen + 1];
      char* caret = stpcpy (buf, get);
      *caret++ = '?';
      caret = stpcpy (caret, qs);
      assert (caret - buf < sizeof (buf));
      ret = evhttp_make_request (conn, req, EVHTTP_REQ_GET, buf);
    }
    if (ret) throw std::runtime_error ("hget: evhttp_make_request != 0");
    return req;
  }
};

}

#endif // _GLIM_HGET_INCLUDED
