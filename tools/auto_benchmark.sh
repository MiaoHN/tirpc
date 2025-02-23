#!/bin/bash

# 配置区（根据实际情况修改）
TEST_ROUNDS=30    # 总测试轮次
SERVER_PORT=19999 # 服务器端口
QPS_ENDPOINT="http://127.0.0.1:${SERVER_PORT}/qps?id=1"
WRK_OPTIONS="-c 1000 -t 8 -d 20 --latency"
SERVER_CMD="./rpc_http_server"
OUTPUT_FILE="benchmark_results.csv"
PY_ANALYSIS_SCRIPT="analyze_results.py"

# ------------------- 以下内容无需修改 -------------------
# 初始化环境
echo "初始化测试环境..."
sudo cpupower frequency-set -g performance >/dev/null 2>&1    # 锁定CPU频率
sync && echo 3 | sudo tee /proc/sys/vm/drop_caches >/dev/null # 清缓存

# 创建结果文件
echo "type,qps" >$OUTPUT_FILE

for ((i = 1; i <= TEST_ROUNDS; i++)); do
  echo -e "\n第 ${i}/${TEST_ROUNDS} 轮测试开始..."

  # 测试带锁队列
  echo "[带锁队列] 启动服务器..."
  taskset -c 0-7 $SERVER_CMD --use-locked-queue >/dev/null 2>&1 &
  SERVER_PID=$!
  sleep 5 # 等待服务器启动

  echo "[带锁队列] 运行wrk测试..."
  wrk $WRK_OPTIONS $QPS_ENDPOINT | awk '/Requests\/sec/ {print "locked,"$2}' >>$OUTPUT_FILE
  kill $SERVER_PID
  wait $SERVER_PID 2>/dev/null

  # 测试无锁队列
  echo "[无锁队列] 启动服务器..."
  taskset -c 0-7 $SERVER_CMD --use-lock-free-queue >/dev/null 2>&1 &
  SERVER_PID=$!
  sleep 5

  echo "[无锁队列] 运行wrk测试..."
  wrk $WRK_OPTIONS $QPS_ENDPOINT | awk '/Requests\/sec/ {print "lockfree,"$2}' >>$OUTPUT_FILE
  kill $SERVER_PID
  wait $SERVER_PID 2>/dev/null
done

# 生成分析报告
echo -e "\n生成分析报告..."
python3 $PY_ANALYSIS_SCRIPT

# 恢复环境
sudo cpupower frequency-set -g ondemand >/dev/null 2>&1
