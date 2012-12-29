#ifndef _GLIM_EXCEPTION_HPP_INCLUDED
#define _GLIM_EXCEPTION_HPP_INCLUDED

#include <stdexcept>
#include <string>
#include <stdint.h>

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

// Option to stack-trace *all* exceptions? http://stackoverflow.com/a/11674810/257568
// Ideas:
// RAII control via thread-local integer (with bits): option to capture stack trace (printed on "what()")
// see http://stacktrace.svn.sourceforge.net/viewvc/stacktrace/stacktrace/call_stack_gcc.cpp?revision=40&view=markup
// RAII bit: option to apply addr2line (applied from "what()")
// http://stackoverflow.com/a/4611112/257568
// RAII bit: option to *log* an exception when it is created
// global log handler (std::function?) *and* RAII thread-local handler (C function pointer) in case one needs to override
// RAII option to log exception with VALGRIND (with optional trace)
// RAII option to do a user-specific action upon exception (C function pointer)
// RAII option to log thread id and *pause* the thread in exception constructor (user can attach GDB and investigate)
// (or we might call an empty function: "I once used something similar,
//  but with an empty function debug_breakpoint. When debugging, I simply entered "bre debug_breakpoint"
//  at the gdb prompt - no asembler needed (compile debug_breakpoint in a separate compilation unit to avoid having the call optimized away)."
//  - http://stackoverflow.com/a/4720403/257568)
// RAII option to call a debugger? (see: http://stackoverflow.com/a/4732119/257568)

class Exception: public std::runtime_error {
 protected:
  const char* _file; int32_t _line;
  std::string _what;
  uint32_t _options;
 public:
  /** The reference to the thread-local options. */
  inline static uint32_t& options() {
    static __thread uint32_t OPTIONS = 0;
    return OPTIONS;
  }
  enum Options: uint32_t {
    PLAIN_WHAT = 1 ///< Pass `what` as is, do not add any information to it.
  };

  Exception (const std::string& message): std::runtime_error (message), _file (0), _line (-1), _options (options()) {}
  Exception (const std::string& message, const char* file, int32_t line): std::runtime_error (message), _file (file), _line (line), _options (options()) {}
  virtual const char* what() const throw() {
    if (_options && PLAIN_WHAT) return std::runtime_error::what();
    std::string& buf = const_cast<std::string&> (_what);
    if (buf.empty()) {
      if (_file || _line > 0) {
        buf.append (1, '[');
        if (_file) buf.append (_file);
        if (_line >= 0) buf.append (1, ':') .append (std::to_string (_line));
        buf.append ("] ");
      }
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

} // namespace glim

#endif // _GLIM_EXCEPTION_HPP_INCLUDED
