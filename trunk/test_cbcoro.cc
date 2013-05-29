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
  RemoveFroples (const char* argument): _argument (argument) {
    printf ("RF: constructor\n");
    cbcStart();
  }
  virtual void run() throw() override {
    for (int i = 1; i <= 4; ++i) {
      printf ("RF: Removing frople %i...\n", i);
      int returnedFrople = 0;
      yieldForCallback ([this,i,&returnedFrople]() {
        if (i != 2) {
          // Sometimes we use a callback.
          esDelete (i, [this,&returnedFrople](int frople) {
            printf ("RF,CB: frople %i.\n", frople);
            returnedFrople = frople;
            invokeFromCallback();
          });
        } else {
          // Sometimes we don't use a callback.
          returnedFrople = 0;
          invokeFromCallback();
        }
      });
      printf ("RF: Returned from callback; _returnTo is: %li; frople %i\n", (intptr_t) _returnTo, returnedFrople);
    }
    deleteLater (this);
    printf ("RF: finish! _returnTo is: %li\n", (intptr_t) _returnTo);
    cbcReturn();
  };
};

int main() {
  new RemoveFroples ("argument");
  printf ("main: returned from RemoveFroples\n");
  sleep (5);
  CBCoroStatic::maintance(); // Perform any pending cleanups.
  return 0;
}
