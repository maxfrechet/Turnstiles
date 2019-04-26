#ifndef SRC_TURNSTILE_H_
#define SRC_TURNSTILE_H_

#include <condition_variable>
#include <mutex>
#include <queue>
#include <type_traits>

class Turnstile {
 public:
  std::mutex mutex;
  std::condition_variable cv;
  size_t waiting;
  bool is_ready;
  Turnstile();
};

class Mutex {
 public:
  Turnstile *turnstile;
  Mutex();
  Mutex(const Mutex &) = delete;

  void lock();    // NOLINT
  void unlock();  // NOLINT
};

#endif  // SRC_TURNSTILE_H_
