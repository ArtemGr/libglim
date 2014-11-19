#ifndef GLIM_MEMCACHE_HPP_
#define GLIM_MEMCACHE_HPP_

#include <memcache.h>
#include <mutex> // http://en.cppreference.com/w/cpp/thread/mutex
#include <string>
#include <stdexcept>
#include <stdlib.h> // free

namespace glim {

//! Header-only wrapper around libmemcache.
//! Debian: "apt-get install libmemcache-dev".
class Memcache {
protected:
  const char* _host;
  const char* _port;
  memcache* _mc;
  std::mutex _mutex;
public:
  Memcache (const char* host, const char* port): _host (host), _port (port) {
    memcache* mc = mc_new();
    mc_server_add (mc, host, port);
    _mc = mc;
  }
  void reconnect() {
    std::unique_lock<std::mutex> lock (_mutex);
    memcache* mc = mc_new();
    mc_server_add (mc, _host, _port);
    memcache* old = _mc;
    _mc = mc;
    mc_free (old);
  }
  //! Throws `runtime_error` if not successfull.
  void set (std::string key, std::string value, time_t expire = 0, u_int16_t flags = 0) {
    std::unique_lock<std::mutex> lock (_mutex);
    int ret = mc_set (_mc, (char*) key.c_str(), key.length(), value.c_str(), value.length(), expire, flags);
    if (ret != 0) throw std::runtime_error (std::string ("mc_set"));
  }
  std::string get (std::string key) {
    std::unique_lock<std::mutex> lock (_mutex);
    size_t retlen = 0;
    void* data = mc_aget2 (_mc, (char*) key.c_str(), key.length(), &retlen);
    if (data == NULL) return std::string();
    std::string ret ((const char*) data, retlen);
    free (data);
    return ret;
  }
  void remove (std::string key, const time_t hold = 1) {
    std::unique_lock<std::mutex> lock (_mutex);
    int ret = mc_delete (_mc, (char*) key.c_str(), key.length(), hold);
    if (ret != 0) throw std::runtime_error (std::string ("mc_delete"));
  }
  virtual ~Memcache () {
    std::unique_lock<std::mutex> lock (_mutex);
    mc_free (_mc);
  }
};

}; // namespace glim

#endif // GLIM_MEMCACHE_HPP_
