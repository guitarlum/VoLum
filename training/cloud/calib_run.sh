#!/bin/bash
# Calibration launcher. EP env controls epochs (default 40).
cd /workspace/a2
EP="${EP:-40}"
case "$1" in
  reset)
    tmux kill-server 2>/dev/null
    rm -f results_*.csv podlogs/*.log
    rm -rf runs/* out/*
    echo "RESET $(date -Is)" ;;
  one)
    tmux new-session -d -s q0 "SHARD=0 NSHARD=8 EPOCHS=$EP THREADS=$THREADS bash run_batch_cloud.sh > podlogs/q0.log 2>&1"
    echo "STARTED_ONE ep=$EP threads=$THREADS $(date -Is)" ;;
  many)
    N="${2:-4}"
    for k in $(seq 1 "$N"); do
      tmux new-session -d -s "q$k" "SHARD=$k NSHARD=8 EPOCHS=$EP THREADS=$THREADS bash run_batch_cloud.sh > podlogs/q$k.log 2>&1"
    done
    echo "STARTED_MANY n=$N ep=$EP threads=$THREADS $(date -Is)" ;;
  *) echo "usage: [EP=40] calib_run.sh reset|one|many <N>"; exit 1 ;;
esac
tmux ls 2>/dev/null || echo "(none)"
