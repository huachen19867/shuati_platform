#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
smoke="${TMPDIR:-/tmp}/shuati-p3p4-smoke"

rm -rf "$smoke"
mkdir -p \
  "$smoke/public" \
  "$smoke/logs" \
  "$smoke/data/testcases" \
  "$smoke/data/submissions" \
  "$smoke/data/judge_tmp"

cat > "$smoke/app.yaml" <<CFG
app:
  name: "shuati-platform"
  environment: "smoke"
server:
  host: "127.0.0.1"
  port: 18081
  public_dir: "$smoke/public"
database:
  host: "127.0.0.1"
  port: 3306
  name: "shuati_platform"
  user: "shuati"
  password: "change-me"
  pool_size: 4
  acquire_timeout_ms: 3000
judge:
  docker_image: "shuati-cpp-judge:latest"
  workers: 4
  source_size_limit_kb: 64
  compile_timeout_ms: 10000
  run_timeout_ms: 2000
  memory_limit_mb: 128
  output_limit_kb: 1024
  compile_message_limit_kb: 8
  stderr_limit_kb: 4
  temp_dir: "$smoke/data/judge_tmp"
storage:
  testcase_dir: "$smoke/data/testcases"
  submission_dir: "$smoke/data/submissions"
  source_retention_hours: 24
logs:
  level: "info"
  access: "$smoke/logs/access.log"
  error: "$smoke/logs/error.log"
  judge: "$smoke/logs/judge.log"
security:
  session_ttl_hours: 24
  submit_interval_seconds: 5
  upload_max_mb: 20
bootstrap:
  super_admin:
    enabled: true
    username: "root"
    password: "secret123"
CFG

"$repo_root/build/p3p4/shuati_server" --config "$smoke/app.yaml" \
  > "$smoke/server.out" 2>&1 &
pid=$!
trap 'kill "$pid" >/dev/null 2>&1 || true; wait "$pid" >/dev/null 2>&1 || true' EXIT

for _ in $(seq 1 80); do
  if curl -fsS http://127.0.0.1:18081/api/health >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
  if ! kill -0 "$pid" >/dev/null 2>&1; then
    cat "$smoke/server.out"
    exit 1
  fi
done

curl -sS -D "$smoke/root.headers" -o "$smoke/root-login.json" \
  -H "Content-Type: application/json" \
  -d '{"username":"root","password":"secret123"}' \
  http://127.0.0.1:18081/api/auth/login >/dev/null
root_token="$(awk -F': ' 'tolower($1)=="x-session-token"{gsub("\r","",$2); print $2}' "$smoke/root.headers")"
test -n "$root_token"

cat > "$smoke/problem.json" <<'JSON'
{"title":"A Plus B","statement":"Read two integers.","input_description":"a b","output_description":"a+b","samples_json":"[]","difficulty":"easy","tags":"math,intro"}
JSON
curl -fsS -H "X-Session-Token: $root_token" \
  -H "Content-Type: application/json" \
  --data-binary @"$smoke/problem.json" \
  http://127.0.0.1:18081/api/admin/problems | grep -q "A Plus B"

curl -fsS -H "X-Session-Token: $root_token" \
  -H "Content-Type: application/json" \
  -d '{"input":"1 2\n","output":"3\n"}' \
  http://127.0.0.1:18081/api/admin/problems/1/testcases | grep -q "case_index"

curl -fsS "http://127.0.0.1:18081/api/problems?keyword=plus&difficulty=easy&tag=math" |
  grep -q "A Plus B"

curl -fsS -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"password123"}' \
  http://127.0.0.1:18081/api/auth/register >/dev/null

curl -sS -D "$smoke/alice.headers" -o "$smoke/alice-login.json" \
  -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"password123"}' \
  http://127.0.0.1:18081/api/auth/login >/dev/null
alice_token="$(awk -F': ' 'tolower($1)=="x-session-token"{gsub("\r","",$2); print $2}' "$smoke/alice.headers")"
test -n "$alice_token"

cat > "$smoke/submission.json" <<'JSON'
{"source":"#include <iostream>\nint main(){long long a,b; if(std::cin>>a>>b) std::cout<<a+b<<\"\\n\"; return 0;}"}
JSON
curl -fsS -H "X-Session-Token: $alice_token" \
  -H "Content-Type: application/json" \
  --data-binary @"$smoke/submission.json" \
  http://127.0.0.1:18081/api/problems/1/submissions |
  tee "$smoke/submitted.json" | grep -q "Accepted"

curl -fsS -H "X-Session-Token: $alice_token" \
  http://127.0.0.1:18081/api/submissions/1 |
  tee "$smoke/detail.json" | grep -q "case_index"

echo "SMOKE_OK"
