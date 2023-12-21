## Introduction
This project serves as a Proof-of-Concept (PoC) for leveraging optimization in a specific processing scenario:

Find the longest sequence of `0` in input of zeros and ones, when replace `0` to `1` to maximize distance between `1`

### Common implementation
- Input is `std::vector<bool>`
- Find the longest sequence in the loop

```cpp
size_t longestSeqSize = 0;
size_t longestSeqPos = 0;
size_t currentSeqSize = 0;

for (auto i = 0u; i != input.size(); ++i) {
    auto t = input[i];
    if (t == 1) {
        if (longestSeqSize < currentSeqSize) {
            longestSeqSize = currentSeqSize;
            longestSeqPos = i - currentSeqSize;
        }
        currentSeqSize = 0;
    } else {
        ++currentSeqSize;
    }
}
```
Problems:
- Branchy code, if the input is bernoulli distribution with `q=0.5` we have a lot of branch mis-predictions
- `input[i]` requires bit operations that adds extra CPU cycles

`std::vector<bool>` is specialization of vector with storing multiple values in one byte. 
It spends less memory but additional instructions are usually make code slower. Doesn't recommend for low-latency code without checking.
> `std::vector<bool>` doesn't provide `.data()` method to access raw memory.
### `std::vector<uint8_t>`
Common solution without compress input vector. Store one character in one byte.   
It's faster in average, especially than input data can be fitted in L1 cache.  
- For `*_1` test cases where input is less L1 cache, speed-up about x2. 
- For `*_30` `vector<bool>` in L1, `vector<uint8_t>` in L2 and speed-up is much less.
- For `*_120` `vector<bool>` in L2, `vector<uint8_t>` in L3 and speed-up is much less.

> These comparing `vector<bool>` vs `vector<uint8_t>` are so depends on set-up

### Memoized
We want to process characters in batch by 8 values as one `uint8_t`. 
When, we can generate table for results for all `2^8` possible values and merge results. 

To merge result of adjacent batch we need the next value of `uint8_t`:
- count of `0` before first `1`
- size of tail of `0`
- longest sequence of `0` inside batch

