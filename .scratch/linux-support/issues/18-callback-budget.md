Status: claimed
Blocked by: 17-report

# 18 Callback budget gate

## Scope

May write:

- `iPlug2/IPlug/APP/**`, inside `OS_LINUX` or new Linux-only functions
- `NeuralAmpModeler/scripts/smoke-standalone-audio-linux.sh`
- `NeuralAmpModeler/scripts/linux/smoke/**`
- `NeuralAmpModeler/tests/test_iplug_app_linux_policy.cpp`
- `.github/workflows/ci.yml`, only the Linux standalone smoke invocation
- `NeuralAmpModeler/installer/changelog.txt`
- `backlog/` (two new QA items)
- this ticket and `reports/18-callback-budget.md`

## Work

- CI keeps the zero-JACK-xrun rule (`--strict-xruns`) and also checks the callback budget at 80 percent of the period.
- The box fails the 60 s window when a callback exceeds its period or 70 percent of that period. A jackd xrun whose callbacks stayed inside the budget is printed as a driver xrun and does not fail.
- `VOLUM_CALLBACK_TIMING=1` records callback durations. Unset, it does nothing. Linux only.
- Doctests own the budget math, the classifier and the env parse.
- Two backlog items: RealtimeSanitizer on the audio path, and a per-release callback-timing trend.
