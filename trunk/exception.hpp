#ifndef _GLIM_EXCEPTION_HPP_INCLUDED
#define _GLIM_EXCEPTION_HPP_INCLUDED

#include <stdexcept>
#include <string>
#include <stdint.h>
#include <unistd.h> // write

#if defined(__GNUC__)
# include <execinfo.h> // backtrace; http://www.gnu.org/software/libc/manual/html_node/Backtraces.html
#endif

/** Throws `::glim::Exception` passing the current file and line into constructor. */
#define GTHROW(message) throw ::glim::Exception (message, __FILE__, __LINE__)
/** Throws a `::glim::Exception` derived exception `name` passing the current file and line into constructor. */
#define GNTHROW(name, message) throw name (message, __FILE__, __LINE__)
/** Helps defining new `::glim::Exception`-based exceptions.\n
 * Named exceptions might be useful in a debugger. */
#define G_DEFINE_EXCEPTION(name) \
  struct name: public ::glim::Exception { \
    name (const ::std::string& message, const char* file, int line): ::glim::Exception (message, file, line) {} \
  }

namespace glim {

// Ideas:
// RAII control via thread-local integer (with bits): option to capture stack trace (printed on "what()")
// see http://stacktrace.svn.sourceforge.net/viewvc/stacktrace/stacktrace/call_stack_gcc.cpp?revision=40&view=markup
// A handler to log exception with VALGRIND (with optional trace)
// A handler to log thread id and *pause* the thread in exception constructor (user can attach GDB and investigate)
// (or we might call an empty function: "I once used something similar,
//  but with an empty function debug_breakpoint. When debugging, I simply entered "bre debug_breakpoint"
//  at the gdb prompt - no asembler needed (compile debug_breakpoint in a separate compilation unit to avoid having the call optimized away)."
//  - http://stackoverflow.com/a/4720403/257568)
// A handler to call a debugger? (see: http://stackoverflow.com/a/4732119/257568)

// todo: A helper converting backtrace to addr2line invocation, e.g.
// bin/test_exception() [0x4020cc];bin/test_exception(__cxa_throw+0x47) [0x402277];bin/test_exception() [0x401c06];/lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xfd) [0x57f0ead];bin/test_exception() [0x401fd1];
// should be converted to
// addr2line -pifCa -e bin/test_exception 0x4020cc 0x402277 0x401c06 0x57f0ead 0x401fd1

/** If `stdStringPtr` is not null then backtrace is saved there (must point to an std::string instance),
 * otherwise printed to write(2). */
inline void captureBacktrace (void* stdStringPtr) {
#if defined(__GNUC__) || defined (_EXECINFO_H) || defined (_EXECINFO_H_)
  const int arraySize = 10; void *array[arraySize];
  int got = ::backtrace (array, arraySize);
  if (stdStringPtr) {
    std::string* out = (std::string*) stdStringPtr;
    char **strings = ::backtrace_symbols (array, got);
    for (int tn = 0; tn < got; ++tn) {out->append (strings[tn]); out->append (1, ';');}
    ::free (strings);
  } else ::backtrace_symbols_fd (array, got, 2);
#else
# warning captureBacktrace: I do not know how to capture backtrace there. Patches welcome.
#endif
}

class Exception: public std::runtime_error {
 protected:
  const char* _file; int32_t _line;
  std::string _what;
  uint32_t _options;

  /** Append [{file}:{line}] into `buf`. */
  void appendLine (std::string& buf) const {
    if (_file || _line > 0) {
      buf.append (1, '[');
      if (_file) buf.append (_file);
      if (_line >= 0) buf.append (1, ':') .append (std::to_string (_line));
      buf.append ("] ");
    }
  }

  /** Append a stack trace to `_what`. */
  void capture() {
    if (options() & CAPTURE_TRACE) {
      appendLine (_what);
      _what += "[at ";
      captureBacktrace (&_what);
      _what.append ("] ");
      _what += std::runtime_error::what();
    }
  }
 public:
  /** The reference to the thread-local options. */
  inline static uint32_t& options() {
    static __thread uint32_t OPTIONS = 0;
    return OPTIONS;
  }
  enum Options: uint32_t {
    PLAIN_WHAT = 1, ///< Pass `what` as is, do not add any information to it.
    HANDLE_ALL = 1 << 1, ///< Run the custom handler from `__cxa_throw`.
    CAPTURE_TRACE = 1 << 2 ///< Append a stack trace into the `Exception::_what` (with the help of the `captureBacktrace`).
  };

