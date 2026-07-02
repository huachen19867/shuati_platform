#!/usr/bin/env bash
set -euo pipefail

image="${1:-shuati-cpp-judge:latest}"
workdir="${2:?usage: scripts/judge_docker_run_example.sh IMAGE WORKDIR COMMAND...}"
shift 2

docker run --rm \
  --network none \
  --cpus "1.0" \
  --memory "128m" \
  --memory-swap "128m" \
  --pids-limit 64 \
  --read-only \
  --tmpfs /tmp:rw,noexec,nosuid,size=64m \
  --security-opt no-new-privileges \
  --cap-drop ALL \
  -v "$workdir:/work:rw" \
  -w /work \
  "$image" \
  "$@"
