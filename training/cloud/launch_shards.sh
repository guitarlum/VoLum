#!/bin/bash
# Launch a range of shards as tmux sessions on this pod.
# Usage: NSHARD=12 EPOCHS=700 SHARD_LO=0 SHARD_HI=5 bash launch_shards.sh
# (this pod handles shard ids SHARD_LO..SHARD_HI of NSHARD total across the fleet)
NSHARD="${NSHARD:-1}"
EPOCHS="${EPOCHS:-700}"
SHARD_LO="${SHARD_LO:-0}"
SHARD_HI="${SHARD_HI:-0}"
THREADS="${THREADS:-2}"
CAP_DIR="${CAP_DIR:-/workspace/captures_full}"
ESCALATE_AT="${ESCALATE_AT:-660}"
A2_BASE="${A2_BASE:-/workspace/a2}"
NAMES="${NAMES:-}"
mkdir -p "$A2_BASE/podlogs"
for k in $(seq "$SHARD_LO" "$SHARD_HI"); do
  tmux new-session -d -s "train$k" \
    "SHARD=$k NSHARD=$NSHARD EPOCHS=$EPOCHS THREADS=$THREADS CAP_DIR=$CAP_DIR ESCALATE_AT=$ESCALATE_AT A2_BASE=$A2_BASE NAMES=$NAMES bash /workspace/a2/run_batch_cloud.sh > $A2_BASE/podlogs/shard$k.log 2>&1"
  echo "launched shard $k / $NSHARD (epochs=$EPOCHS threads=$THREADS esc_at=$ESCALATE_AT base=$A2_BASE)"
done
tmux ls
