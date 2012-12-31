#define _GLIM_EXCEPTIONS_CODE
#define _GLIM_ALL_EXCEPTIONS_CODE
#include "exception.hpp"
#include <iostream>
#include <typeinfo>
#include <assert.h>

static void testThrowLine() {
  int line = 0; std::string message; try {
    line = __LINE__; GTHROW ("message");
  } catch (const std::exception& ex) {
    message = ex.what();
  }
  //std::cout << message << ' ' << std::flush;
  assert (message.size());
  assert (std::string (message) .find (":" + std::to_string (line)) != std::string::npos);

  line = 0; message.clear(); std::string name; try {
    line = __LINE__; G_DEFINE_EXCEPTION (FooEx); GNTHROW (FooEx, "foo");
  } catch (const std::exception& ex) {
    message = ex.what(); name = typeid (ex) .name();
  }
  //std::cout << message << ' ' << name << ' ' << std::flush;
  assert (message.size());
  assert (std::string (message) .find (":" + std::to_string (line)) != std::string::npos);
  assert (name.find ("FooEx") != std::string::npos);

  message.clear(); try {
    glim::ExceptionControl plainWhat (glim::Exception::Options::PLAIN_WHAT);
    GTHROW ("bar");
  } catch (const std::exception& ex) {
    message = ex.what();
  }
  assert (message == "bar");
}

static void testAllExceptionsHack() {
  std::string traceBuf;
  glim::ExceptionHandler traceExceptions (glim::Exception::Options::HANDLE_ALL, glim::ExceptionHandler::backtrace, &traceBuf);
  try {
    throw "catch me"; // Catched by `_GLIM_ALL_EXCEPTIONS_CODE` and handled with `glim::ExceptionControl::backtrace`.
  } catch (const char* skip) {}
  std::cout << std::endl << traceBuf << std::endl;
  assert (traceBuf.size());
}

int main () {
  std::cout << "Testing exception.hpp ... " << std::flush;
  testThrowLine();
  testAllExceptionsHack();
  std::cout << "pass." << std::endl;
  return 0;
}
