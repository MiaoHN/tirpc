# TiRPC - Tiny RPC Framework

![tirpc](https://img.shields.io/github/v/release/MiaoHN/tirpc?color=2&label=tirpc&logoColor=2&style=plastic) ![GitHub License](https://img.shields.io/github/license/MiaoHN/tirpc)

A lightweight multi-threaded, multi-coroutine RPC framework implemented with reference to [Gooddbird/tinyrpc](https://github.com/Gooddbird/tinyrpc). For the core code principles, please refer to the [original author's article](https://www.zhihu.com/column/c_1515880656429510656).

## Performance Test

### Test Environment

- CPU: Intel Core i5-9300H @ 8x 2.4GHz
- RAM: 15934MiB
- OS: Ubuntu 20.04 jammy (on the Windows Subsystem for Linux)

### Test Results

```bash
wrk -c 1000 -t 8 -d 30 --latency 'http://127.0.0.1:19999/qps?id=1'
```

| IO thread | 1000     | 2000     | 5000     | 10000    |
| --------- | -------- | -------- | -------- | -------- |
| 1         | 8529.86  | 8710.45  | 8562.38  | 8809.32  |
| 4         | 38781.26 | 36080.29 | 37408.68 | 36920.82 |
| 16        | 72882.96 | 63116.69 | 64311.55 | 75873.56 |

## References

- [libco](https://github.com/Tencent/libco)
- [tinyrpc](https://github.com/Gooddbird/tinyrpc)
- [sylar](https://github.com/sylar-yin/sylar)
