# TiRPC - Tiny RPC Framework

多线程多协程轻量 RPC 框架

## Examples

### RPC Service

![rpc_service](https://cdn.jsdelivr.net/gh/MiaoHN/image-host@master/images/202502142213545.png)

### Http Server

![http_server](https://cdn.jsdelivr.net/gh/MiaoHN/image-host@master/images/202502142240987.png)

## Benchmark

### 测试环境

- CPU: Intel Core i5-9300H @ 8x 2.4GHz
- RAM: 15934MiB
- OS: Ubuntu 20.04 jammy(on  the Windows Subsystem for Linux)

### 测试结果

```bash
wrk -c 1000 -t 8 -d 30 --latency 'http://127.0.0.1:19999/qps?id=1'
```

| IO thread | 1000     | 2000     | 5000     | 10000    |
| --------- | -------- | -------- | -------- | -------- |
| 1         | 8529.86  | 8710.45  | 8562.38  | 8809.32  |
| 4         | 38781.26 | 36080.29 | 37408.68 | 36920.82 |
| 16        | 72882.96 | 63116.69 | 64311.55 | 75873.56 |

## Reference

- [libco](https://github.com/Tencent/libco)
- [tinyrpc](https://github.com/Gooddbird/tinyrpc)
- [sylar](https://github.com/sylar-yin/sylar)