#### Table generation
Because we generate this table only once in compile time, code can be very naive and straightforward.
> Functions are marked as `consteval`
```cpp
struct alignas(16) MemoizedData {
    uint8_t l, m, r;
};

consteval MemoizedData analyzeBits(uint8_t value) {
    assert(value != 0); // For 0 we use only first value
    uint8_t firstOne = 0, longestZero = 0, lastOne = 0;
    uint8_t currentZero = 0;
    bool foundOne = false;

    for (uint8_t i = 0; i < 8; ++i) {
        if (value & (1 << i)) {
            if (!foundOne) {
                firstOne = i;
                foundOne = true;
            }
            longestZero = std::max(longestZero, currentZero);
            currentZero = 0;
            lastOne = i;
        } else {
            if (foundOne) {
                currentZero++;
            }
        }
    }

    return {firstOne, longestZero, static_cast<uint8_t>(7 - lastOne)};
}

static constexpr auto UINT8_SIZE = (1 << 8) - 1;

consteval auto gen() {
    std::array<MemoizedData, UINT8_SIZE + 1> out{};
    out[0] = {8, 8, 8};
    for (uint8_t i = UINT8_SIZE; i != 0; --i) {
        out[i] = analyzeBits(i);
    }
    return out;
};

decltype(auto) process8(uint8_t n) {
    static constexpr auto cached = gen();
    return cached[n];
}
```
#### BoolVector
Because `vector<bool>` doesn't allow access to raw pointer, so we need a wrapper of vector for this:
```cpp

class BoolVector {
public:
    explicit BoolVector(size_t size)
        : m_size(size)
        , m_data((size + 7) / 8, 0) {}

    bool get(size_t index) const {
        checkRange(index);
        size_t byteIndex = index / 8;
        size_t bitIndex = index % 8;
        return (m_data[byteIndex] & (1 << bitIndex)) != 0;
    }

    void set(size_t index, bool value) {
        checkRange(index);
        size_t byteIndex = index / 8;
        size_t bitIndex = index % 8;
        if (value) {
            m_data[byteIndex] |= (1 << bitIndex);
        } else {
            m_data[byteIndex] &= ~(1 << bitIndex);
        }
    }

    const uint8_t* rawData() const {
        return m_data.data();
    }

    size_t size() const {
        return m_size;
    }

    size_t chunks() const {
        return m_data.size();
    }

    size_t fullChunks() const {
        return m_data.size() - (size() % 8 != 0);
    }

private:
    size_t m_size;
    std::vector<uint8_t> m_data;

    void checkRange(size_t index) const noexcept {
        if (index >= size()) {
            assert(false);
//            throw std::out_of_range("Index out of range");
        }
    }
};
```
#### Implementation
```cpp
void findInChunk(BoolVector& input, size_t pos) {
    assert(pos % 8 == 0);
    size_t longestSeqSize = 0;
    size_t longestSeqPos = 0;
    size_t current = 0;
    for (auto i = pos; i != pos + 8; ++i) {
        auto t = input.get(i);
        if (t == 1) {
            if (longestSeqSize < current) {
                longestSeqSize = current;
                longestSeqPos = i - current;
            }
            current = 0;
        } else {
            ++current;
        }
    }
    assert(longestSeqPos + longestSeqSize / 2);
    input.set(longestSeqPos + longestSeqSize / 2, true);
}

void distanceMemoized(BoolVector& input) {
    auto const size = input.size();
    auto const chunks = input.fullChunks();

    size_t current = 0;
    size_t longestSeqSize = 0;
    size_t longestSeqPos = 0;
    bool inChunk = false;

    for (auto i = 0u; i != chunks; ++i) {
        auto [np, longest, ns] = process8(input.rawData()[i]);
        if (np == 8) {
            current += 8;
        } else {
            auto lp = np + current;
            if (lp > longestSeqSize) [[unlikely]] {
                longestSeqSize = lp;
                assert(i * 8 + np >= longestSeqSize);
                longestSeqPos = i * 8 + np - longestSeqSize;
                inChunk = false;
            }
            if (longest > longestSeqSize) [[unlikely]] {
                inChunk = true;
                longestSeqSize = longest;
                longestSeqPos = i * 8;
            }
            current = ns;
        }
    }

    if (size % 8 != 0) {
        for (auto j = chunks * 8; j != size; ++j) {
            auto t = input.get(j);
            if (t == 1) {
                if (longestSeqSize < current) {
                    longestSeqSize = current;
                    assert(j >= current);
                    longestSeqPos = j - current;
                    inChunk = false;
                }
                current = 0;
            } else {
                ++current;
            }
        }
    }

    if (longestSeqSize < current) {
        assert(input.get(size - 1) == false);
        input.set(size - 1, true);
    } else if (longestSeqPos == 0) {
        if (!inChunk) {
            assert(input.get(0) == false || longestSeqSize == 0);
            input.set(0, true);
        } else {
            findInChunk(input, longestSeqPos);
        }
    } else {
        if (!inChunk) {
            assert(input.get(longestSeqPos + longestSeqSize / 2) == false);
            input.set(longestSeqPos + longestSeqSize / 2, true);
        } else {
            findInChunk(input, longestSeqPos);
        }
    }
}
```

Batch processing gives significant speed-up, because it combines small input size, with less cache misses, and few operations per character.
> `[[unlikely]]` gives visible speed-up, because of changing code layout and updating the longest position is limited by `O(srqt(N))`.

Code under `[[unlikely]]` is moved to the end of function, that reduce cycle size.

`FindInChunk` allows don't find the exact position inside batch in process, and makes it only once. 
> `FindInChunk` can be also replaced by table look-up, but because this function can be called only once with very limited cycles, I didn't it.

