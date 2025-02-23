import pandas as pd
import numpy as np
from scipy import stats

# 读取数据
df = pd.read_csv("benchmark_results.csv")

# 数据清洗
df['qps'] = df['qps'].astype(float)

# 分组统计
locked = df[df['type'] == 'locked']['qps']
lockfree = df[df['type'] == 'lockfree']['qps']

# 统计描述
print(f"带锁队列：\n均值 {locked.mean():.2f} ± {locked.std():.2f}")
print(f"无锁队列：\n均值 {lockfree.mean():.2f} ± {lockfree.std():.2f}")

# 显著性检验
t_stat, p_val = stats.ttest_ind(locked, lockfree)
print(f"\nT检验结果：p={p_val:.4f} (若<0.05则差异显著)")

# 可视化（需要matplotlib）
try:
    import matplotlib.pyplot as plt
    plt.figure(figsize=(10, 6))
    plt.boxplot([locked, lockfree], labels=['Locked', 'Lock-Free'])
    plt.title('QPS Distribution Comparison')
    plt.ylabel('Requests/sec')
    plt.savefig('qps_comparison.png')
    print("\n已生成可视化图表: qps_comparison.png")
except:
    print("\n（未安装matplotlib，跳过可视化）")