  typedef void (*handler_fn)(void*);
  /** The pointer to the thread-local exception handler. */
  inline static handler_fn* handler() {
    static __thread handler_fn HANDLER = nullptr;
    return &HANDLER;
  }
  /** The pointer to the thread-local argument for the exception handler. */
  inline static void** handlerArg() {
    static __thread void* HANDLER_ARG = nullptr;
    return &HANDLER_ARG;
  }

  Exception (const std::string& message):
    std::runtime_error (message), _file (0), _line (-1), _options (options()) {
    capture();}
  Exception (const std::string& message, const char* file, int32_t line):
    std::runtime_error (message), _file (file), _line (line), _options (options()) {
    capture();}
  virtual const char* what() const throw() {
    if (_options & PLAIN_WHAT) return std::runtime_error::what();
    std::string& buf = const_cast<std::string&> (_what);
    if (buf.empty()) {
      appendLine (buf);
      buf.append (std::runtime_error::what());
    }
    return buf.c_str();
  }
};

/** RAII control of thrown `Exception`s.\n
 * Modifies the `Exception` options via a thread-local variable and restores them back upon destruction.\n
 * Currently uses http://gcc.gnu.org/onlinedocs/gcc-4.7.2/gcc/Thread_002dLocal.html
 * (might use C++11 `thread_local` in the future). */
class ExceptionControl {
 protected:
  uint32_t _savedOptions;
 public:
  ExceptionControl (Exception::Options newOptions) {
    uint32_t& options = Exception::options();
    _savedOptions = options;
    options = newOptions;
  }
  ~ExceptionControl() {
    Exception::options() = _savedOptions;
  }
};

class ExceptionHandler {
protected:
 uint32_t _savedOptions;
 Exception::handler_fn _savedHandler;
 void* _savedHandlerArg;
public:
 ExceptionHandler (Exception::Options newOptions, Exception::handler_fn handler, void* handlerArg) {
   _savedOptions = Exception::options(); Exception::options() = newOptions;
   _savedHandler = *Exception::handler(); *Exception::handler() = handler;
   _savedHandlerArg = *Exception::handlerArg(); *Exception::handlerArg() = handlerArg;
 }
 ~ExceptionHandler() {
   Exception::options() = _savedOptions;
   *Exception::handler() = _savedHandler;
   *Exception::handlerArg() = _savedHandlerArg;
 }
};

} // namespace glim

#endif // _GLIM_EXCEPTION_HPP_INCLUDED

/**
 * Special handler for ALL exceptions. Usage:
 * 1) In the `main` module inject this code with:
 *   #define _GLIM_ALL_EXCEPTIONS_CODE
 *   #include <glim/exception.hpp>
 * 2) Link with "-ldl" (for `dlsym`).
 * 3) Use the ExceptionHandler to enable special behaviour in the current thread:
 *   glim::ExceptionHandler traceExceptions (glim::Exception::Options::HANDLE_ALL, glim::captureBacktrace, nullptr);
 *
 * About handing all exceptions see:
 *   http://stackoverflow.com/a/11674810/257568
 *   http://blog.sjinks.pro/c-cpp/969-track-uncaught-exceptions/
 */
#ifdef _GLIM_ALL_EXCEPTIONS_CODE

#include <dlfcn.h> // dlsym

typedef void(*cxa_throw_type)(void*, void*, void(*)(void*)); // Tested with GCC 4.7.
static cxa_throw_type NATIVE_CXA_THROW = 0;

extern "C" void __cxa_throw (void* thrown_exception, void* tinfo, void (*dest)(void*)) {
  if (!NATIVE_CXA_THROW) NATIVE_CXA_THROW = reinterpret_cast<cxa_throw_type> (::dlsym (RTLD_NEXT, "__cxa_throw"));
  if (!NATIVE_CXA_THROW) ::std::terminate();

  using namespace glim;
  uint32_t options = Exception::options();
  if (options & Exception::Options::HANDLE_ALL) {
    Exception::handler_fn handler = *Exception::handler();
    if (handler) handler (*Exception::handlerArg());
  }

  NATIVE_CXA_THROW (thrown_exception, tinfo, dest);
}

#endif // _GLIM_ALL_EXCEPTIONS_CODE
