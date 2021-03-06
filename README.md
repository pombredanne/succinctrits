# succinctrits
This library provides succinct Rank/Select data structures on trits (i.e., ternary digits). A vector consisting of n values from {0,1,2} is stored in 1.6n bits of space, which is close to the theoretically-optimal space usage of 1.57n bits [1]. The Rank/Select data structures are implemented in 0.32n additional bits of space in a similar manner to traditional approaches for bit vectors (e.g., [2]).

The trit-vector implementation is based on Fischer's approach presented in [3].

## Install

This library consists of only header files. Please pass the path to the directory [`succinctrits/include`](https://github.com/kampersanda/succinctrits/tree/master/include).

## Sample usage

```c++
#include <iostream>
#include <random>

#include <rs_support.hpp>
#include <trit_vector.hpp>

int main() {
    std::vector<uint8_t> trits = {1, 0, 1, 2, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 2, 1, 2, 0, 1};
    succinctrits::trit_vector tv(trits.begin(), trits.size());

    std::cout << "=== Trie Array ===" << std::endl;
    std::cout << "addr: ";
    for (uint64_t i = 0; i < tv.get_num_trits(); ++i) {
        std::cout << i % 10 << ' ';
    }
    std::cout << std::endl << "trit: ";
    for (uint64_t i = 0; i < tv.get_num_trits(); ++i) {
        std::cout << uint32_t(tv[i]) << ' ';
    }
    std::cout << std::endl;

    succinctrits::rs_support<0> tv_rs_0(&tv);
    succinctrits::rs_support<1> tv_rs_1(&tv);
    succinctrits::rs_support<2> tv_rs_2(&tv);

    std::cout << "=== Operations ===" << std::endl;
    std::cout << "rank_0(6)   = " << tv_rs_0.rank(6) << std::endl;
    std::cout << "rank_1(10)  = " << tv_rs_1.rank(10) << std::endl;
    std::cout << "rank_2(3)   = " << tv_rs_2.rank(3) << std::endl;
    std::cout << "select_0(3) = " << tv_rs_0.select(3) << std::endl;
    std::cout << "select_1(5) = " << tv_rs_1.select(5) << std::endl;
    std::cout << "select_2(0) = " << tv_rs_2.select(0) << std::endl;

    std::cout << "=== Statistics ===" << std::endl;
    std::cout << "num trits: " << tv.get_num_trits() << std::endl;
    std::cout << "num 0s:    " << tv_rs_0.get_num_target_trits() << std::endl;
    std::cout << "num 1s:    " << tv_rs_1.get_num_target_trits() << std::endl;
    std::cout << "num 2s:    " << tv_rs_2.get_num_target_trits() << std::endl;

    {
        std::ofstream ofs("trits.idx");
        tv.save(ofs);
        tv_rs_0.save(ofs);
        tv_rs_1.save(ofs);
        tv_rs_2.save(ofs);
    }

    {
        std::ifstream ifs("trits.idx");
        succinctrits::trit_vector other_tv;
        succinctrits::rs_support<0> other_tv_rs_0;
        succinctrits::rs_support<1> other_tv_rs_1;
        succinctrits::rs_support<2> other_tv_rs_2;
        other_tv.load(ifs);
        other_tv_rs_0.load(ifs);
        other_tv_rs_1.load(ifs);
        other_tv_rs_2.load(ifs);
        other_tv_rs_0.set_vector(&other_tv);
        other_tv_rs_1.set_vector(&other_tv);
        other_tv_rs_2.set_vector(&other_tv);
    }

    std::remove("trits.idx");
    return 0;
}
```

The output will be

```
=== Trie Array ===
addr: 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 
trit: 1 0 1 2 1 0 0 1 0 1 0 0 1 1 1 2 1 2 0 1 
=== Operations ===
rank_0(6)   = 2
rank_1(10)  = 5
rank_2(3)   = 0
select_0(3) = 8
select_1(5) = 12
select_2(0) = 3
=== Statistics ===
num trits: 20
num 0s:    7
num 1s:    10
num 2s:    3
```

## Benchmark

- 3.5 GHz Intel Core i7
- 16 GB 2133 MHz LPDDR3
- OS X 10.14.6
- Apple clang version 11.0.0

```
$ ./benchmark/benchmark 
=== Benchmark for 1000000 trits ===
# access time: 19.3666 ns/op
# rank time:   34.0611 ns/op
# select time: 117.213 ns/op
# trit_vector: 1.60006 bits/trit
# rs_support:  0.321088 bits/trit
=== Benchmark for 10000000 trits ===
# access time: 27.337 ns/op
# rank time:   43.6852 ns/op
# select time: 143.572 ns/op
# trit_vector: 1.60001 bits/trit
# rs_support:  0.320986 bits/trit
=== Benchmark for 100000000 trits ===
# access time: 23.5833 ns/op
# rank time:   76.2127 ns/op
# select time: 241.136 ns/op
# trit_vector: 1.6 bits/trit
# rs_support:  0.320977 bits/trit
```

## References

1. Mihai Patrascu. **Succincter**. In *FOCS*, pages 305–313, 2008.
2. Rodrigo González, Szymon Grabowski, Veli Mäkinen, and Gonzalo Navarro. **Practical implementation of rank and select queries**. In *WEA*, pages 27–38, 2005.
3. Johannes Fischer and Daniel Peters. **GLOUDS: Representing tree-like graphs**. *J. Discrete Algorithm.*, 36:39–49, 2016.