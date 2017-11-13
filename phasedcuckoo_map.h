//
// Created by Shihao Jing on 11/12/17.
//

#ifndef PARALLEL_CONCURRENCY_PHASEDCUCKOO_MAP_H
#define PARALLEL_CONCURRENCY_PHASEDCUCKOO_MAP_H

#include <cstddef>
#include <vector>

#define LIMIT 10
#define PROBE_SIZE 4
#define THRESHOLD  2

template <typename T>
class set {
private:
    T **table;
    size_t set_size;
public:
    set() : table(new T*[PROBE_SIZE]), set_size(0) { }
    
    ~set() {
        for (int i = 0; i < set_size; ++i) 
            delete table[i];
        delete [] table;
    }

    size_t size() {
        return set_size;
    }

    bool contains(T key) {
        for (int i = 0; i < set_size; ++i) {
            if (table[i] != nullptr && *(table[i]) == key)
                return true;
        }
        return false;
    }

    T get(size_t idx) {
        return *(table[idx]);
    }

    void add(T key) {
        T *p = new T(key);
        table[set_size++] = p;
    }

    bool remove(T key) {
        for (int i = 0; i < set_size; ++i) {
            if (table[i] != nullptr && *(table[i]) == key) {
                --set_size;
                std::swap(table[i], table[set_size]);
                delete table[set_size];
                return true;
            }
        }
        return false;
    }
};

template <typename T>
class phasedcuckoo_map {
private:
    std::vector<set<T>> tables[2];
    size_t cap;
    size_t size_;

    void guardered_lock(T key) {

    }

    void lock_acquire(T key) {

    }

    void lock_release(T key) {

    }

    void resize() {
        size_t old_capacity = cap;;
        cap = cap << 1;

        std::vector<set<T>> &old_primary_table   = tables[0];
        std::vector<set<T>> &old_secondary_table = tables[1];

        tables[0] = std::vector<set<T>>(cap);
        tables[1] = std::vector<set<T>>(cap);

        for (int i = 0; i < old_capacity; ++i) {
            set<T> &set0 = old_primary_table[i]; 
            for (int j = 0; j < set0.size(); ++j)
                add(set0.get(j));
            set<T> &set1 = old_secondary_table[i]; 
            for (int j = 0; j < set1.size(); ++j)
                add(set1.get(j));
        }
    }

    bool relocate(int i, uint32_t hvi) {
        uint32_t hvj = 0;
        int j = 1 - i;
        for (int round = 0; round < LIMIT; ++round) {
            set<T> &set_i = tables[i][hvi & hashmask(cap)];
            T y = set_i.get(0);
            const void *kp = &y;
            // TODO fix: length of key may not be sizeof(key)
            int len = sizeof(T);
            switch (i) {
                case 0: 
                    hash(kp, len, hashseed2, &hvj);
                    break;
                case 1:
                    hash(kp, len, hashseed, &hvj);
                    break;
            }    
            guardered_lock(y);
            set<T> &set_j = tables[j][hvj & hashmask(cap)];
            if (set_i.remove(y)) {
                if (set_j.size() < THRESHOLD) {
                    set_j.add(y);
                    return true;
                }
                else if (set_j.size() < PROBE_SIZE) {
                    set_j.add(y);
                    i = 1 - i;
                    hvi = hvj;
                    j = 1 - j;
                }
                else {
                    set_i.add(y);
                    return false;
                }
            }
            else if (set_i.size() >= THRESHOLD) {
                continue;
            }
            else {
                return true;
            }
        }

        return false;
    }

public:
    explicit phasedcuckoo_map(size_t capacity) : tables{std::vector<set<T>>(hashsize(capacity)), std::vector<set<T>>(hashsize(capacity))},
                                                 cap(hashsize(capacity)), size_(0) 
    {
    }

    bool remove(T key) {
        guardered_lock(key);

        const void *kp = &key;
        // TODO fix: length of key may not be sizeof(key)
        int len = sizeof(T);

        uint32_t hv0 = 0;
        hash(kp, len, hashseed, &hv0);
        uint32_t hv1 = 0;
        hash(kp, len, hashseed2, &hv1);

        set<T> &set0 = tables[0][hv0 & hashmask(cap)];
        set<T> &set1 = tables[1][hv1 & hashmask(cap)];

        if (set0.contains(key)) {
            set0.remove(key);
            return true;
        }
        else if (set1.contains(key)) {
            set1.remove(key);
            return true;
        }

        return false;
    }

    bool add(T key) {
        lock_acquire(key);

        const void *kp = &key;
        // TODO fix: length of key may not be sizeof(key)
        int len = sizeof(T);
        
        uint32_t hv0 = 0;
        hash(kp, len, hashseed, &hv0);
        uint32_t hv1 = 0;
        hash(kp, len, hashseed2, &hv1);

        int i = -1, h = -1;
        bool must_resize = false;

        set<T> &set0 = tables[0][hv0 & hashmask(cap)];
        set<T> &set1 = tables[1][hv1 & hashmask(cap)];

        if (set0.contains(key) || set1.contains(key)) {
            lock_release(key);
            return false;
        }

        if (set0.size() < THRESHOLD) {
            set0.add(key);
            lock_release(key);
            return true;
        }
        else if (set1.size() < THRESHOLD) {
            set1.add(key);
            lock_release(key);
            return true;
        }
        else if (set0.size() < PROBE_SIZE) {
            set0.add(key);
            i = 0;
            h = hv0;
            lock_release(key);
        }
        else if (set1.size() < PROBE_SIZE) {
            set1.add(key);
            i = 1;
            h = hv1;
            lock_release(key);
        }
        else {
            must_resize = true;
            lock_release(key);
        }

        if (must_resize) {
            resize();
            add(key);
        }
        else if (!relocate(i, h)) {
            resize();
        }

        return true;
    }

    size_t size() {
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