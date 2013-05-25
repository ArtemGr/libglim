// http://en.wikipedia.org/wiki/Setcontext; man 3 makecontext; man 2 getcontext
// http://www.boost.org/doc/libs/1_53_0/libs/context/doc/html/index.html
// g++ -std=c++11 -O1 -Wall -g test_cbcoro.cc -pthread && ./a.out

#define _GLIM_EXCEPTION_CODE
#include <glim/exception.hpp>

#include "cbcoro.hpp"
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

/** A typical remote service with callback. */
void esDelete (int frople, std::function<void(int)> cb) {
  std::thread th ([cb,frople]() {
    printf ("esDelete: sleeping for a second\n");
    std::this_thread::sleep_for (std::chrono::seconds (1));
    cb (frople);
  }); th.detach();
}

struct RemoveFroples: public CBCoro<4096> {
  const char* _argument;
  bool _fromCB;
  RemoveFroples (const char* argument): _argument (argument), _fromCB (false) {
    printf ("RF: constructor\n");
    cbcStart();
  }
  virtual void run() override {
    printf ("RF: run\n");
    for (int i = 1; i <= 4; ++i) {
      printf ("RF: Removing frople %i...\n", i);
      yieldForCallback ([&]() {
        printf ("RF: in yieldForCallback; %i\n", __LINE__);
        _fromCB = false;
        if (getcontext (&_context)) GTHROW ("!getcontext"); // Capture.
        if (_fromCB) {
          printf ("RF: in future; %i; _returnTo is: %li\n", __LINE__, (intptr_t) _returnTo);
        } else { // If still in the present.
          printf ("RF: in present; %i; _returnTo is: %li\n", __LINE__, (intptr_t) _returnTo);
//          esDelete (i, [this](int frople) {
//            printf ("RF,CB: frople %i; resuming RemoveFroples; _returnTo is: %li.\n", frople, (intptr_t) _returnTo);
            if (_returnTo != nullptr) {
              // We have not yet "returned" from the `yieldForCallback`,
              // meaning that the `invokeFromCallback` was executed immediately from inside the `yieldForCallback`.
              // In that case we must DO NOTHING, we must simply continue running on the current stack.
              _invokedFromYield = true;
            } else {
              ucontext_t cbContext; _returnTo = &cbContext; _fromCB = true;
              if (swapcontext (&cbContext, &_context)) GTHROW ("!swapcontext");
              if (_returnTo == &cbContext) _returnTo = nullptr;
            }
//          });
          if (_invokedFromYield) {
            printf ("RF: returning; to the current stack, because of _invokedFromYield\n");
            _invokedFromYield = false;
          } else {
            printf ("RF: returning; _returnTo is: %li\n", (intptr_t) _returnTo);
            ucontext_t* returnTo = _returnTo;
            _returnTo = nullptr;
            if (returnTo != nullptr) setcontext (returnTo);
          }
        }
      });
      printf ("RF: Returned from callback; _returnTo is: %li\n", (intptr_t) _returnTo);
    }
    deleteLater (this);
    printf ("RF: finish! _returnTo is: %li\n", (intptr_t) _returnTo);
    if (_returnTo != nullptr) setcontext (_returnTo);
  };
};

int main() {
  new RemoveFroples ("argument");
  printf ("main: returned from RemoveFroples\n");
  sleep (5);
  CBCoroStatic::maintance(); // Perform any pending cleanups.
  return 0;
}
