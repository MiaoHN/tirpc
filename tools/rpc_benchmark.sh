#!/bin/bash

# 配置区（根据实际情况修改）
TEST_ROUNDS=30    # 总测试轮次
SERVER_PORT=19999 # 服务器端口
SERVER_CMD="./rpc_server"
CLIENT_CMD="./rpc_client"
OUTPUT_FILE="rpc_bench_results.csv"
PY_ANALYSIS_SCRIPT="analyze_rpc_results.py"

# ------------------- 以下内容无需修改 -------------------
# 初始化环境
echo "初始化测试环境..."
sudo cpupower frequency-set -g performance >/dev/null 2>&1
sync && echo 3 | sudo tee /proc/sys/vm/drop_caches >/dev/null

# 创建结果文件
echo "queue_type,test_type,total_calls,success_calls,total_time_ms,avg_time_ms" >$OUTPUT_FILE

benchmark() {
  local queue_type=$1
  echo -e "\n测试队列类型: $queue_type"

  # 启动服务器
  echo "启动服务器..."
  taskset -c 0-7 $SERVER_CMD --$queue_type >/dev/null 2>&1 &
  SERVER_PID=$!

  # 等待服务器就绪
  for _ in {1..10}; do
    nc -z 127.0.0.1 $SERVER_PORT && break
    sleep 1
  done

  # 运行客户端测试
  echo "运行客户端..."
  $CLIENT_CMD | awk -v qt="$queue_type" '{
    if (/Single Channel Test - Total calls:/) {
        match($0, /Total calls: ([0-9]+)/, a); total = a[1];
        match($0, /Successful calls: ([0-9]+)/, b); success = b[1];
        getline; match($0, /Total time: ([0-9.]+)/, c); time = c[1];
        getline; match($0, /Average time per successful call: ([0-9.]+)/, d); avg = d[1];
        printf "%s,single,%d,%d,%.2f,%.2f\n", qt, total, success, time, avg
    }
    if (/Multiple Channels Test - Total calls:/) {
        match($0, /Total calls: ([0-9]+)/, a); total = a[1];
        match($0, /Successful calls: ([0-9]+)/, b); success = b[1];
        getline; match($0, /Total time: ([0-9.]+)/, c); time = c[1];
        getline; match($0, /Average time per successful call: ([0-9.]+)/, d); avg = d[1];
        printf "%s,multi,%d,%d,%.2f,%.2f\n", qt, total, success, time, avg
    }
    }' >>$OUTPUT_FILE

  # 关闭服务器
  kill $SERVER_PID
  wait $SERVER_PID 2>/dev/null
}

for ((i = 1; i <= TEST_ROUNDS; i++)); do
  echo -e "\n第 ${i}/${TEST_ROUNDS} 轮测试开始..."
  benchmark "use-locked-queue"    # 测试带锁队列
  benchmark "use-lock-free-queue" # 测试无锁队列
done

# 生成分析报告
echo -e "\n生成分析报告..."
python3 $PY_ANALYSIS_SCRIPT

# 恢复环境
sudo cpupower frequency-set -g ondemand >/dev/null 2>&1
