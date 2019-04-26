#include "turnstile.h"
#include <iostream>

static constexpr int N = 431;

std::mutex mutexes[N];
std::mutex safety;

Turnstile::Turnstile() {
  is_ready = false;
  waiting = 0;
}

namespace Pool {
// "Construct on first use idiom"
std::queue<Turnstile*>& queue() {
  static auto* queue = new std::queue<Turnstile*>();
  return *queue;
}

static char trap = 't';
static volatile size_t pool_size = 0;
static volatile bool first = true;

void resize_pool() {
  if (first) {
    pool_size = 40;
    for (size_t i = 0; i < pool_size; i++) {
      queue().push(new Turnstile);
    }
    first = false;
    return;
  }

  for (size_t i = 0; i < pool_size; i++) {
    Turnstile t;
    queue().push(new Turnstile);
  }
  pool_size *= 2;
}

void reduce_pool() {
  for (size_t i = 0; i < queue().size() / 2; i++) {
    queue().pop();
  }
  pool_size = queue().size();
}
}  // namespace Pool

Mutex::Mutex() { turnstile = nullptr; }

void Mutex::lock() {
  auto id = reinterpret_cast<uintptr_t>(this) % N;
  mutexes[id].lock();

  if (turnstile == nullptr) {
    // which means it is the first thread
    turnstile = reinterpret_cast<Turnstile*>(&(Pool::trap));
    mutexes[id].unlock();
    return;
  } else if (turnstile == reinterpret_cast<Turnstile*>(&(Pool::trap))) {
    safety.lock();
    if (Pool::queue().empty()) {
      Pool::resize_pool();
    }
    turnstile = Pool::queue().front();
    Pool::queue().pop();
    safety.unlock();
  }
  (turnstile->waiting)++;

  mutexes[id].unlock();

  std::unique_lock<std::mutex> lk((turnstile->mutex));
  (turnstile->cv).wait(lk, [this] { return turnstile->is_ready; });

  mutexes[id].lock();

  (turnstile->waiting)--;
  turnstile->is_ready = false;

  mutexes[id].unlock();
}

void Mutex::unlock() {
  auto id = reinterpret_cast<uintptr_t>(this) % N;
  mutexes[id].lock();

  if (turnstile == reinterpret_cast<Turnstile*>(&Pool::trap)) {  // trap
    turnstile = nullptr;
    mutexes[id].unlock();
    return;
  }
  
  if (turnstile->waiting > 0) {
    turnstile->is_ready = true;
    (turnstile->cv).notify_one();
    mutexes[id].unlock();
  } else {
    safety.lock();
    if (Pool::queue().size() > (3 / 4 * Pool::pool_size)) {
      Pool::reduce_pool();
    }
    Pool::queue().push(turnstile);
    safety.unlock();

    turnstile = nullptr;
    mutexes[id].unlock();
  }
}