Using `uint16_t` is not justified, because memory for table is `2^64 ~ 65kb` more than L1 cache. 
### BranchLess 
The common optimization with branchy code is to make it branch-less. The loop body:
```cpp
auto [np, longest, ns] = process8(input.rawData()[i]);
if (np == 8) {
    current += 8;
} else {
    auto lp = np + current;
    if (lp > longestSeqSize) [[unlikely]] {
        longestSeqSize = lp;
        assert(i * 8 + np >= longestSeqSize);
        longestSeqPos = i * 8 + np - longestSeqSize;
        inChunk = false;
    }
    if (longest > longestSeqSize) [[unlikely]] {
        inChunk = true;
        longestSeqSize = longest;
        longestSeqPos = i * 8;
    }
    current = ns;
}
```
can be replaced by 
```cpp
auto const& [np, longest, ns] = process8(input.rawData()[i]);

auto leftValue = np + current;

using StateT = std::tuple<uint32_t, uint32_t, uint32_t>;
StateT all{current + 8, longestSeqSize, longestSeqPos};
StateT noChanges{ns, longestSeqSize, longestSeqPos};
StateT leftUpdate{ns, leftValue, i * 8 - current};
StateT insideUpdate{ns, longest, i * 8 | IN_CHUNK_BIT};

std::array<StateT, 5> results{all, noChanges, leftUpdate, insideUpdate, insideUpdate};

auto hasOnes = np != 8;
auto leftTrigger = np + current > longestSeqSize;
auto triggerInside = longest > longestSeqSize && longest > np + current;
std::tie(current, longestSeqSize, longestSeqPos)
    = results[hasOnes * (1 + leftTrigger + triggerInside * 2)];
```
But, in our case, this is not so efficient. 
Because we need to change three `uint8_t` variables in loop, and we have 4 opportunities for this.
We should use 5 dimension possible answer, that spends `sizeof(uint32_t) * 3 * 5`, that is a lot of `mov` instructions.
So, it leads to very large asm output:
```asm
movzx	eax, BYTE PTR [rdi+rax]
lea	ecx, 0[0+rbx*8]
vmovd	xmm4, r8d
mov	r10d, ecx
sub	ecx, r9d
or	r10d, -2147483648
vmovd	xmm5, ecx
mov	ecx, r8d
vmovd	xmm2, r10d
lea	r10d, 8[r9]
sal	rax, 4
vmovd	xmm3, r10d
add	rax, r13
vpunpckldq	xmm3, xmm3, xmm0
vpinsrd	xmm0, xmm0, r8d, 1
movzx	edx, BYTE PTR [rax]
movzx	esi, BYTE PTR 2[rax]
vpunpcklqdq	xmm0, xmm0, xmm3
movzx	eax, BYTE PTR 1[rax]
mov	r15d, edx
add	edx, r9d
vpinsrd	xmm4, xmm4, esi, 1
vmovd	xmm3, esi
cmp	edx, r8d
vpinsrd	xmm1, xmm5, edx, 1
vmovd	xmm5, eax
mov	DWORD PTR 72[rsp], esi
cmovnb	ecx, edx
vpunpcklqdq	xmm1, xmm4, xmm1
cmp	ecx, eax
vinserti128	ymm0, ymm0, xmm1, 0x1
vpunpckldq	xmm1, xmm3, xmm2
vpinsrd	xmm2, xmm2, eax, 1
setb	cl
xor	eax, eax
cmp	r8d, edx
vmovdqu	YMMWORD PTR 16[rsp], ymm0
setb	al
movzx	ecx, cl
cmp	r15b, 8
vpinsrd	xmm0, xmm5, esi, 1
lea	eax, 1[rax+rcx*2]
vpunpcklqdq	xmm0, xmm1, xmm0
vmovq	QWORD PTR 64[rsp], xmm2
cmove	eax, ebp
vmovdqu	XMMWORD PTR 48[rsp], xmm0
cdqe
```

## Benchmark

#### Linux
```text
AMD Ryzen 7 5700U, linux clang 18.0-pre (trunk)
100 repetitions, median value, cv < 1.5%
Run on (16 X 4369.92 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x8)
  L1 Instruction 32 KiB (x8)
  L2 Unified 512 KiB (x8)
  L3 Unified 4096 KiB (x2)
```
| *                  | BM_process/EQ_1  | BM_process/R_1  | BM_process/EQ_30  | BM_process/R_30  | BM_process/EQ_120  | BM_process/R_120  | BM_process/RR_120  |
|--------------------|------------------|-----------------|-------------------|------------------|--------------------|-------------------|--------------------|
| Slow               | 7765ns           | 7561ns          | 880.889us         | 421.309us        | 3541.31us          | 1653.81us         | 786.595us          |
| UintS              | 7505ns           | 7284ns          | 799.938us         | 371.4us          | 3218.47us          | 1456.31us         | 740.281us          |
| UintBranchLess     | 28.147us         | 28.131us        | 680.196us         | 680.128us        | 2719.86us          | 2719.91us         | 2719.76us          |
| MemoizedS          | 935ns            | 947ns           | 26.63us           | 30.815us         | 105.747us          | 229.453us         | 381.315us          |
| MemoizedSAligned   | 918ns            | 927ns           | 29.025us          | 31.438us         | 113.777us          | 215.995us         | 355.219us          |
| MemoizedBranchLess | 5340ns           | 5340ns          | 128.145us         | 128.099us        | 512.331us          | 512.296us         | 512.204us          |

