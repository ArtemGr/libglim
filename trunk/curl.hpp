// Very simple header-only wrapper around libcurl.
// See also: http://thread.gmane.org/gmane.comp.web.curl.library/1322 (this one uses a temporary file).

#ifndef _GLIM_CURL_INCLUDED
#define _GLIM_CURL_INCLUDED

#include "gstring.hpp"
#include "exception.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <functional>
#include <string.h>
#include <stdint.h>

namespace glim {

inline size_t curlWriteToString (void *buffer, size_t size, size_t nmemb, void *userp) {
  ((std::string*) userp)->append ((const char*) buffer, size * nmemb);
  return size * nmemb;};

inline size_t curlReadFromString (void *ptr, size_t size, size_t nmemb, void *userdata);
inline size_t curlWriteHeader (void *ptr, size_t size, size_t nmemb, void *curlPtr);

/**
 * Simple HTTP requests using cURL.
 * Example:
 *   std::string w3 = glim::Curl() .http ("http://www.w3.org/") .go().str();
 */
class Curl {
 protected:
  Curl (const Curl&): _curl (NULL), _headers (NULL), _sent (0), _needs_cleanup (true) {} // No copying.
 public:
  struct PerformError: public glim::Exception {
    PerformError (const char* message, const char* file, int32_t line):
      glim::Exception (message, file, line) {}
  };
  struct GetinfoError: public glim::Exception {
    CURLcode _code; std::string _error;
    GetinfoError (CURLcode code, std::string error, const char* file, int32_t line):
      glim::Exception (error, file, line),
      _code (code), _error (error) {}
  };
 public:
  CURL* _curl;
  struct curl_slist *_headers;
  std::function<void (const char* header, int len)> _headerListener;
  std::string _send; ///< We're using `std::string` instead of `gstring` in order to support payloads larger than 16 MiB.
  uint32_t _sent;
  std::string _got;
  bool _needs_cleanup:1; ///< ~Curl will do `curl_easy_cleanup` if `true`.
  char _errorBuf[CURL_ERROR_SIZE];

  /** @param cleanup can be turned off if the CURL is freed elsewhere. */
  Curl (bool cleanup = true): _curl (curl_easy_init()), _headers (NULL), _sent (0), _needs_cleanup (cleanup) {*_errorBuf = 0;}
  /** Wraps an existing handle (will invoke `curl_easy_cleanup` nevertheless).
   * @param cleanup can be turned off if the CURL is freed elsewhere. */
  Curl (CURL* curl, bool cleanup = true): _curl (curl), _headers (NULL), _sent (0), _needs_cleanup (cleanup) {*_errorBuf = 0;}
  ~Curl(){
    if (_headers) {curl_slist_free_all (_headers); _headers = NULL;}
    if (_curl) {if (_needs_cleanup) curl_easy_cleanup (_curl); _curl = NULL;}
  }

  /** Stores the content to be sent into an `std::string` inside `Curl`.\n
   * In order to have an effect this method should be used *before* the `http` and `smtp` methods. */
  template<typename STR> Curl& send (STR&& text) {_send = std::forward<std::string> (text); _sent = 0; return *this;}

  /** Adds "Content-Type" header into `_headers`. */
  Curl& contentType (const char* ct) {
    char ctb[64]; gstring cth (sizeof (ctb), ctb, false, 0);
    cth << "Content-Type: " << ct << "\r\n";
    _headers = curl_slist_append (_headers, cth.c_str());
    return *this;
  }

  /** @param fullHeader is a full HTTP header and a newline, e.g. "User-Agent: Me\r\n". */
  Curl& header (const char* fullHeader) {
    _headers = curl_slist_append (_headers, fullHeader);
    return *this;
  }

