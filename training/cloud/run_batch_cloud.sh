#!/bin/bash
# Run one shard of the cloud batch. Usage: SHARD=0 NSHARD=6 EPOCHS=700 bash run_batch_cloud.sh
export CAP_DIR="${CAP_DIR:-/workspace/captures}"
export A2_BASE="${A2_BASE:-/workspace/a2}"
export MPLBACKEND=Agg
export EPOCHS="${EPOCHS:-700}"
export LIMIT="${LIMIT:-0}"
export SHARD="${SHARD:-0}"
export NSHARD="${NSHARD:-1}"
export BATCH="${BATCH:-16}"
export ESCALATE_AT="${ESCALATE_AT:-660}"
export ESCALATE_EPOCHS="${ESCALATE_EPOCHS:-800}"
if [ -n "$THREADS" ]; then
  export OMP_NUM_THREADS="$THREADS" MKL_NUM_THREADS="$THREADS" OPENBLAS_NUM_THREADS="$THREADS" \
         NUMEXPR_NUM_THREADS="$THREADS" NUMBA_NUM_THREADS="$THREADS" VECLIB_MAXIMUM_THREADS="$THREADS"
  export TORCH_THREADS="${TORCH_THREADS:-$THREADS}"
fi
echo "[run_batch_cloud] SHARD=$SHARD/$NSHARD EPOCHS=$EPOCHS LIMIT=$LIMIT THREADS=${THREADS:-default} start $(date -Is)"
python -u /workspace/a2/batch_train_cloud.py
echo "[run_batch_cloud] SHARD=$SHARD end $(date -Is)"
