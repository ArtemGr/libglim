#include <functional>
namespace glim {

// http://stackoverflow.com/questions/2121607/any-raii-template-in-boost-or-c0x/

/// RAII helper. Keeps the functor and runs it in the destructor.
/// The idea to name it `finally` comes from http://www.codeproject.com/Tips/476970/finally-clause-in-Cplusplus.
/// Example: \code
///   finally unmap ([&]() {munmap (fd, size);});
///   // or
///   auto unmap = raiiFun ([&]() {munmap (fd, size);});
/// \endcode
template<typename Fun> struct finally {
  Fun _fun;
  finally (finally&&) = default;
  finally (const finally&) = default;
  template<typename FunArg> finally (FunArg fun): _fun (std::forward<Fun> (fun)) {}
  ~finally() {_fun();}
};

/// Runs the given functor when going out of scope.
/// Example: \code
///   auto closeFd = raiiFun ([&]() {close (fd);});
///   auto unmap = raiiFun ([&]() {munmap (fd, size);});
/// \endcode
template<typename Fun> finally<Fun> raiiFun (const Fun& fun) {return finally<Fun> (fun);}

/// Runs the given functor when going out of scope.
/// Example: \code
///   auto closeFd = raiiFun ([&]() {close (fd);});
///   auto unmap = raiiFun ([&]() {munmap (fd, size);});
/// \endcode
template<typename Fun> finally<Fun> raiiFun (Fun&& fun) {return finally<Fun> (std::move (fun));}

}