  /**
   * Sets the majority of options for the http request.\n
   * NB: If `send` was used with a non-empty string then `http` will use `CURLOPT_UPLOAD`, setting http method to `PUT`.
   */
  Curl& http (const char* url, int timeoutSec = 20) {
    curl_easy_setopt (_curl, CURLOPT_URL, url);
    curl_easy_setopt (_curl, CURLOPT_WRITEFUNCTION, curlWriteToString);
    curl_easy_setopt (_curl, CURLOPT_WRITEDATA, &_got);
    curl_easy_setopt (_curl, CURLOPT_TIMEOUT, timeoutSec);
    curl_easy_setopt (_curl, CURLOPT_NOSIGNAL, 1L); // required per http://curl.haxx.se/libcurl/c/libcurl-tutorial.html#Multi-threading
    curl_easy_setopt (_curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP);
    curl_easy_setopt (_curl, CURLOPT_ERRORBUFFER, _errorBuf);
    if (_send.size()) {
      curl_easy_setopt (_curl, CURLOPT_UPLOAD, 1L); // http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTUPLOAD
      curl_easy_setopt (_curl, CURLOPT_INFILESIZE, (long) _send.size());
      curl_easy_setopt (_curl, CURLOPT_READFUNCTION, curlReadFromString);
      curl_easy_setopt (_curl, CURLOPT_READDATA, this);}
    if (_headers)
      curl_easy_setopt (_curl, CURLOPT_HTTPHEADER, _headers); // http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTHTTPHEADER
    return *this;
  }

  /**
   * Set options for smtp request.\n
   * Example: \code
   *   long rc = Curl().send ("Subject: subject\r\n\r\n" "text\r\n") .smtp ("from", "to") .go().status();
   *   if (rc != 250) std::cerr << "Error sending email: " << rc << std::endl;
   * \endcode */
  Curl& smtp (const char* from, const char* to) {
    curl_easy_setopt (_curl, CURLOPT_URL, "smtp://127.0.0.1");
    if (from) curl_easy_setopt (_curl, CURLOPT_MAIL_FROM, from);
    if (to) _headers = curl_slist_append (_headers, to);
    if (_headers) curl_easy_setopt (_curl, CURLOPT_MAIL_RCPT, _headers);
    if (_send.size()) {
      curl_easy_setopt (_curl, CURLOPT_INFILESIZE, (long) _send.size());
      curl_easy_setopt (_curl, CURLOPT_READFUNCTION, curlReadFromString);
      curl_easy_setopt (_curl, CURLOPT_READDATA, this);
    }
    return *this;
  }

  /** Uses `CURLOPT_CUSTOMREQUEST` to set the http method. */
  Curl& method (const char* method) {
    curl_easy_setopt (_curl, CURLOPT_CUSTOMREQUEST, method);
    return *this;
  }

  /** Setup a handler to process the headers CURL gets from the response.\n
   * "The header callback will be called once for each header and only complete header lines are passed on to the callback".\n
   * See http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTHEADERFUNCTION */
  Curl& headerListener (std::function<void (const char* header, int len)> listener) {
    curl_easy_setopt (_curl, CURLOPT_HEADERFUNCTION, curlWriteHeader);
    curl_easy_setopt (_curl, CURLOPT_WRITEHEADER, this);
    _headerListener = listener;
    return *this;
  }

  /** Reset the buffers and perform the CURL request. */
  Curl& go() {
    _got.clear();
    *_errorBuf = 0;
    if (curl_easy_perform (_curl)) throw PerformError (_errorBuf, __FILE__, __LINE__);
    return *this;
  }

  std::string str() const {return _got;}
  const char* c_str() const {return _got.c_str();}
  gstring gstr() const {return gstring (0, (void*) _got.data(), false, _got.size());}

  long status() const {
    long status; CURLcode err = curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &status);
    if (err) throw GetinfoError (err, std::string ("CURL error ") + std::to_string (err) + ": " + curl_easy_strerror (err), __FILE__, __LINE__);
    return status;}
};

inline size_t curlReadFromString (void *ptr, size_t size, size_t nmemb, void *userdata) {
  Curl* curl = (Curl*) userdata;
  size_t len = std::min (curl->_send.size() - curl->_sent, size * nmemb);
  if (len) memcpy (ptr, curl->_send.data() + curl->_sent, len);
  curl->_sent += len;
  return len;}

// http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTHEADERFUNCTION
inline size_t curlWriteHeader (void *ptr, size_t size, size_t nmemb, void *curlPtr) {
  Curl* curl = (Curl*) curlPtr;
  std::function<void (const char* header, int len)>& listener = curl->_headerListener;
  int len = size * nmemb;
  if (listener) listener ((const char*) ptr, len);
  return (size_t) len;
}

/**
 * Example:
 *   std::string w3 = glim::curl2str ("http://www.w3.org/");
 */
inline std::string curl2str (const char* url, int timeoutSec = 20) {
  try {
    return glim::Curl().http (url, timeoutSec) .go().str();
  } catch (const std::exception&) {}
  return std::string();
}

}

#endif