#### Windows
```text
Ryzen 7 3700X, windows MinGW 11
100 repetitions, median value, cv < 1.3%
Run on (16 X 3593 MHz CPU s)
CPU Caches:
L1 Data 32 KiB (x8)
L1 Instruction 32 KiB (x8)
L2 Unified 512 KiB (x8)
L3 Unified 16384 KiB (x2)
```
| Category           | BM_process/EQ_1 | BM_process/R_1 | BM_process/EQ_30 | BM_process/R_30 | BM_process/EQ_120 | BM_process/R_120 | BM_process/RR_120 |
|--------------------|-----------------|----------------|------------------|-----------------|-------------------|------------------|-------------------|
| Slow               | 10986ns         | 10603ns        | 962.182us        | 465.21us        | 3840.78us         | 1804.36us        | 836.68us          |
| UintS              | 5390ns          | 5580ns         | 854.492us        | 399.013us       | 3446.69us         | 1586.91us        | 634.766us         |
| UintBranchLess     | 36.901us        | 36.83us        | 878.514us        | 899.431us       | 3605.77us         | 3523.28us        | 3525.64us         |
| MemoizedS          | 1256ns          | 1256ns         | 29.82us          | 34.816us        | 119.629us         | 254.981us        | 376.607us         |
| MemoizedSAligned   | 1283ns          | 1378ns         | 32.958us         | 35.313us        | 117.188us         | 253.906us        | 376.607us         |
| MemoizedBranchLess | 7847ns          | 7847ns         | 184.168us        | 184.168us       | 749.86us          | 749.86us         | 732.422us         |

### Test cases
Test distribution:
- `EQ_*` is bench test with string that contains equal count of `0` and `1`. (`bernoulli_distribution(0.5)`).
- `R_*` is string with more `0` than `1` (`bernoulli_distribution(0.2)`).
- `RR_*` is string with significantly more `0` than `1` (`bernoulli_distribution(0.05)`).

Test size:
- `*_1`: 45 symbols, several loop for any solution
- `*_30`: ~30 Kb, L1-cache for all except `Uint*` solutions
- `*_120`: ~120 Kb, L2-cache for all except `Uint*` solutions

### Speed-up between test cases
Speed-up between `EQ_120` to `*_120`. First value is linux speed-up, second is windows.

| Category           | R_120         | RR_120        |
|--------------------|---------------|---------------|
| Slow               | x2.14 # x2.13 | x4.50 # x4.59 |
| UintS              | x2.21 # x2.17 | x4.35 # x5.43 |
| UintBranchLess     | x1.00 # x1.02 | x1.00 # x1.02 |
| MemoizedS          | x0.46 # x0.47 | x0.28 # x0.32 |
| MemoizedSAligned   | x0.53 # x0.46 | x0.32 # x0.31 |
| MemoizedBranchLess | x1.00 # x1.00 | x1.00 # x1.02 |


### Speed-up between implementations
Speed-up between Slow implementation and another one. First value is linux speed-up, second is windows.

| Category           | EQ_120          | R_120         | RR_120        |
|--------------------|-----------------|---------------|---------------|
| Slow               | x1              | x1            | x1            |
| UintS              | x1.10  # x1.11  | x1.14 # x1.14 | x1.06 # x1.32 |
| UintBranchLess     | x1.30  # x1.07  | x0.61 # x0.51 | x0.29 # x0.24 |
| MemoizedS          | x33.49 # x32.11 | x7.21 # x7.08 | x2.06 # x2.22 |
| MemoizedSAligned   | x31.13 # x32.77 | x7.66 # x7.11 | x2.21 # x2.22 |
| MemoizedBranchLess | x6.91  # x5.12  | x3.23 # x2.41 | x1.54 # x1.14 |

