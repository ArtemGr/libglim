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
#include <sys/ucontext.h>
#include <mutex>

/** A helper for `CBCoro`. */
class CBCoroStatic {
 protected:
  static std::mutex& maintanceMutex() {
    static std::mutex MAINTANCE_MUTEX;
    return MAINTANCE_MUTEX;
  }
  typedef CBCoroStatic* CBCoroPtr;
  static CBCoroPtr& deleteQueue () {
    static CBCoroPtr DELETE_QUEUE = nullptr;
    return DELETE_QUEUE;
  }
  /** Helps the CBCoro instance to delete itself while not stepping on its own toes.
   * @param cbco The instance to remove later; can be nullptr to just delete the previous one. */
  static void deleteLater (CBCoroStatic* cbco) {
    std::lock_guard<std::mutex> lock (maintanceMutex());
    CBCoroStatic* prev = deleteQueue(); if (prev != nullptr) delete prev;
    deleteQueue() = cbco;
  }
  virtual ~CBCoroStatic() {}
 public:
  static void maintance() {
    deleteLater (nullptr);
  }
};

/** Simplifies turning callback control flows into normal imperative control flows. */
template <int STACK_SIZE>
class CBCoro: public CBCoroStatic {
 protected:
  ucontext_t _context;
  char _stack[STACK_SIZE];
  CBCoro() {
    printf ("CBCoro constructor\n");
    if (getcontext (&_context)) GTHROW ("!getcontext");
    _context.uc_stack.ss_sp = _stack;
    _context.uc_stack.ss_size = STACK_SIZE;
    #pragma GCC diagnostic ignored "-Wunused-value"
    VALGRIND_STACK_REGISTER (_stack, _stack + STACK_SIZE);
  }
  virtual ~CBCoro() {
    printf ("CBCoro destructor\n");
    VALGRIND_STACK_DEREGISTER (_stack);
  }
  /** Starts the coroutine on the `_stack` (makecontext, swapcontext). */
  void cbcStart() {
    ucontext_t back; _context.uc_link = &back;
    printf ("cbcStart; invoking cbcRun with cbCoro: %li\n", (intptr_t) this);
    makecontext (&_context, (void(*)()) cbcRun, 1, (intptr_t) this);
    swapcontext (&back, &_context);
    printf ("cbcStart; returned from cbcRun\n");
  }
  static void cbcRun (CBCoro* cbCoro) {
    printf ("cbcRun; cbCoro: %li\n", (intptr_t) cbCoro);
    cbCoro->run();
  }
  virtual void run() = 0;
};

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
  bool _fromCB = false;
  ucontext_t* _returnTo = nullptr;
  RemoveFroples (const char* argument): _argument (argument) {
    printf ("RF: constructor\n");
    cbcStart();
  }
  virtual void run() override {
    printf ("RF: run\n");
    for (int i = 1; i <= 4; ++i) {
      printf ("RF: Removing frople %i...\n", i);
      _fromCB = false;
      if (getcontext (&_context)) GTHROW ("!getcontext"); // Capture.
      if (!_fromCB) { // If still in the present.
        esDelete (i, [this](int frople) {
          printf ("RF,CB: frople %i; resuming RemoveFroples.\n", frople);
          ucontext_t cbContext; _returnTo = &cbContext; _fromCB = true;
          if (swapcontext (&cbContext, &_context)) GTHROW ("!swapcontext");
        });
        if (_returnTo != nullptr) setcontext (_returnTo); else return;
      }
      printf ("RF: Returned from callback\n");
    }
    deleteLater (this);
    if (_returnTo != nullptr) setcontext (_returnTo); else return;
  };
};

int main() {
  new RemoveFroples ("argument");
  printf ("main: returned from RemoveFroples\n");
  sleep (5);
  CBCoroStatic::maintance(); // Perform any pending cleanups.
  return 0;
}