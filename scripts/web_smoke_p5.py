#!/usr/bin/env python3
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

try:
    from playwright.sync_api import Error as PlaywrightError
    from playwright.sync_api import sync_playwright
except ImportError as exc:
    raise SystemExit(
        "python playwright is required: python3 -m pip install --user playwright && python3 -m playwright install chromium"
    ) from exc


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "p5p7" / "shuati_server"
BASE = "http://127.0.0.1:18083"


def write_config(tmp: Path) -> Path:
    for path in [
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
  environment: "web-smoke"
server:
  host: "127.0.0.1"
  port: 18083
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


def main() -> int:
    if not BUILD.exists():
        raise SystemExit(f"missing server binary: {BUILD}")
    tmp = Path(tempfile.mkdtemp(prefix="shuati-web-smoke-"))
    config = write_config(tmp)
    server = subprocess.Popen(
        [str(BUILD), "--config", str(config)],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        time.sleep(0.8)
        with sync_playwright() as p:
            try:
                browser = p.chromium.launch(headless=True)
            except PlaywrightError as exc:
                raise SystemExit(
                    "playwright chromium could not launch. Install browser system dependencies with: "
                    "sudo python3 -m playwright install-deps chromium"
                ) from exc
            page = browser.new_page(viewport={"width": 1366, "height": 900})
            page.goto(BASE)
            page.wait_for_load_state("networkidle")
            page.get_by_role("link", name="登录").click()
            page.get_by_label("用户名").fill("root")
            page.get_by_label("密码").fill("secret123")
            page.get_by_role("button", name="登录").click()
            page.wait_for_url("**/#/problems")
            page.get_by_role("link", name="后台").click()
            page.get_by_label("标题").fill("A Plus B")
            page.get_by_label("题面").fill("Read two integers.")
            page.get_by_label("输入描述").fill("a b")
            page.get_by_label("输出描述").fill("a+b")
            page.get_by_label("标签，逗号分隔").fill("math,intro")
            page.get_by_role("button", name="创建题目").click()
            page.get_by_role("button", name="覆盖测试点").click()
            page.get_by_role("link", name="题目").click()
            page.get_by_role("link", name="打开").click()
            page.wait_for_selector(".ace_editor", timeout=10000)
            page.get_by_role("button", name="提交代码").click()
            page.wait_for_selector("text=Accepted", timeout=20000)
            browser.close()
        print("WEB_SMOKE_OK")
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
