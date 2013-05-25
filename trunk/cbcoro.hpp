// http://en.wikipedia.org/wiki/Setcontext; man 3 makecontext; man 2 getcontext
// http://www.boost.org/doc/libs/1_53_0/libs/context/doc/html/index.html
// g++ -std=c++11 -O1 -Wall -g test_cbcoro.cc -pthread && ./a.out

#include <ucontext.h>
#include <valgrind/valgrind.h>
#include <glim/exception.hpp>
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
  ucontext_t* _returnTo;
  bool _invokedFromYield; ///< True if `invokeFromCallback` was called directly from `yieldForCallback`.
  char _stack[STACK_SIZE];
  CBCoro(): _returnTo (nullptr), _invokedFromYield (false) {
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
    // Since we have to "return" from inside the `yieldForCallback`,
    // we're not actually using the `_context.uc_link` and `return`, we use `setcontext (_returnTo)` instead.
    _returnTo = &back;
    swapcontext (&back, &_context);
    printf ("cbcStart; returned from cbcRun\n");
  }
  static void cbcRun (CBCoro* cbCoro) {
    printf ("cbcRun; cbCoro: %li\n", (intptr_t) cbCoro);
    cbCoro->run();
  }
  virtual void run() = 0;
  /** Captures the stack, runs the `fun` and relinquish the control to `_returnTo`.\n
   * This method will never "return" by itself, in order for it to "return" the
   * `fun` MUST call `invokeFromCallback`, maybe later and from a different stack. */
  template <typename F> void yieldForCallback (F fun) {
    printf ("yieldForCallback; %i\n", __LINE__);
    fun();
  }
};
