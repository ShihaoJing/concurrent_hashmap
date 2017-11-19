//
// Created by Shihao Jing on 11/12/17.
//

#ifndef PARALLEL_CONCURRENCY_TRANSACTION_PHASEDCUCKOO_MAP_H
#define PARALLEL_CONCURRENCY_TRANSACITON_PHASEDCUCKOO_MAP_H

#include <cstddef>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <list>
#include <cmath>

template <typename T, typename Hash = std::hash<T>>
class transaction_phasedcuckoo_map {
 private:
  set<T>* tables[2];
  size_t hashpower;
  size_t cap;
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
  }

  size_t hash0(T y) {
    return h(y);
  }

  size_t hash1(T y) {
    return h(y) + 1295871;
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
    
    __transaction_atomic {

    if (old_capacity != cap) {
      return; // someone has resized before
    }

    set<T>* old_tables[2] = {tables[0], tables[1]};

    cap = (cap << 1);
    tables[0] = new set<T>[cap];
    tables[1] = new set<T>[cap];

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < old_capacity; ++j) {
        set<T> &old_set = old_tables[i][j];
        for (int k = 0; k < old_set.size(); ++k) {
          add_no_lock(old_set.get(k));
        }
      }
    }

    delete [] old_tables[0];
    delete [] old_tables[1];

    }

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

      __transaction_atomic {

      // resize may happend before
      if (tables[i][hvi & hashmask(cap)].size() < THRESHOLD) {
        return true;
      }

      if (tables[i][hvi & hashmask(cap)].remove(y)) {
        if (tables[j][hvj & hashmask(cap)].size() < THRESHOLD) {
          tables[j][hvj & hashmask(cap)].add(y);
          return true;
        }
        else if (tables[j][hvj & hashmask(cap)].size() < PROBE_SIZE) {
          tables[j][hvj & hashmask(cap)].add(y);
          i = 1 - i;
          hvi = hvj;
          j = 1 - j;
        }
        else {
          tables[i][hvi & hashmask(cap)].add(y);
          return false;
        }
      }
      else if (tables[i][hvi & hashmask(cap)].size() >= THRESHOLD) {
        continue;
      }
      else {
        return true;
      }
    
    } // transaction


    }

    return false;
  }

 public:
  explicit transaction_phasedcuckoo_map(size_t capacity) : tables{new set<T>[hashsize(capacity)], new set<T>[hashsize(capacity)]},
                                               hashpower(capacity),
                                               cap(hashsize(capacity)),  size_(0)
  {

  }

  bool remove(T key) {
    __transaction_atomic {

    size_t hv0 = hash0(key);
    size_t hv1 = hash1(key);

    if (tables[0][hv0 & hashmask(cap)].contains(key)) {
      tables[0][hv0 & hashmask(cap)].remove(key);
      return true;
    }
    else if (tables[1][hv1 & hashmask(cap)].contains(key)) {
      tables[1][hv1 & hashmask(cap)].remove(key);
      return true;
    }

    }
    return false;
  }

  bool add(T key) {

    int i; // which table to relocate
    size_t h; // hash value of h0 or h1

    bool must_resize = false;

    size_t hv0 = hash0(key);
    size_t hv1 = hash1(key);

    __transaction_atomic {

    if (tables[0][hv0 & hashmask(cap)].contains(key) || tables[1][hv1 & hashmask(cap)].contains(key)) {
      return false;
    }

    if (tables[0][hv0 & hashmask(cap)].size() < THRESHOLD) {
      tables[0][hv0 & hashmask(cap)].add(key);
      return true;
    }
    else if (tables[1][hv1 & hashmask(cap)].size() < THRESHOLD) {
      tables[1][hv1 & hashmask(cap)].add(key);
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

    }


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