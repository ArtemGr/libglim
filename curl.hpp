// Very simple header-only wrapper around libcurl.
// See also: http://thread.gmane.org/gmane.comp.web.curl.library/1322 (this one uses a temporary file).

#ifndef _GLIM_CURL_INCLUDED
#define _GLIM_CURL_INCLUDED

#include "gstring.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <string.h>
#include <stdint.h>

namespace glim {

inline size_t curlWriteToGstring (void *buffer, size_t size, size_t nmemb, void *userp) {
  ((gstring*) userp)->append ((const char*) buffer, size * nmemb);
  return size * nmemb;};

inline size_t curlReadFromGstring (void *ptr, size_t size, size_t nmemb, void *userdata);

//inline size_t curlWriteToString (void *buffer, size_t size, size_t nmemb, void *userp) {
//  ((std::string*) userp)->append ((const char*) buffer, size * nmemb);
//  return size * nmemb;};

/**
 * Simple HTTP requests using cURL.
 * Example:
 *   std::string w3 = glim::Curl() .http ("http://www.w3.org/") .go().str();
 */
class Curl {
 protected:
  Curl (const Curl&): _curl (NULL), _headers (NULL), _send (NULL), _sent (0) {} // No copying.
 public:
  struct PerformError: std::runtime_error {
    char _message[CURL_ERROR_SIZE+1];
    PerformError (const char* message): std::runtime_error(std::string()) {
      strncpy (_message, message, CURL_ERROR_SIZE); _message[CURL_ERROR_SIZE] = 0;}
    virtual const char* what() const throw() {return _message;}
  };
  struct GetinfoError: std::runtime_error {
    CURLINFO _info; const char* _error;
    GetinfoError (CURLINFO info, const char* error): std::runtime_error(std::string()), _info (info), _error (error) {}
    virtual const char* what() const throw() {return _error;}
  };
 public:
  CURL* _curl;
  struct curl_slist *_headers;
  gstring* _send;
  gstring _localSend;
  uint32_t _sent;
  gstring _got;
  char _errorBuf[CURL_ERROR_SIZE];

  Curl(): _curl (curl_easy_init()), _headers (NULL), _send (NULL), _sent (0) {*_errorBuf = 0;}
  /** Tries to reuse the given character buffer. */
  Curl (size_t bufSize, char* buf): _curl (curl_easy_init()), _headers (NULL), _send (NULL), _sent (0),
    _got (bufSize, buf, false, 0) {*_errorBuf = 0;}
  /** Wraps an existing handle (will invoke `curl_easy_cleanup` nevertheless). */
  Curl (CURL* curl): _curl (curl), _headers (NULL), _send (NULL), _sent (0) {*_errorBuf = 0;}
  ~Curl(){
    if (_headers) {curl_slist_free_all (_headers); _headers = NULL;}
    if (_curl) {curl_easy_cleanup (_curl); _curl = NULL;}
  }

  Curl& send (gstring* text) {_send = text; _sent = 0; return *this;}
  Curl& send (const gstring& text) {_localSend = text; _send = &_localSend; _sent = 0; return *this;}

  /** Adds "Content-Type" header into `_headers`. */
  Curl& contentType (const char* ct) {
    char ctb[64]; gstring cth (sizeof (ctb), ctb, false, 0);
    cth << "Content-Type: " << ct << "\r\n";
    _headers = curl_slist_append (_headers, cth.c_str());
    return *this;
  }

  /**
   * Sets the majority of options for the http request.\n
   * NB: If `send` was used with a non-empty string then `http` will use `CURLOPT_UPLOAD`, setting http method to `PUT`.
   */
  Curl& http (const char* url, int timeoutSec = 20) {
    curl_easy_setopt (_curl, CURLOPT_URL, url);
    curl_easy_setopt (_curl, CURLOPT_WRITEFUNCTION, curlWriteToGstring);
    curl_easy_setopt (_curl, CURLOPT_WRITEDATA, &_got);
    curl_easy_setopt (_curl, CURLOPT_TIMEOUT, timeoutSec);
    curl_easy_setopt (_curl, CURLOPT_NOSIGNAL, 1L); // required per http://curl.haxx.se/libcurl/c/libcurl-tutorial.html#Multi-threading
    curl_easy_setopt (_curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP);
    curl_easy_setopt (_curl, CURLOPT_ERRORBUFFER, _errorBuf);
    if (_send && !_send->empty()) {
      curl_easy_setopt (_curl, CURLOPT_UPLOAD, 1L); // http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTUPLOAD
      curl_easy_setopt (_curl, CURLOPT_INFILESIZE, (long) _send->size());
      curl_easy_setopt (_curl, CURLOPT_READFUNCTION, curlReadFromGstring);
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
    if (_send && !_send->empty()) {
      curl_easy_setopt (_curl, CURLOPT_READFUNCTION, glim::curlReadFromGstring);
      curl_easy_setopt (_curl, CURLOPT_READDATA, this);
    }
    return *this;
  }

  /** Uses `CURLOPT_CUSTOMREQUEST` to set the http method. */
  Curl& method (const char* method) {
    curl_easy_setopt (_curl, CURLOPT_CUSTOMREQUEST, method);
    return *this;
  }

  Curl& go() {
    *_errorBuf = 0;
    if (curl_easy_perform (_curl)) throw PerformError (_errorBuf);
    return *this;
  }

  std::string str() const {return _got.str();}
  gstring& gstr() {return _got;}
  const gstring& gstr() const {return _got;}
  const char* c_str() {return _got.c_str();}

  long status() {
    long status; CURLcode err = curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &status);
    if (err) throw GetinfoError (CURLINFO_RESPONSE_CODE, curl_easy_strerror (err));
    return status;}

};

inline size_t curlReadFromGstring (void *ptr, size_t size, size_t nmemb, void *userdata) {
  Curl* curl = (Curl*) userdata;
  size_t len = std::min (curl->_send->size() - curl->_sent, size * nmemb);
  if (len) memcpy (ptr, curl->_send->data() + curl->_sent, len);
  curl->_sent += len;
  return len;}

/**
 * Example:
 *   std::string w3 = glim::curl2str ("http://www.w3.org/");
 */
inline std::string curl2str (const char* url, int timeoutSec = 20) {
  return glim::Curl().http (url, timeoutSec) .go().str();}

}

#endif
