//
// Created by Shihao Jing on 11/12/17.
//

#ifndef PARALLEL_CONCURRENCY_TRANSACTION_CUCKOO_MAP_H
#define PARALLEL_CONCURRENCY_TRANSACTION_CUCKOO_MAP_H

#include <iostream>

template <typename T, typename Hash = std::hash<T>>
class transaction_cuckoo_map {
private:
  size_t cap;
  size_t count;
  T** tables[2];
  Hash h;

  size_t hash0(T y) {
    return h(y);
  }

  size_t hash1(T y) {
    return h(y) + 1295871;
  }


  void resize(size_t old_cap) {
    T **old_tables[2];

    __transaction_atomic {

    if (old_cap != cap) {
        return;
    }

    old_tables[0] = tables[0];
    old_tables[1] =  tables[1];

    cap = (cap << 1);
    tables[0] = (T**)malloc(sizeof(T**) * cap);
    tables[1] = (T**)malloc(sizeof(T**) * cap);

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < cap; ++j) {
        tables[i][j] = nullptr;
      }
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < old_cap; ++j) {
        if (old_tables[i][j] != nullptr) {
          add(*(old_tables[i][j]));
          delete old_tables[i][j];
        }
      }
    }

    delete [] old_tables[0];
    delete [] old_tables[1];

    }

  }

public:
  explicit transaction_cuckoo_map(size_t hashpower) : tables{new T*[hashsize(hashpower)], new T*[hashsize(hashpower)]},
                                          cap(hashsize(hashpower)), count(0)
  {
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < cap; ++j) {
        tables[i][j] = nullptr;
      }
    }
  }


  bool contains(T key) {
    uint32_t hv0 = hash0(key);
    uint32_t hv1 = hash1(key);

    __transaction_atomic {

    if (tables[0][hv0 & hashmask(cap)] != nullptr &&
        *(tables[0][hv0 & hashmask(cap)]) == key) {
      return true;
    }
    else if (tables[1][hv1 & hashmask(cap)] != nullptr &&
        *(tables[1][hv1 & hashmask(cap)]) == key) {
      return true;
    }

    return false;

    }
  }

  [[transaction_safe]] bool add(T key) {
    if (contains(key))
      return false;

    auto x = new T(key);

    for (int i = 0; i < LIMIT; ++i) {
      __transaction_atomic {

      uint32_t hv0 = hash0(*x);
      auto tmp = tables[0][hv0 & hashmask(cap)];
      tables[0][hv0 & hashmask(cap)] = x;
      x = tmp;
      if (x == nullptr)
        return true;

      uint32_t hv1 = hash1(*x);
      tmp = tables[1][hv1 & hashmask(this->cap)];
      tables[1][hv1 & hashmask(this->cap)] = x;
      x = tmp;
      if (x == nullptr)
        return true;

      }
    }


    // to avoid memory leak
    key = *x;
    delete x;

    resize(cap);

    return add(key);
  }

  bool remove(T key) {
    uint32_t hv0 = hash0(key);
    uint32_t hv1 = hash1(key);

    __transaction_atomic {

    if (tables[0][hv0 & hashmask(cap)] != nullptr &&
        *(tables[0][hv0 & hashmask(cap)]) == key) {
      delete tables[0][hv0 & hashmask(cap)];
      tables[0][hv0 & hashmask(cap)] = nullptr;
      return true;
    }
    else if (tables[1][hv1 & hashmask(cap)] != nullptr &&
        *(tables[1][hv1 & hashmask(cap)]) == key) {
      delete tables[1][hv1 & hashmask(cap)];
      tables[1][hv1 & hashmask(cap)] = nullptr;
      return true;
    }

    return false;

    }
  }

  size_t size() {
    size_t ret = 0;
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < cap; ++j) {
        if (tables[i][j] != nullptr) {
          ++ret;
        }
      }
    }
    return ret;
  }
};

#endif //PARALLEL_CONCURRENCY_CUCKOO_MAP_H
