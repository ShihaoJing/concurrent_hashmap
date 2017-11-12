//
// Created by Shihao Jing on 11/10/17.
//

#ifndef PARALLEL_CONCURRENCY_TRANSCT_MAP_H
#define PARALLEL_CONCURRENCY_TRANSCT_MAP_H

template <typename T, typename ProbFunc>
class transct_map {
  T **primary_hashtable;
  T **old_hashtable;
  size_t cap;
  size_t size_;
  double threashhold = 0.8;
  ProbFunc probFunc;

 public:
  explicit transct_map(unsigned hashpower) : primary_hashtable(new T*[hashsize(hashpower)]),
                                         old_hashtable(nullptr),
                                         cap(hashsize(hashpower)),
                                         size_(0),
                                         probFunc()
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
    size_t idx = probFunc(hv,iter++, kp, len) & hashmask(cap);

    __transaction_relaxed {
      while (primary_hashtable[idx] != nullptr) {
        if (*(primary_hashtable[idx]) == key)
          return false;
        //idx = (hv + iter * iter) & hashmask(cap);
        // ++iter;
        idx = probFunc(hv,iter++, kp, len) & hashmask(cap);
      }

      ++size_;
      primary_hashtable[idx] = new T(key);
      return true;
    }
  }

  bool remove(T key) {
    const void *kp = &key;
    // TODO fix: length of key may not be sizeof(key)
    int len = sizeof(key);

    int iter = 0;
    uint32_t hv = 0;
    hash(kp, len, hashseed, &hv);
    size_t idx = probFunc(hv,iter++, kp, len) & hashmask(cap);

    __transaction_relaxed {
      while (primary_hashtable[idx] != nullptr) {
        if (*(primary_hashtable[idx]) == key) {
          --size_;
          primary_hashtable[idx] = nullptr;
          return true;
        }
        //idx = (hv + iter * iter) & hashmask(cap);
        //++iter;
        idx = probFunc(hv,iter++, kp, len) & hashmask(cap);
      }

      return false;
    }
  }

  size_t size() {
    size_t ret = 0;
    for (int i = 0; i < cap; ++i) {
      if (primary_hashtable[i] != nullptr)
        ++ret;
    }
    return ret;
  }

  void resize() {
    
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

#endif //PARALLEL_CONCURRENCY_TRANSCT_MAP_H
