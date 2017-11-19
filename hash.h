//
// Created by Shihao Jing on 11/9/17.
//

#ifndef PARALLEL_CONCURRENCY_HASH_H
#define PARALLEL_CONCURRENCY_HASH_H

#include <cstdint>
#include <cstddef>

#define hashsize(n) (1<<(n))
#define hashmask(n) (n-1)

typedef void (*hash_func)(const void *key, int len, uint32_t seed, void *out);

static size_t do_linear(size_t hv, int iter) {
    return hv + iter;
}

static size_t do_quadratic(size_t hv, int iter) {
    return hv + iter * iter;
}

extern size_t prob(size_t hv, int iter) {
  return do_quadratic(hv, iter);
}

extern uint32_t hashseed;
extern uint32_t hashseed2;
extern hash_func hash;
extern constexpr size_t LOCK_POWER = 6;


#endif //PARALLEL_CONCURRENCY_HASH_H