## Explain
### NAIVE `vector<bool>`
Naive solution processes only one element per cycle and has branchy `if (t == 1)`. 
For `EQ_*`, it gives a lot of branch mis-predictions. 
But if source data has more `0` (or `1`) in string, it speeds-up this solution.
So, mis-prediction leads huge time spending, time difference with and without mis-predictions is x2.15 and x4.50.

#### NAIVE `vector<uint8_t>`
This implementation slightly better than `vector<bool>` because spends less instruction per loop. 
But has some problem with cache lines, because it occupies in 8 times more memory, so `*_30`, `*_120` tests go to another cache level.
Speed-up in small tests compare to `vector<bool>` is about x1.95 for windows and x1.0 for linux.
> Need to check linux implementation of `vector<bool>`

For windows, speed-up is significantly reduce when the size is increased. And the speed-up for long strings is about 10%.
> This implementation has the same problem with branch mis-predictions

### NAIVE branch-less
Because branch-less doesn't use `if` in the main loop, branch-less implementation have very stable time for any test case distribution.
The speed doesn't depend on test.
But, the asm code in loop is large and cannot be filled in one cache line. So, we have huge back-end idle and high iTLB cache misses.
> See Memoized branch-less

### Memoized
This version processes 8 symbols in one loop cycle, so, the speed-up is greate just on this moment. 
The code for merge results is fast and occupies several instructions. 
Also, we have 3 if branch in loop:
- `if (np == 8)`: case when we have 8 `0` in batch.
This is the main mis-prediction branch for non `EQ_*` test cases:
1. Rarely for `EQ_*` (0.3%)
2. Seldom for `R_*` (16.7%)
3. Often for `RR_*` (66.34%) 
- `if (longest > longestSeqSize) [[unlikely]]`: when the longest sequence is inside batch
Can be passed only 6 times, so mis-prediction is miserable
- `if (lp > longestSeqSize) [[unlikely]]`: when the left sequence ended in the batch
Rarely for any test case

> `EQ_120` branch-misses 0.03% vs `RR_120` branch-misses 11.64%

##### `EQ_120`
> `perf stat -ddd`
```text
         15,779.15 msec task-clock                       #    1.000 CPUs utilized             
                45      context-switches                 #    2.852 /sec                      
                 0      cpu-migrations                   #    0.000 /sec                      
             1,036      page-faults                      #   65.656 /sec                      
    68,073,803,810      cycles                           #    4.314 GHz                         (33.34%)
        45,545,604      stalled-cycles-frontend          #    0.07% frontend cycles idle        (33.35%)
     2,468,310,925      stalled-cycles-backend           #    3.63% backend cycles idle         (33.35%)
   313,033,620,392      instructions                     #    4.60  insn per cycle            
                                                  #    0.01  stalled cycles per insn     (33.35%)
    82,264,951,313      branches                         #    5.214 G/sec                       (33.35%)
        24,416,726      branch-misses                    #    0.03% of all branches             (33.34%)
    66,441,914,696      L1-dcache-loads                  #    4.211 G/sec                       (33.33%)
       257,591,068      L1-dcache-load-misses            #    0.39% of all L1-dcache accesses   (33.33%)
   <not supported>      LLC-loads                                                             
   <not supported>      LLC-load-misses                                                       
       698,109,507      L1-icache-loads                  #   44.243 M/sec                       (33.33%)
           500,054      L1-icache-load-misses            #    0.07% of all L1-icache accesses   (33.33%)
           364,695      dTLB-loads                       #   23.112 K/sec                       (33.33%)
            22,643      dTLB-load-misses                 #    6.21% of all dTLB cache accesses  (33.33%)
             7,240      iTLB-loads                       #  458.833 /sec                        (33.33%)
             1,050      iTLB-load-misses                 #   14.50% of all iTLB cache accesses  (33.33%)
       256,416,974      L1-dcache-prefetches             #   16.250 M/sec                       (33.33%)
   <not supported>      L1-dcache-prefetch-misses
```
##### `RR_120`
> `perf stat -ddd`
```text
         14,834.29 msec task-clock                       #    1.000 CPUs utilized             
                60      context-switches                 #    4.045 /sec                      
                 0      cpu-migrations                   #    0.000 /sec                      
             1,035      page-faults                      #   69.771 /sec                      
    64,193,351,934      cycles                           #    4.327 GHz                         (33.31%)
     2,125,627,177      stalled-cycles-frontend          #    3.31% frontend cycles idle        (33.34%)
       531,394,385      stalled-cycles-backend           #    0.83% backend cycles idle         (33.37%)
    68,818,904,389      instructions                     #    1.07  insn per cycle            
                                                  #    0.03  stalled cycles per insn     (33.39%)
    15,076,700,227      branches                         #    1.016 G/sec                       (33.40%)
     1,755,532,928      branch-misses                    #   11.64% of all branches             (33.39%)
    20,437,895,169      L1-dcache-loads                  #    1.378 G/sec                       (33.36%)
        79,155,655      L1-dcache-load-misses            #    0.39% of all L1-dcache accesses   (33.33%)
   <not supported>      LLC-loads                                                             
   <not supported>      LLC-load-misses                                                       
       779,452,307      L1-icache-loads                  #   52.544 M/sec                       (33.31%)
           521,228      L1-icache-load-misses            #    0.07% of all L1-icache accesses   (33.30%)
           334,151      dTLB-loads                       #   22.526 K/sec                       (33.30%)
            18,330      dTLB-load-misses                 #    5.49% of all dTLB cache accesses  (33.30%)
             9,582      iTLB-loads                       #  645.936 /sec                        (33.30%)
             1,576      iTLB-load-misses                 #   16.45% of all iTLB cache accesses  (33.30%)
        73,412,388      L1-dcache-prefetches             #    4.949 M/sec                       (33.30%)
   <not supported>      L1-dcache-prefetch-misses
```

