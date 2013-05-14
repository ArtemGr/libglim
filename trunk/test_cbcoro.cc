// http://en.wikipedia.org/wiki/Setcontext; man 3 makecontext; man 2 getcontext
// http://www.boost.org/doc/libs/1_53_0/libs/context/doc/html/index.html
// g++ -std=c++11 -O1 -Wall -g test_cbcoro.cc -pthread && ./a.out

#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // sleep
#include <string.h> // strerror
#include <errno.h>
#include <functional>
using std::function;
#include <thread>
#include <memory>
using std::shared_ptr; using std::make_shared;
#include <string>
using std::string; using std::to_string;
#include <valgrind/valgrind.h>
#define _GLIM_EXCEPTION_CODE
#include <glim/exception.hpp>

void esDelete (int frople, std::function<void(int)> cb) {
  std::thread th ([cb,frople]() {
    printf ("esDelete: sleeping for a second\n");
    std::this_thread::sleep_for (std::chrono::seconds (1));
    cb (frople);
  }); th.detach();
}

void removeFroples() {
  ucontext_t* rfContext = (ucontext_t*) calloc (1, sizeof (ucontext_t));
  ucontext_t* cbContext = (ucontext_t*) calloc (1, sizeof (ucontext_t));
  for (int i = 1; i <= 4; ++i) {
    printf ("rf: Removing frople %i...\n", i);

    string* got = new string;
    if (getcontext (rfContext)) GTHROW ("!getcontext"); // Capture.
    if (got->empty()) { // If we're still in the present.
      esDelete (i, [rfContext,cbContext,got,i](int frople) {
        *got = to_string (frople) + " removed.";
        printf ("cb: Swapping to rfContext\n");
        if (swapcontext (cbContext, rfContext)) GTHROW ("!swapcontext"); // Resume `removeFroples`.
        printf ("cb: Returned from rfContext\n");
        if (i == 4) free (cbContext); // That was a last callback.
      });
      if (i > 1) {
        printf ("rf: Returning to cbContext\n");
        setcontext (cbContext);
      } else {
        printf ("rf: Returning to main\n");
        return;
      }
    }
    printf ("rf: got for %i: %s\n", i, got->c_str());
    delete got;
  }
  printf ("rf: Returning to cbContext\n");
  free (rfContext);
  setcontext (cbContext);
}

int main() {
  // The function `removeFroples` returns early and so it needs to be run on a separate stack,
  // otherwise its stack would be shared with and corrupted by main.
  shared_ptr<ucontext_t> rfContext ((ucontext_t*) calloc (1, sizeof (ucontext_t)),
    [](ucontext_t* rfContext) {
      if (rfContext->uc_stack.ss_sp) {
        VALGRIND_STACK_DEREGISTER (rfContext->uc_stack.ss_sp);
        free (rfContext->uc_stack.ss_sp);
      }
      free (rfContext);});
  if (getcontext (rfContext.get())) GTHROW ("!getcontext");
  rfContext->uc_stack.ss_sp = calloc (1, 4096);
  rfContext->uc_stack.ss_size = 4096;
  #pragma GCC diagnostic ignored "-Wunused-value"
  VALGRIND_STACK_REGISTER (rfContext->uc_stack.ss_sp, (char*) rfContext->uc_stack.ss_sp + 4096);
  ucontext_t main; rfContext->uc_link = &main;
  makecontext (rfContext.get(), removeFroples, 0);

  swapcontext (&main, rfContext.get()); // Invoke `removeFroples`.
  printf ("main: sleeping...\n");
  sleep (5);
  rfContext.reset();

  printf("main: exiting\n");
  return 0;
}