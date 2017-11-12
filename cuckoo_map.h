//
// Created by Shihao Jing on 11/12/17.
//

#ifndef PARALLEL_CONCURRENCY_CUCKOO_MAP_H
#define PARALLEL_CONCURRENCY_CUCKOO_MAP_H

#include "map.h"
#include <iostream>

#define LIMIT 10

template <typename T>
class cuckoo_map : public BaseMap<T> {
private:
  T **secondary_table;

  void resize() override {
    std::cout << "resize happening" << std::endl;
    size_t oldCapacity = this->cap;
    this->cap = this->cap << 1;
    
    T **old_primary_table = this->primary_table;
    T **old_secondary_table = this->secondary_table;
    
    this->primary_table = new T*[this->cap];
    this->secondary_table = new T*[this->cap];
    for (int i = 0; i < this->cap; ++i) {
      this->primary_table[i] = nullptr;
      this->secondary_table[i] = nullptr;
    }

    for (int i = 0; i < oldCapacity; ++i) {
      if (old_primary_table[i] != nullptr) {
        add(*old_primary_table[i]);
        delete(old_primary_table[i]);
      }
      if (old_secondary_table[i] != nullptr) {
        add(*old_secondary_table[i]);
        delete(old_secondary_table[i]);
      }
    }

    delete(old_primary_table);
    delete(old_secondary_table);
  }
public:
  explicit cuckoo_map(size_t hashpower) : BaseMap<T>(hashpower), secondary_table(new T*[hashsize(hashpower)])
  {
    for (int i = 0; i < this->cap; ++i)
      secondary_table[i] = nullptr;
  }


  bool contains(T key) override {
    const void *kp = &key;
    // TODO fix: length of key may not be sizeof(key)
    int len = sizeof(T);

    uint32_t hv1 = 0;
    hash(kp, len, hashseed, &hv1);
    uint32_t hv2 = 0;
    hash(kp, len, hashseed2, &hv2);

    if (this->primary_table[hv1 & hashmask(this->cap)] != nullptr &&
        *(this->primary_table[hv1 & hashmask(this->cap)]) == key) {
      return true;
    }
    else if (this->secondary_table[hv2 & hashmask(this->cap)] != nullptr &&
        *(this->secondary_table[hv2 & hashmask(this->cap)]) == key) {
      return true;
    }

    return false;
  }

  bool add(T key) override {
    if (contains(key))
      return false;

    auto x = new T(key);
    for (int i = 0; i < LIMIT; ++i) {
      uint32_t hv1 = 0;
      hash(x, sizeof(T), hashseed, &hv1);
      auto tmp = this->primary_table[hv1 & hashmask(this->cap)];
      this->primary_table[hv1 & hashmask(this->cap)] = x;
      x = tmp;
      if (x == nullptr)
        return true;

      uint32_t hv2 = 0;
      hash(x, sizeof(T), hashseed2, &hv2);
      tmp = this->secondary_table[hv2 & hashmask(this->cap)];
      this->secondary_table[hv2 & hashmask(this->cap)] = x;
      x = tmp;
      if (x == nullptr)
        return true;
    }

    // to avoid memory leak
    key = *x;
    delete(x);

    resize();
    return add(key);
  }

  bool remove(T key) override {
    const void *kp = &key;
    // TODO fix: length of key may not be sizeof(key)
    int len = sizeof(T);

    uint32_t hv1 = 0;
    hash(kp, len, hashseed, &hv1);
    uint32_t hv2 = 0;
    hash(kp, len, hashseed2, &hv2);

    if (this->primary_table[hv1 & hashmask(this->cap)] != nullptr &&
        *(this->primary_table[hv1 & hashmask(this->cap)]) == key) {
      delete(this->primary_table[hv1 & hashmask(this->cap)]);
      this->primary_table[hv1 & hashmask(this->cap)] = nullptr;
      return true;
    }
    else if (secondary_table[hv2 & hashmask(this->cap)] != nullptr &&
        *(secondary_table[hv2 & hashmask(this->cap)]) == key) {
      delete(this->secondary_table[hv2 & hashmask(this->cap)]);
      this->secondary_table[hv2 & hashmask(this->cap)] = nullptr;
      return true;
    }

    return false;
  }

  size_t size() override {
    size_t ret = 0;
    for (int i = 0; i < this->cap; ++i) {
      if (this->primary_table[i] != nullptr)
        ++ret;
      if (this->secondary_table[i] != nullptr)
        ++ret;
    }
    return ret;
  }
};

#endif //PARALLEL_CONCURRENCY_CUCKOO_MAP_H
