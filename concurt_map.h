//
// Created by Shihao Jing on 11/9/17.
//

#ifndef PARALLEL_CONCURRENCY_CONCURT_MAP_H
#define PARALLEL_CONCURRENCY_CONCURT_MAP_H

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <random>

template <typename T, typename ProbFunc>
class concurt_map {
  T **primary_hashtable;
  T **old_hashtable;
  std::mutex locks[hashsize(LOCK_POWER)];
  size_t lock_cap;
  size_t cap;
  size_t size_;
  ProbFunc probFunc;

 public:
  explicit concurt_map(unsigned hashpower) : primary_hashtable(new T*[hashsize(hashpower)]), old_hashtable(nullptr),
                                             cap(hashsize(hashpower)), lock_cap(hashsize(LOCK_POWER)),
                                             size_(0), probFunc()
  {
    for (int i = 0; i < cap; ++i)
      primary_hashtable[i] = nullptr;
  }

  bool add(T key) {
    const void *kp = &key;
    // TODO fix: length of key may not be sizeof(key)
    int len = sizeof(key);

    int iter = 0;
    uint32_t hv = 0;
    hash(kp, len, hashseed, &hv);

    uint32_t next_hv = probFunc(hv,iter++, kp, len);
    locks[next_hv & hashmask(lock_cap)].lock();

    while (primary_hashtable[next_hv & hashmask(cap)] != nullptr) {
      if (*(primary_hashtable[next_hv & hashmask(cap)]) == key) {
        locks[next_hv & hashmask(lock_cap)].unlock();
        return false;
      }
      locks[next_hv & hashmask(lock_cap)].unlock();
      next_hv = probFunc(hv,iter++, kp, len);
      locks[next_hv & hashmask(lock_cap)].lock();
    }
    ++size_;
    primary_hashtable[next_hv & hashmask(cap)] = new T(key);
    locks[next_hv & hashmask(lock_cap)].unlock();
    return true;
  }

  bool remove(T key) {
    const void *kp = &key;
    // TODO fix: length of key may not be sizeof(key)
    int len = sizeof(key);

    int iter = 0;
    uint32_t hv = 0;
    hash(kp, len, hashseed, &hv);

    uint32_t next_hv = probFunc(hv,iter++, kp, len);
    locks[next_hv & hashmask(lock_cap)].lock();

    while (primary_hashtable[next_hv & hashmask(cap)] != nullptr) {
      if (*(primary_hashtable[next_hv & hashmask(cap)]) == key) {
        --size_;
        primary_hashtable[next_hv & hashmask(cap)] = nullptr;
        locks[next_hv & hashmask(lock_cap)].unlock();
        return true;
      }
      locks[next_hv & hashmask(lock_cap)].unlock();
      next_hv = probFunc(hv,iter++, kp, len);
      locks[next_hv & hashmask(lock_cap)].lock();
    }
    locks[next_hv & hashmask(lock_cap)].unlock();
    return false;
  }

  size_t size() {
    size_t ret = 0;
    for (int i = 0; i < cap; ++i) {
      if (primary_hashtable[i] != nullptr)
        ++ret;
    }
    return ret;
  }

  void populate(unsigned range) {
    std::random_device r;
    std::default_random_engine e(r());
    std::uniform_int_distribution<int> uniform_dist(0, range);
    for (int i = 0; i < 1024; ++i) {
      add(uniform_dist(e));
    }
  }
};

#endif //PARALLEL_CONCURRENCY_CONCURT_MAP_H
