#ifndef _RUNNER_INCLUDED
#define _RUNNER_INCLUDED

#include <functional>
#include <thread>
#include <condition_variable>
#include <chrono>

#include <curl/curl.h>

namespace glim {

/** Uses a separate thread to run CURLM requests and completion handlers, as well as other periodic jobs. */
class Runner {
  struct FreeCurl { ///< Free CURL during stack unwinding.
    Runner* runner; CURL* curl;
    FreeCurl (Runner* scheduler, CURL* curl): runner (scheduler), curl (curl) {}
    ~FreeCurl() {
      runner->_handlers.erase (curl);
      curl_multi_remove_handle (runner->_curlm, curl);
      curl_easy_cleanup (curl);
    }
  };
protected:
  typedef std::function<void(CURLMsg*)> handler_t;
  /** Locked when the thread isn't sleeping/waiting. */
  std::mutex _workMutex;
  std::map<CURL*, handler_t> _handlers;
  /** Functions to run periodically. */
  std::map<glim::gstring, std::function<void()>> _jobs;
  CURLM* _curlm = curl_multi_init();
  /** CURL's number of handlers waiting to be processed. */
  int _running_handles = 0;
  /** `false` tells the thread that Scheduler is destructed. */
  bool* _active = new bool (true);
  std::thread _thread;
  /** Conditional variables require protection. */
  std::mutex _alarmMutex;
  /** Rings when there is a new handler. */
  std::condition_variable _alarm;
  static void staticRun (Runner* scheduler) {scheduler->run();}
  void run() {
    bool* schedulerIsNotDestroyed = _active; // The pointer is valid even after Scheduler destruction.
    while (*schedulerIsNotDestroyed) {
      bool sleep = false;
      if (_running_handles) { // Wait for file descriptor activity, cf. http://curl.haxx.se/libcurl/c/multi-double.html
        int maxfd = -1; fd_set fdread; FD_ZERO(&fdread); fd_set fdwrite; FD_ZERO(&fdwrite); fd_set fdexcep; FD_ZERO(&fdexcep);
        struct timeval timeout; timeout.tv_sec = 0; timeout.tv_usec = _pauseMs * 1000; // Shouldn't be too big or we'll miss the new requests.
        std::unique_lock<std::mutex> workLock (_workMutex);
        //long curl_timeo = -1; curl_multi_timeout (_curlm, &curl_timeo);
        //if (curl_timeo >= 0) {timeout.tv_sec = curl_timeo / 1000;
        //  if (timeout.tv_sec > 1) timeout.tv_sec = 1; else timeout.tv_usec = (curl_timeo % 1000) * 1000;}
        CURLMcode fdset_rc = curl_multi_fdset (_curlm, &fdread, &fdwrite, &fdexcep, &maxfd);
        if (fdset_rc != CURLM_OK) {
          std::cerr << "Scheduler: curl_multi_fdset: " << fdset_rc; sleep = true;
        } else {
          int select_rc = select (maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);
          if (select_rc == -1) {std::cerr << "Scheduler: select: -1"; sleep = true;}
        }
      } else sleep = true;
      if (sleep) { // Wait for new handles.
        std::unique_lock<std::mutex> alarmLock (_alarmMutex);
        _alarm.wait_for (alarmLock, std::chrono::milliseconds (_pauseMs));
      }
      if (!*schedulerIsNotDestroyed) break;
      std::unique_lock<std::mutex> workLock (_workMutex);
      int hnum = _running_handles;
      curl_multi_perform (_curlm, &_running_handles);
      if (hnum != _running_handles) {
        nextMessage:
          int msgs_in_queue;
          CURLMsg* msg = curl_multi_info_read (_curlm, &msgs_in_queue);
          if (msg) try {
            auto curl = msg->easy_handle;
            FreeCurl freeCurl (this, curl);
            auto it = _handlers.find (curl);
            if (it != _handlers.end()) it->second (msg);
            if (msgs_in_queue > 0) goto nextMessage;
          } catch (const std::exception& ex) {
            std::cerr << "Scheduler, handler: " << ex.what();
          }
      }
      // Run non-CURL jobs.
      for (auto it = _jobs.begin(), end = _jobs.end(); it != end; ++it) try {
        it->second ();
      } catch (const std::exception& ex) {
        std::cerr << "Runner: error in job " << it->first << ": " << ex.what();
      }
    }
    delete schedulerIsNotDestroyed;
  }
public:
  unsigned _pauseMs = 100;
  Runner(): _thread (Runner::staticRun, this) {}
  /** Wait for the operation to complete, then call the `handler`, then free the `curl`. */
  void multi (CURL* curl, handler_t handler) {
    std::unique_lock<std::mutex> workLock (_workMutex);
    curl_multi_add_handle (_curlm, curl);
    _handlers[curl] = handler;
    workLock.unlock();
    std::unique_lock<std::mutex> alarmLock (_alarmMutex); // NB: Must be locked here or the alarm won't work.
    _alarm.notify_all();
  }
  /** Register a new job to be run on the thread loop. */
  std::function<void()>& job (const glim::gstring& name) {
    std::unique_lock<std::mutex> workLock (_workMutex);
    return _jobs[name];
  }
  void removeJob (const glim::gstring& name) {
    std::unique_lock<std::mutex> workLock (_workMutex);
    _jobs.erase (name);
  }
  ~Runner() {
    *_active = false;
    _alarm.notify_all();
    std::unique_lock<std::mutex> lock (_workMutex);
    for (auto it = _handlers.begin(), end = _handlers.end(); it != end; ++it) {
      CURL* curl = it->first;
      curl_multi_remove_handle (_curlm, curl);
      curl_easy_cleanup (curl);
    }
    if (_curlm) {curl_multi_cleanup (_curlm); _curlm = NULL;}
  }
};

} // namespace glim

#endif // _RUNNER_INCLUDED
