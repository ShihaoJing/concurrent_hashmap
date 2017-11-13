//
// Created by Shihao Jing on 11/9/17.
//

#ifndef PARALLEL_CONCURRENCY_SEQ_MAP_H
#define PARALLEL_CONCURRENCY_SEQ_MAP_H

#include "map.h"
#include <cstddef>
#include <cstdint>
#include <random>
#include <iostream>

template <typename T, typename ProbFunc>
class seq_map : public BaseMap<T> {
  ProbFunc probFunc;

  void resize() {
    std::cout << "resize happening" << std::endl;
    size_t oldCapacity = this->cap;
    this->cap = this->cap << 1;
    
    T **old_primary_table = this->primary_table;
    
    this->primary_table = new T*[this->cap];

    for (int i = 0; i < this->cap; ++i) {
      this->primary_table[i] = nullptr;
    }

    for (int i = 0; i < oldCapacity; ++i) {
      if (old_primary_table[i] != nullptr) {
        add(*old_primary_table[i]);
        delete old_primary_table[i];
      }
    }

    delete [] old_primary_table;
  }

public:
  explicit seq_map(size_t hashpower) : BaseMap<T>(hashpower), probFunc() { }

  bool contains(T key) override {
    const void *kp = &key;
    // TODO fix: length of key may not be sizeof(key)
    int len = sizeof(key);

    int iter = 0;
    uint32_t hv = 0;
    hash(kp, len, hashseed, &hv);
  
    uint32_t next_hv = probFunc(hv,iter++, kp, len);

    while (this->primary_table[next_hv & hashmask(this->cap)] != nullptr) {
      if (*(this->primary_table[next_hv & hashmask(this->cap)]) == key) {
        return true;
      }
      next_hv = probFunc(hv,iter++, kp, len);
    }

    return false;
  }

  bool add(T key) {
    const void *kp = &key;
    // TODO fix: length of key may not be sizeof(key)
    int len = sizeof(key);

    int iter = 0;
    uint32_t hv = 0;
    hash(kp, len, hashseed, &hv);
  
    uint32_t next_hv = probFunc(hv,iter++, kp, len);

    while (this->primary_table[next_hv & hashmask(this->cap)] != nullptr) {
      if (*(this->primary_table[next_hv & hashmask(this->cap)]) == key) {
        return false;
      }
      next_hv = probFunc(hv,iter++, kp, len);
    }

    ++this->size_;
    this->primary_table[next_hv & hashmask(this->cap)] = new T(key);
    
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

    while (this->primary_table[next_hv & hashmask(this->cap)] != nullptr) {
      if (*(this->primary_table[next_hv & hashmask(this->cap)]) == key) {
        delete this->primary_table[next_hv & hashmask(this->cap)];
        --this->size_;
        this->primary_table[next_hv & hashmask(this->cap)] = nullptr;
        return true;
      }
      next_hv = probFunc(hv,iter++, kp, len);
    }

    return false;
  }

  size_t size() {
    size_t ret = 0;
    for (int i = 0; i < this->cap; ++i) {
      if (this->primary_table[i] != nullptr)
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

#endif //PARALLEL_CONCURRENCY_SEQ_MAP_H
