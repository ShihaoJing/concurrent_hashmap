//
// Created by Shihao Jing on 11/12/17.
//

#ifndef PARALLEL_CONCURRENCY_REFINED_PHASEDCUCKOO_MAP_H
#define PARALLEL_CONCURRENCY_REFINED_PHASEDCUCKOO_MAP_H

#include <cstddef>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <list>
#include <cmath>
#include <atomic>

template <typename T, typename Hash = std::hash<T>>
class refined_phasedcuckoo_map {
 private:
  set<T>* tables[2];
  std::mutex* locks[2];
  //std::mutex resize_lock;
  std::atomic_flag resize_lock = ATOMIC_FLAG_INIT;
  size_t hashpower;
  size_t cap;
  size_t lock_cap;
  size_t size_;
  Hash h;

  void validate() {
    std::unordered_set<T> validation;
    int c = 0;
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < cap; ++j) {
        for (int k = 0; k < tables[i][j].size(); ++k) {
          if (validation.insert(tables[i][j].get(k)).second == false) {
            throw std::logic_error("unexpected duplicate in phasedcuckoo");
          }
        }
      }
    }
    std::cout << lock_cap << std::endl;
  }

  size_t hash0(T y) {
    return h(y);
  }

  size_t hash1(T y) {
    return h(y) + 1295871;
  }

  void lock_acquire(T key) {
    while (resize_lock.test_and_set(std::memory_order_acquire)); // acquire resize lock

    size_t hv0 = hash0(key);
    size_t hv1 = hash1(key);

    locks[0][hv0 & hashmask(lock_cap)].lock();
    locks[1][hv1 & hashmask(lock_cap)].lock();

    resize_lock.clear(std::memory_order_release);  // release lock
  }

  void lock_release(T key) {
    size_t hv0 = hash0(key);
    size_t hv1 = hash1(key);

    locks[0][hash0(key) & hashmask(lock_cap)].unlock();
    locks[1][hash1(key) & hashmask(lock_cap)].unlock();
  }

  void add_no_lock(T key) {
    size_t hv0 = hash0(key);
    size_t hv1 = hash1(key);

    set<T> &set0 = tables[0][hv0 & hashmask(cap)];
    set<T> &set1 = tables[1][hv1 & hashmask(cap)];

    if (set0.size() < THRESHOLD) {
      set0.add(key);
    }
    else if (set1.size() < THRESHOLD) {
      set1.add(key);
    }
    else if (set0.size() < PROBE_SIZE) {
      set0.add(key);
    }
    else if (set1.size() < PROBE_SIZE) {
      set1.add(key);
    }
    else {
      throw std::logic_error("Unexpected case in add_no_lock");
    }
  }

  void resize(int old_capacity) {
    while (resize_lock.test_and_set(std::memory_order_acquire)); // acquire resize lock

    for (size_t i = 0; i < lock_cap; ++i) {
      locks[0][i].lock();
    }


    if (old_capacity != cap) {
      for (size_t i = 0; i < lock_cap; ++i) {
            locks[0][i].unlock();
      }
      resize_lock.clear(std::memory_order_release);  // release lock
      return; // someone has resized before
    }

    set<T>* old_tables[2] = {tables[0], tables[1]};

    cap = (cap << 1);
    tables[0] = new set<T>[cap];
    tables[1] = new set<T>[cap];

    for (size_t i = 0; i < old_capacity; ++i) {
      set<T> &set0 = old_tables[0][i];
      for (size_t j = 0; j < set0.size(); ++j)
        add_no_lock(set0.get(j));

      set<T> &set1 = old_tables[1][i];
      for (size_t j = 0; j < set1.size(); ++j)
        add_no_lock(set1.get(j));
    }

    delete [] old_tables[0];
    delete [] old_tables[1];

    for (size_t i = 0; i < lock_cap; ++i) {
      locks[0][i].unlock();
    }

    delete [] locks[0];
    delete [] locks[1];

    lock_cap = (lock_cap << 1);
    locks[0] = new std::mutex[lock_cap];
    locks[1] = new std::mutex[lock_cap];

    resize_lock.clear(std::memory_order_release);  // release lock
  }

  bool relocate(int i, size_t hvi) {
    size_t hvj = 0;
    int j = 1 - i;
    for (int round = 0; round < LIMIT; ++round) {
      T y = tables[i][hvi & hashmask(cap)].get(0);
      switch (i) {
        case 0: 
          hvj = hash1(y);
          break;
        case 1:
          hvj = hash0(y);
          break;
      }

      lock_acquire(y);

      // resize may happend before
      if (tables[i][hvi & hashmask(cap)].size() < THRESHOLD) {
        lock_release(y);
        return true;
      }

      if (tables[i][hvi & hashmask(cap)].remove(y)) {
        if (tables[j][hvj & hashmask(cap)].size() < THRESHOLD) {
          tables[j][hvj & hashmask(cap)].add(y);
          lock_release(y);
          return true;
        }
        else if (tables[j][hvj & hashmask(cap)].size() < PROBE_SIZE) {
          tables[j][hvj & hashmask(cap)].add(y);
          i = 1 - i;
          hvi = hvj;
          j = 1 - j;
          lock_release(y);
        }
        else {
          tables[i][hvi & hashmask(cap)].add(y);
          lock_release(y);
          return false;
        }
      }
      else if (tables[i][hvi & hashmask(cap)].size() >= THRESHOLD) {
        lock_release(y);
        continue;
      }
      else {
        lock_release(y);
        return true;
      }
    }

    return false;
  }

 public:
  explicit refined_phasedcuckoo_map(size_t capacity) : tables{new set<T>[hashsize(capacity)], new set<T>[hashsize(capacity)]},
                                               locks{new std::mutex[hashsize(LOCK_POWER)], new std::mutex[hashsize(LOCK_POWER)]},
                                               hashpower(capacity),
                                               cap(hashsize(capacity)), lock_cap(hashsize(LOCK_POWER)), size_(0)
  {

  }

  bool remove(T key) {
    lock_acquire(key);

    size_t hv0 = hash0(key);
    size_t hv1 = hash1(key);

    if (tables[0][hv0 & hashmask(cap)].contains(key)) {
      tables[0][hv0 & hashmask(cap)].remove(key);
      lock_release(key);
      return true;
    }
    else if (tables[1][hv1 & hashmask(cap)].contains(key)) {
      tables[1][hv1 & hashmask(cap)].remove(key);
      lock_release(key);
      return true;
    }

    lock_release(key);
    return false;
  }

  bool add(T key) {
    lock_acquire(key);

    int i; // which table to relocate
    size_t h; // hash value of h0 or h1

    bool must_resize = false;

    size_t hv0 = hash0(key);
    size_t hv1 = hash1(key);

    if (tables[0][hv0 & hashmask(cap)].contains(key) || tables[1][hv1 & hashmask(cap)].contains(key)) {
      lock_release(key);
      return false;
    }

    if (tables[0][hv0 & hashmask(cap)].size() < THRESHOLD) {
      tables[0][hv0 & hashmask(cap)].add(key);
      lock_release(key);
      return true;
    }
    else if (tables[1][hv1 & hashmask(cap)].size() < THRESHOLD) {
      tables[1][hv1 & hashmask(cap)].add(key);
      lock_release(key);
      return true;
    }
    else if (tables[0][hv0 & hashmask(cap)].size() < PROBE_SIZE) {
      tables[0][hv0 & hashmask(cap)].add(key);

      i = 0;
      h = hv0;
    }
    else if (tables[1][hv1 & hashmask(cap)].size() < PROBE_SIZE) {
      tables[1][hv1 & hashmask(cap)].add(key);
      i = 1;
      h = hv1;
    }
    else {
      must_resize = true;
    }

    lock_release(key);

    if (must_resize) {
      resize(cap);
      return add(key);
    }
    else if (!relocate(i, h)) {
      resize(cap);
    }

    return true;
  }

  size_t size() {
    validate();

    size_t ret = 0;
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < cap; ++j) {
        ret += tables[i][j].size();
      }
    }

    return ret;
  }

};


#endif