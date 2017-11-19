//
// Created by Shihao Jing on 11/9/17.
//

#include "hash.h"
#include "murmur3_hash.h"
#include "seq_map.h"
#include "cuckoo_map.h"
#include "phasedcuckoo_map.h"
#include "refined_phasedcuckoo_map.h"
#include <unordered_set>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <chrono>

uint32_t hashseed;
uint32_t hashseed2;
hash_func hash;

void hash_init() {
  hashseed = 1240981;
  hashseed2 = 12590812;
  hash = MurmurHash3_x86_32;
}

/// help() - Print a help message
void help(char *progname) {
  using std::cout;
  using std::endl;
  cout << "Usage: " << progname << " [OPTIONS]" << endl;
  cout << "Execute a concurrent set microbenchmark" << endl;
  cout << "  -k    key range for elements in the list" << endl;
  cout << "  -o    operations per thread" << endl;
  cout << "  -b    # buckets for hash tests" << endl;
  cout << "  -r    ratio of lookup operations" << endl;
  cout << "  -t    number of threads" << endl;
  cout << "  -n    test name" << endl;
  cout << "        [l]ist, [r]wlist, [h]ash, [s]entinel hash" << endl;
}

template <typename set_t>
void bench(unsigned keyrange, unsigned iters, unsigned hashpower, unsigned ratio, unsigned nthreads) {
  using std::cout;
  using std::cerr;
  using std::endl;
  using std::chrono::duration_cast;
  using std::chrono::duration;


  std::random_device r;
  std::default_random_engine e(r());
  std::uniform_int_distribution<int> key_rand(0, keyrange);
  std::uniform_int_distribution<int> ratio_rand(1, 100);

  set_t my_set(hashpower);

  int inserted[nthreads];
  int removed[nthreads];
  for (int j = 0; j < nthreads; ++j)
    inserted[j] = removed[j] = 0;

  std::unordered_set<int> set;
  auto work = [&](int tid) {
    for (int i = 0; i < iters; ++i) {
      int key = key_rand(e);
      int action = ratio_rand(e);
      if (action <= ratio) {
        //set.insert(key);
        if (my_set.add(key))
          ++inserted[tid];
      }
      else {
        //set.erase(key);
        if (my_set.remove(key))
          ++removed[tid];
      }
    }
  };

  // timers for benchmark start and end
  std::chrono::high_resolution_clock::time_point start_time, end_time;

  start_time = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; ++i) {
    threads.push_back(std::thread(work, i));
  }

  for (int i = 0; i < nthreads; ++i) {
    threads[i].join();
  }

  end_time = std::chrono::high_resolution_clock::now();

  // print the benchmark throughput
  auto diff = end_time - start_time;
  double exec_time = std::chrono::duration<double, std::milli> (diff).count();
  printf("latency: %f ms\n", exec_time);
  printf("Ops per seccond: %f \n", iters / (exec_time / 1000));

  int expect_size = 0;
  for (int i = 0; i < nthreads; ++i) {
    expect_size += (inserted[i] - removed[i]);
  }

  int set_size = my_set.size();

  cout << "expect_size: " << expect_size << endl;
  cout << "set_size     " << set_size << endl;
  cout << "std set size " << set.size() << endl;

  if (expect_size == set_size)
    cout << "expect_size == my_set.size()" << endl;
  else
    cerr << "Error: expect_size != my_set.size()" << endl;
}

int main(int argc, char** argv) {
  using std::cout;
  using std::cerr;
  using std::endl;

  hash_init();

  // for getopt
  long opt;
  // parameters
  unsigned keyrange = 256;
  unsigned ops      = 1000;
  unsigned hashpower  = 16;
  unsigned ratio    = 60;
  unsigned threads  = 8;
  char     test     = 's';

  // parse the command-line options.  see help() for more info
  while ((opt = getopt(argc, argv, "hr:o:c:R:m:n:t:p:")) != -1) {
    switch(opt) {
      case 'h': help(argv[0]);           return 0;
      case 'r': keyrange = atoi(optarg); break;
      case 'o': ops      = atoi(optarg); break;
      case 'c': hashpower  = atoi(optarg); break;
      case 'R': ratio    = atoi(optarg); break;
      case 't': threads  = atoi(optarg); break;
      case 'm': test = optarg[0];        break;
    }
  }

  // print the configuration
  cout << "Configuration: " << endl;
  cout << "  key range:            " << keyrange << endl;
  cout << "  ops/thread:           " << ops << endl;
  cout << "  hashpower:            " << hashpower << endl;
  cout << "  insert/remove:        " << ratio << "/" << (100 - ratio) << endl;
  cout << "  threads:              " << threads << endl;
  cout << "  test name:            " << test << endl;
  cout << endl;

  // run the microbenchmark
  if (test == 's') {
    bench<seq_map<int>>(keyrange, ops, hashpower, ratio, threads);
  }
  else if (test == 'k') {
    bench<cuckoo_map<int>>(keyrange, ops, hashpower, ratio, threads);
  }
  else if (test == 'p') {
    bench<phasedcuckoo_map<int>>(keyrange, ops, hashpower, ratio, threads);
  }
  else if (test == 'r') {
    bench<refined_phasedcuckoo_map<int>>(keyrange, ops, hashpower, ratio, threads);
  }

  return 0;
}