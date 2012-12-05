#include "runner.hpp"
#include "curl.hpp"
#include <assert.h>

int main () {
  std::cout << "Testing runner.hpp ..." << std::flush;

  std::shared_ptr<struct event_base> evbase (event_base_new(), event_base_free);
  glim::Runner runner (evbase, [](const char* error) {std::cerr << error << std::endl;});
  auto curl = std::make_shared<glim::Curl> (false); curl->http ("http://glim.ru/env.cgi", 1);
  runner.multi (curl->_curl, [curl](CURLMsg* msg) {
    std::cout << " status: " << curl->status();
    if (curl->status() == 200) std::cout << " ip: " << curl->gstr().view (0, std::max (curl->gstr().find ("\n"), 0));
  });

  std::cout << " pass." << std::endl;
  //waiting: "was introduced in Libevent 2.1.1-alpha"//libevent_global_shutdown();
  return 0;
}
