#!/usr/bin/env bash
# eigen is hosted on GitLab, which answers "unable to handle this request" when
# it is under load. actions/checkout stops after Git's single retry. Keep trying.
set -u

attempts=8
for attempt in $(seq 1 "$attempts"); do
  if git submodule update --init --recursive; then
    exit 0
  fi
  echo "submodule checkout failed (attempt ${attempt}/${attempts})"
  if [[ "$attempt" -eq "$attempts" ]]; then
    echo "submodule checkout failed after ${attempts} attempts" >&2
    exit 1
  fi
  sleep $((attempt * 20))
done
