//
// Created by Shihao Jing on 11/12/17.
//

#ifndef PARALLEL_CONCURRENCY_PHASEDCUCKOO_MAP_H
#define PARALLEL_CONCURRENCY_PHASEDCUCKOO_MAP_H

#include <cstddef>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <list>
#include <cmath>
#include <atomic>
#include <queue>

using namespace std;

template <typename T>
class set {
 private:
  T **data;
  size_t count;
 public:
  set() : data(new T*[PROBE_SIZE]), count(0) {
    for (int i = 0; i < PROBE_SIZE; ++i) {
      data[i] = nullptr;
    }
  }

  ~set() {
    /* 
    for (int i = 0; i < count; ++i)
      delete data[i];
    delete [] data; 
    */
  }

  size_t size() {
    return count;
  }

  bool contains(T key) {
    for (int i = 0; i < count; ++i) {
      if (*data[i] == key) {
        return true;
      }
    }
    return false;
  }

  T get(size_t idx) {
    return *(data[idx]);
  }

  void add(T key) {
    data[count++] = new T(key);
  }

  bool remove(T key) {
    for (int i = 0; i < count; ++i) {
      if (*data[i] == key) {
        //delete data[i];
        for (int j = i+1; j < count; ++j) {
          data[j-1] = data[j];
        }
        --count;
        return true;
      }
    }
    return false;
  }
};  

template <typename T, typename Hash = std::hash<T>>
class phasedcuckoo_map {
 private:
  set<T>* tables[2];
  std::mutex* locks[2];
  std::mutex resize_lock;
  size_t cap;
  size_t lock_cap;
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
  }

  size_t hash0(T y) {
    return h(y) + 0x79b9;
  }

  size_t hash1(T y) {
    return h(y) + 0x7905;
  }

  bool try_lock(T key) {
    size_t hv0 = hash0(key);
    size_t hv1 = hash1(key);

    mutex &lock0 = locks[0][hv0 & hashmask(lock_cap)];
    mutex &lock1 = locks[1][hv1 & hashmask(lock_cap)];

    if (lock0.try_lock()) {
      if (lock1.try_lock()) {
        return true;
      }
      else {
        lock0.unlock();
        return false;
      }
    }
    else {
      return false;
    }
  }

  void lock_acquire(T key) {
    size_t hv0 = hash0(key);
    size_t hv1 = hash1(key);

    locks[0][hv0 & hashmask(lock_cap)].lock();
    locks[1][hv1 & hashmask(lock_cap)].lock();
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
    lock_guard<mutex> guard(resize_lock);

    for (size_t i = 0; i < lock_cap; ++i) {
      locks[0][i].lock();
    }

    if (old_capacity != cap) {
      for (size_t i = 0; i < lock_cap; ++i) {
        locks[0][i].unlock();
      }
      return; // someone has resized before
    }

    set<T>* old_tables[2] = {tables[0], tables[1]};

    cap = (cap << 1);
    tables[0] = new set<T>[cap];
    tables[1] = new set<T>[cap];

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < old_capacity; ++j) {
        for (int k = 0; k < old_tables[i][j].size(); ++k) {
          add_no_lock(old_tables[i][j].get(k));
        }
      }
    }

    //delete [] old_tables[0];
    //delete [] old_tables[1];

    for (size_t i = 0; i < lock_cap; ++i) {
      locks[0][i].unlock();
    }
  }

  bool relocate(int i, size_t hvi, int old_capacity) {
    size_t hvj = 0;
    int j = 1 - i;
    for (int round = 0; round < LIMIT; ++round) {
      resize_lock.lock();

      if (old_capacity != cap) {
        resize_lock.unlock();
        return true;
      }

      T y = tables[i][hvi & hashmask(cap)].get(0);

      if (try_lock(y) == false) {
        resize_lock.unlock();
        --round;
        continue;
      }

      switch (i) {
        case 0: 
          hvj = hash1(y);
          break;
        case 1:
          hvj = hash0(y);
          break;
      }

      // resize may happend before
      if (tables[i][hvi & hashmask(cap)].size() < THRESHOLD) {
        lock_release(y);
        resize_lock.unlock();
        return true;
      }

      if (tables[i][hvi & hashmask(cap)].remove(y)) {
        if (tables[j][hvj & hashmask(cap)].size() < THRESHOLD) {
          tables[j][hvj & hashmask(cap)].add(y);
          lock_release(y);
          resize_lock.unlock();
          return true;
        }
        else if (tables[j][hvj & hashmask(cap)].size() < PROBE_SIZE) {
          tables[j][hvj & hashmask(cap)].add(y);
          i = 1 - i;
          hvi = hvj;
          j = 1 - j;
          lock_release(y);
          resize_lock.unlock();
        }
        else {
          tables[i][hvi & hashmask(cap)].add(y);
          lock_release(y);
          resize_lock.unlock();
          return false;
        }
      }
      else if (tables[i][hvi & hashmask(cap)].size() >= THRESHOLD) {
        lock_release(y);
        resize_lock.unlock();
        continue;
      }
      else {
        lock_release(y);
        resize_lock.unlock();
        return true;
      }
    }

    return false;
  }

 public:
  explicit phasedcuckoo_map(size_t capacity) : tables{new set<T>[hashsize(capacity)], new set<T>[hashsize(capacity)]},
                                               locks{new std::mutex[hashsize(LOCK_POWER)], new std::mutex[hashsize(LOCK_POWER)]},
                                               cap(hashsize(capacity)), lock_cap(hashsize(LOCK_POWER))
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
    else if (!relocate(i, h, cap)) {
      resize(cap);
    } 

    return true;
  }

  void populate() {
    int i = 1024;
    while (i) {
      if (add(rand()))
        --i;
    }
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