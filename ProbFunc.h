//
// Created by Shihao Jing on 11/9/17.
//

#ifndef PARALLEL_CONCURRENCY_PROBFUNC_H
#define PARALLEL_CONCURRENCY_PROBFUNC_H

#include <cstddef>
#include <cstdint>

#include <cstdlib>

class Prob {
protected:
  uint32_t hashseed;
public:
  Prob() : hashseed(rand()) { }
  virtual unsigned long operator()(uint32_t hv, int iter, const void *key, int len) = 0;
};

class LiearProb : public Prob {
public:
  LiearProb() : Prob() { }
  unsigned long operator()(uint32_t hv, int iter, const void *key, int len) override {
    return hv + iter;
  }
};

class QuadraticProb : public Prob {
public:
  QuadraticProb() : Prob() { }
  unsigned long operator()(uint32_t hv, int iter, const void *key, int len) override {
    return hv + iter * iter;
  }
};

class DoubleHashProb : public Prob {
 public:
  DoubleHashProb() : Prob() { }
  unsigned long operator()(uint32_t hv, int iter, const void *key, int len) override {
    uint32_t hv2;
    hash(key, len, hashseed, &hv2);
    return hv + iter * hv2;
  }
};



#endif //PARALLEL_CONCURRENCY_PROBFUNC_H
