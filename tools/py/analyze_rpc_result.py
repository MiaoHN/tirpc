import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from scipy import stats

# 读取数据
df = pd.read_csv("rpc_bench_results.csv")

# 数据预处理
df["throughput"] = df["success_calls"] / (
    df["total_time_ms"] / 1000
)  # 计算吞吐量（QPS）


# 按类型分组分析
def analyze_group(group_name):
    print(f"\n=== {group_name.upper()} TEST ANALYSIS ===")
    grouped = df[df["test_type"] == group_name]

    # 统计描述
    stats_df = grouped.groupby("queue_type").agg(
        {"avg_time_ms": ["mean", "std"], "throughput": ["mean", "std"]}
    )
    print(stats_df)

    # T检验
    locked = grouped[grouped["queue_type"] == "use-locked-queue"]["throughput"]
    lockfree = grouped[grouped["queue_type"] == "use-lock-free-queue"]["throughput"]
    t, p = stats.ttest_ind(locked, lockfree)
    print(f"\nT检验 p值: {p:.4f} (p<0.05表示差异显著)")

    # 可视化
    plt.figure(figsize=(12, 6))
    plt.subplot(1, 2, 1)
    sns.boxplot(x="queue_type", y="avg_time_ms", data=grouped)
    plt.title(f"{group_name} Latency")

    plt.subplot(1, 2, 2)
    sns.boxplot(x="queue_type", y="throughput", data=grouped)
    plt.title(f"{group_name} Throughput")

    plt.tight_layout()
    plt.savefig(f"{group_name}_performance.png")


analyze_group("single")
analyze_group("multi")
