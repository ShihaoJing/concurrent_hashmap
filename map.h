//
// Created by Shihao Jing on 11/12/17.
//

#ifndef PARALLEL_CONCURRENCY_MAP_H_H
#define PARALLEL_CONCURRENCY_MAP_H_H

enum class node_state {
  FREE,
  IN_USE,
  ERASED
};

template<typename T>
struct node {
  T key;
  node_state state = node_state::FREE;
};


#include <cstddef>
template <typename T>
class BaseMap {
protected:
  T **primary_table;
  size_t cap;
  size_t size_;
  virtual void resize() = 0;
public:
  explicit BaseMap(size_t hashpower) : primary_table(new T*[hashsize(hashpower)]), cap(hashsize(hashpower)), size_(0)
  {
    for (int i = 0; i < cap; ++i)
      primary_table[i] = nullptr;
  }

  virtual bool add(T key) = 0;
  virtual bool remove(T key) = 0;
  virtual bool contains(T key) = 0;
  virtual size_t size() = 0;
};

#endif //PARALLEL_CONCURRENCY_MAP_H_H
