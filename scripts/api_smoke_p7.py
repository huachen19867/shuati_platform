#!/usr/bin/env python3
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

try:
    import requests
except ImportError as exc:
    raise SystemExit(
        "python requests is required: python3 -m pip install --user requests"
    ) from exc


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "p5p7" / "shuati_server"
BASE = "http://127.0.0.1:18082"


def write_config(tmp: Path) -> Path:
    for path in [
        tmp / "public",
        tmp / "logs",
        tmp / "data/testcases",
        tmp / "data/submissions",
        tmp / "data/judge_tmp",
    ]:
        path.mkdir(parents=True, exist_ok=True)
    config = tmp / "app.yaml"
    config.write_text(
        f"""
app:
  name: "shuati-platform"
  environment: "api-smoke"
server:
  host: "127.0.0.1"
  port: 18082
  public_dir: "{ROOT / 'public'}"
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
  temp_dir: "{tmp / 'data/judge_tmp'}"
storage:
  testcase_dir: "{tmp / 'data/testcases'}"
  submission_dir: "{tmp / 'data/submissions'}"
  source_retention_hours: 24
logs:
  level: "info"
  access: "{tmp / 'logs/access.log'}"
  error: "{tmp / 'logs/error.log'}"
  judge: "{tmp / 'logs/judge.log'}"
security:
  session_ttl_hours: 24
  submit_interval_seconds: 5
  upload_max_mb: 20
bootstrap:
  super_admin:
    enabled: true
    username: "root"
    password: "secret123"
""",
        encoding="utf-8",
    )
    return config


def request(method: str, path: str, token: str = "", **kwargs):
    headers = kwargs.pop("headers", {})
    if token:
        headers["X-Session-Token"] = token
    response = requests.request(method, f"{BASE}{path}", headers=headers, timeout=20, **kwargs)
    try:
        payload = response.json()
    except json.JSONDecodeError as exc:
        raise AssertionError(f"non-json response {response.status_code}: {response.text}") from exc
    if response.status_code >= 400 or payload.get("code") != 0:
        raise AssertionError(f"{method} {path} failed: {response.status_code} {payload}")
    return response, payload.get("data") or {}


def login(username: str, password: str) -> str:
    response, data = request(
        "POST",
        "/api/auth/login",
        json={"username": username, "password": password},
    )
    return data.get("token") or response.headers["X-Session-Token"]


def main() -> int:
    if not BUILD.exists():
        raise SystemExit(f"missing server binary: {BUILD}")
    tmp = Path(tempfile.mkdtemp(prefix="shuati-api-smoke-"))
    config = write_config(tmp)
    server = subprocess.Popen(
        [str(BUILD), "--config", str(config)],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
      for _ in range(80):
          try:
              request("GET", "/api/health")
              break
          except Exception:
              if server.poll() is not None:
                  raise AssertionError(server.stdout.read())
              time.sleep(0.1)
      else:
          raise AssertionError("server did not become healthy")

      root = login("root", "secret123")
      _, created = request(
          "POST",
          "/api/admin/problems",
          token=root,
          json={
              "title": "A Plus B",
              "statement": "Read two integers.",
              "input_description": "a b",
              "output_description": "a+b",
              "samples_json": "[]",
              "difficulty": "easy",
              "tags": "math,intro",
          },
      )
      problem_id = created["problem"]["id"]
      request(
          "POST",
          f"/api/admin/problems/{problem_id}/testcases",
          token=root,
          json={"input": "1 2\n", "output": "3\n"},
      )
      request("GET", "/api/problems?keyword=plus&difficulty=easy&tag=math")
      request("POST", "/api/auth/register", json={"username": "alice", "password": "password123"})
      alice = login("alice", "password123")
      _, submitted = request(
          "POST",
          f"/api/problems/{problem_id}/submissions",
          token=alice,
          json={
              "source": '#include <iostream>\nint main(){long long a,b; if(std::cin>>a>>b) std::cout<<a+b<<"\\n"; return 0;}'
          },
      )
      assert submitted["submission"]["status"] == "Accepted", submitted
      request("GET", f"/api/submissions/{submitted['submission']['id']}", token=alice)
      request("GET", "/api/admin/submissions", token=root)
      print("API_SMOKE_OK")
      return 0
    finally:
      server.terminate()
      try:
          server.wait(timeout=5)
      except subprocess.TimeoutExpired:
          server.kill()
      shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