### Memoized aligned
The manual aligning main loop address by 128 bytes gives only small speed improvement for a dis-balance tests and only for linux.
Because it makes code harder to read and disable some internal compiler choose of alignment, 
that can depend on another code or based on profile (`-fprofile-instr-use` for clang, `-fprofile-use` for gcc), 
this optimization doesn't look needed. 

> Clang18 supports `[[clang::code_align(128)]]` for easy align loop

### Memoized branch-less
Because branch-less doesn't use `if` in the main loop, branch-less implementation have very stable time for any distribution.
You have branch-misses 0.07% of all branches for any test case.
But, the asm code in loop is very large. Code should load much more instruction cache and, as conclusion, iTLB. 
For example, for linux you have 75% backed idle and 30% iTLB cache misses.

> `26k iTLB-loads` vs `8k iTLB-loads` for memoized common (brachy) code
```text
         64,227.86 msec task-clock                       #    1.000 CPUs utilized             
               284      context-switches                 #    4.422 /sec                      
                 0      cpu-migrations                   #    0.000 /sec                      
             1,039      page-faults                      #   16.177 /sec                      
   277,520,035,010      cycles                           #    4.321 GHz                         (33.32%)
        77,234,745      stalled-cycles-frontend          #    0.03% frontend cycles idle        (33.33%)
   206,204,882,500      stalled-cycles-backend           #   74.30% backend cycles idle         (33.33%)
   833,882,609,154      instructions                     #    3.00  insn per cycle            
                                                  #    0.25  stalled cycles per insn     (33.34%)
    25,840,323,282      branches                         #  402.323 M/sec                       (33.34%)
        18,482,933      branch-misses                    #    0.07% of all branches             (33.35%)
   357,661,562,915      L1-dcache-loads                  #    5.569 G/sec                       (33.35%)
       134,092,044      L1-dcache-load-misses            #    0.04% of all L1-dcache accesses   (33.35%)
   <not supported>      LLC-loads                                                             
   <not supported>      LLC-load-misses                                                       
     1,492,519,001      L1-icache-loads                  #   23.238 M/sec                       (33.34%)
         1,809,506      L1-icache-load-misses            #    0.12% of all L1-icache accesses   (33.34%)
         1,004,434      dTLB-loads                       #   15.639 K/sec                       (33.33%)
            58,516      dTLB-load-misses                 #    5.83% of all dTLB cache accesses  (33.33%)
            25,909      iTLB-loads                       #  403.392 /sec                        (33.32%)
             7,933      iTLB-load-misses                 #   30.62% of all iTLB cache accesses  (33.32%)
       130,458,746      L1-dcache-prefetches             #    2.031 M/sec                       (33.32%)
   <not supported>      L1-dcache-prefetch-misses
```
