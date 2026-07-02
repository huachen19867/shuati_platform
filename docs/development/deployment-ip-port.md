# IP:Port Deployment Smoke Test

This MVP is designed to run on one Linux host and serve both static frontend
files and REST APIs from the same C++ process.

## Build

```bash
cmake -S . -B build/p5p7 -DSHUATI_BUILD_TESTS=ON
cmake --build build/p5p7 -- -j2
ctest --test-dir build/p5p7 --output-on-failure
```

## Start

Use `config/app.yaml` as the baseline and change only the host/port or data
paths that differ on the target machine:

```bash
./build/p5p7/shuati_server --config config/app.yaml
```

For LAN testing, keep `server.host: "0.0.0.0"` and visit:

```text
http://<server-ip>:8080/
```

For local smoke testing, use the bundled scripts:

```bash
scripts/api_smoke_p7.py
scripts/smoke_p3p4.sh
```

The browser smoke requires Python Playwright:

```bash
python3 -m pip install --user playwright
python3 -m playwright install chromium
scripts/web_smoke_p5.py
```

## Expected Checks

- `GET /api/health` returns `{code,message,data}` with `data.status = "ok"`.
- The root page loads the native HTML/CSS/JavaScript app.
- `root / secret123` can sign in as the bootstrap super admin in default config.
- Admin can create a problem and add a testcase through the MVP JSON endpoint.
- A normal user can submit C++ and see the final submission detail.
- Access, error, and judge logs are written to the configured log files.

