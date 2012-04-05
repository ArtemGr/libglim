#ifndef _NSEC_TIMER_H
#define _NSEC_TIMER_H

#include <time.h> // clock_gettime, CLOCK_MONOTONIC
#include <stdint.h>
#include <string>
#include <sstream>
#include <iomanip>

namespace glim {

//! Safe nanoseconds timer.
struct NsecTimer {
  timespec start;
  NsecTimer () {restart();}
  //! Nanoseconds since the creation or restart of the timer.
  int64_t operator()() const {
    timespec nsecStop; clock_gettime (CLOCK_MONOTONIC, &nsecStop);
    return (int64_t) (nsecStop.tv_sec - start.tv_sec) * 1000000000LL + (int64_t) (nsecStop.tv_nsec - start.tv_nsec);
  }
  //! Seconds since the creation or restart of the timer.
  //! Formatted with `std::ios::fixed`.
  std::string seconds () const {
    double fv = operator()() / 1000000000.0;
    std::ostringstream buf;
    buf << std::setiosflags (std::ios::fixed) << fv;
    return buf.str();
  }
  //! Seconds since the creation or restart of the timer.
  std::string seconds (int precision) const {
    int co1 = 1000000000; int co2 = 1; while (precision--) {co1 /= 10; co2 *= 10;}
    double fv = (double) ((int) ((double) operator()() / co1)) / co2;
    std::ostringstream buf;
    buf << fv;
    return buf.str();
  }
  void restart() {clock_gettime (CLOCK_MONOTONIC, &start);}
  int64_t getAndRestart() {int64_t tmp = operator()(); restart(); return tmp;}
};

}

#endif // _NSEC_TIMER_H
