### Benchmark

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
| *                  | BM_process/EQ_1  | BM_process/R_1  | BM_process/EQ_30  | BM_process/R_30  | BM_process/EQ_120  | BM_process/R_120  | BM_process/RR_120  |
|--------------------|------------------|-----------------|-------------------|------------------|--------------------|-------------------|--------------------|
| Slow               | 10986ns          | 10603ns         | 962.182us         | 465.21us         | 3840.78us          | 1804.36us         | 836.68us           |
| UintS              | 5390ns           | 5580ns          | 854.492us         | 399.013us        | 3446.69us          | 1586.91us         | 634.766us          |
| UintBranchLess     | 36.901us         | 36.83us         | 878.514us         | 899.431us        | 3605.77us          | 3523.28us         | 3525.64us          |
| MemoizedS          | 1256ns           | 1256ns          | 29.82us           | 34.816us         | 119.629us          | 254.981us         | 376.607us          |
| MemoizedSAligned   | 1283ns           | 1378ns          | 32.958us          | 35.313us         | 117.188us          | 253.906us         | 376.607us          |
| MemoizedBranchLess | 7847ns           | 7847ns          | 184.168us         | 184.168us        | 749.86us           | 749.86us          | 732.422us          |


