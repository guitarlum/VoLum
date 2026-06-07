#!/bin/bash
# Quick status on a pod.
echo "=== tmux sessions ==="
tmux ls 2>/dev/null || echo "(none)"
echo "=== out count ==="
ls /workspace/a2/out/*.nam 2>/dev/null | wc -l
echo "=== GPU ==="
nvidia-smi --query-gpu=utilization.gpu,memory.used,memory.total --format=csv,noheader
echo "=== nproc / load ==="
nproc; uptime
echo "=== recent results (all shards) ==="
for f in /workspace/a2/results_*.csv; do [ -f "$f" ] && tail -n +2 "$f"; done 2>/dev/null | tail -20
