#include "runner.hpp"
#include "curl.hpp"
#include <thread>
#include <assert.h>

int main () {
  std::cout << "Testing runner.hpp ..." << std::flush; try {

    std::shared_ptr<struct event_base> evbase (event_base_new(), event_base_free);
    glim::Runner runner (evbase, [](const char* error) {std::cerr << error << std::endl;});
    auto curl = std::make_shared<glim::Curl> (false); curl->http ("http://glim.ru/env.cgi?pause=50", 5);
    volatile bool ran = false;
    runner.multi (curl->_curl, [curl,&ran,evbase](CURLMsg* msg) {
      std::cout << " status: " << curl->status();
      if (curl->status() == 200) std::cout << " ip: " << curl->gstr().view (0, std::max (curl->gstr().find ("\n"), 0));
      ran = true;
      event_base_loopbreak (evbase.get());
    });
    //struct timeval tv {1, 0}; event_base_loopexit (evbase.get(), &tv); // Exit the loop in a sec.
    event_base_dispatch (evbase.get());
    if (!ran) GTHROW ("!ran");

    std::cout << " pass." << std::endl;
    //waiting: "was introduced in Libevent 2.1.1-alpha"//libevent_global_shutdown();

  } catch (const std::exception& ex) {
    std::cerr << " exception: " << ex.what() << std::endl;
    return 1;
  }
  return 0;
}
