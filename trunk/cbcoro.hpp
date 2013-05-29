// http://en.wikipedia.org/wiki/Setcontext; man 3 makecontext; man 2 getcontext
// http://www.boost.org/doc/libs/1_53_0/libs/context/doc/html/index.html
// g++ -std=c++11 -O1 -Wall -g test_cbcoro.cc -pthread && ./a.out

#include <ucontext.h>
#include <valgrind/valgrind.h>
#include <glim/exception.hpp>
#include <mutex>
#include <memory>

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
    // TODO: Use an intrusive list (right in CBCoroStatic) with a configurable timeout.
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
  /** Set in `invokeFromCallback`.\n
   * `void` should be safe because of the captured deteler, see http://stackoverflow.com/questions/11624131/casting-shared-ptrt-to-shared-ptrvoid */
  std::shared_ptr<void> _valueFromCB;
  bool _invokeFromYield; ///< True if `invokeFromCallback` was called directly from `yieldForCallback`.
  bool _yieldFromInvoke; ///< True if `yieldForCallback` now runs from `invokeFromCallback`.
  char _stack[STACK_SIZE];
  CBCoro(): _returnTo (nullptr), _invokeFromYield (false), _yieldFromInvoke (false) {
    if (getcontext (&_context)) GTHROW ("!getcontext");
    // TODO: Allocate and free the stack in separate virtual methods, in order to make the allocation extendable.
    // TODO: Allocate the stack with mmap or memalign, making the first and last pages PROT_NONE with mprotect,
    //       in order to simplify stack overflow and underflow detection.
    //       cf. http://man7.org/linux/man-pages/man2/mprotect.2.html
    //       cf. http://man7.org/linux/man-pages/man3/posix_memalign.3.html
    //       Even a simple malloc will make the over/underflows detectable by valgrind & co.
    _context.uc_stack.ss_sp = _stack;
    _context.uc_stack.ss_size = STACK_SIZE;
    #pragma GCC diagnostic ignored "-Wunused-value"
    VALGRIND_STACK_REGISTER (_stack, _stack + STACK_SIZE);
  }
  virtual ~CBCoro() {
    VALGRIND_STACK_DEREGISTER (_stack);
  }
  /** Starts the coroutine on the `_stack` (makecontext, swapcontext), calling the `run`. */
  void cbcStart() {
    ucontext_t back; _context.uc_link = &back;
    makecontext (&_context, (void(*)()) cbcRun, 1, (intptr_t) this);
    // Since we have to "return" from inside the `yieldForCallback`,
    // we're not actually using the `_context.uc_link` and `return`, we use `setcontext (_returnTo)` instead.
    _returnTo = &back;
    swapcontext (&back, &_context); // Now our stack lives and the caller stack is no longer in control.
  }
  static void cbcRun (CBCoro* cbCoro) {
    cbCoro->run();
  }
  /** Relinquish the control to the original owner of the thread, restoring its stack. */
  void cbcReturn() {
    ucontext_t* returnTo = _returnTo;
    if (returnTo != nullptr) {_returnTo = nullptr; setcontext (returnTo);}
  }
  /** Must call `cbcReturn` instead of `return`. */
  virtual void run() throw() = 0;

  /** Captures the stack, runs the `fun` and relinquish the control to `_returnTo`.\n
   * This method will never "return" by itself, in order for it to "return" the
   * `fun` MUST call `invokeFromCallback`, maybe later and from a different stack. */
  template <typename F> void yieldForCallback (F fun) {
    _yieldFromInvoke = false;
    if (getcontext (&_context)) GTHROW ("!getcontext"); // Capture.
    if (_yieldFromInvoke) {
      // We're now in the future, revived by the `invokeFromCallback`.
      // All we do now is "return" to the caller whose stack we captured earlier.
    } else {
      // We're still in the present, still have some work to do.
      fun(); // The `fun` is supposed to do something resulting in the `invokeFromCallback` being called later.
      if (_invokeFromYield) {
        // The `fun` used the `invokeFromCallback` directly, not resorting to callbacks, meaning we don't have to do our magick.
        _invokeFromYield = false;
      } else {
        // So, the `fun` took measures to revive us later, it's time for us to go into torpor and return the control to whoever we've borrowed it from.
        cbcReturn();
      }
    }
  }
  template <typename R> std::shared_ptr<R> valueFromCB() const {
    return std::static_pointer_cast<R> (_valueFromCB);
  }
 public:
  /** To be called from a callback in order to lend the control to CBCoro, continuing it from where it called `yieldForCallback`. */
  template <typename R> void invokeFromCallback (const std::shared_ptr<R>& ret) {
    _valueFromCB = std::static_pointer_cast<void> (ret);
    if (_returnTo != nullptr) {
      // We have not yet "returned" from the `yieldForCallback`,
      // meaning that the `invokeFromCallback` was executed immediately from inside the `yieldForCallback`.
      // In that case we must DO NOTHING, we must simply continue running on the current stack.
      _invokeFromYield = true; // Tells `yieldForCallback` to do nothing.
    } else {
      // Revive the CBCoro, letting it to continue from where it was suspended in `yieldForCallback`.
      ucontext_t cbContext; _returnTo = &cbContext; _yieldFromInvoke = true;
      if (swapcontext (&cbContext, &_context)) GTHROW ("!swapcontext");
      if (_returnTo == &cbContext) _returnTo = nullptr;
    }
  }
  /** To be called from a callback in order to lend the control to CBCoro, continuing it from where it called `yieldForCallback`. */
  void invokeFromCallback() {invokeFromCallback (std::shared_ptr<void>());}
};
