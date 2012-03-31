#ifndef _NSEC_TIMER_H
#define _NSEC_TIMER_H

#include <time.h> // clock_gettime, CLOCK_MONOTONIC

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
  void restart() {clock_gettime (CLOCK_MONOTONIC, &start);}
  int64_t getAndRestart() {int64_t tmp = operator()(); restart(); return tmp;}
};

}

#endif // _NSEC_TIMER_H
