#!/bin/bash
# One-shot per-pod setup (pip/apt installs land on the pod, NOT the shared volume,
# so every fleet pod must run this once). Idempotent.
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq 2>&1 | tail -1
apt-get install -y -qq python3-tk tmux 2>&1 | tail -1
pip install --no-input -q "neural-amp-modeler==0.13.0" tensorboard 2>&1 | tail -2
python -c 'import nam.train.core as core; import torch; print("SETUP_OK nam", __import__("nam").__version__, "cuda", torch.cuda.is_available(), torch.cuda.get_device_name(0))'
nproc
echo "SETUP_DONE"
