# P1 Minimal Service Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the smallest runnable C++ service that loads config, writes level-aware logs, returns unified JSON, serves static files, and exposes `/api/health`.

**Architecture:** Keep P1 as a thin vertical slice. `shuati_core` owns reusable config, logging, JSON, and server route helpers; `shuati_server` only loads config and starts cpp-httplib. Unit tests cover behavior without requiring a live HTTP port.

**Tech Stack:** C++17, CMake, cpp-httplib v0.48.0, Google Test, native HTML/CSS/JavaScript.

---

### Task 1: P1 Tests First

**Files:**
- Create: `tests/unit/logging_test.cpp`
- Create: `tests/unit/json_response_test.cpp`
- Create: `tests/unit/app_config_test.cpp`
- Create: `tests/unit/server_test.cpp`
- Modify: `CMakeLists.txt`

- [x] Add gtest cases for log level parsing, invalid log levels, and level filtering.
- [x] Add gtest cases for unified JSON response shape and JSON string escaping.
- [x] Add gtest cases for app config defaults and configured values.
- [x] Add gtest cases for health-check response payload.
- [x] Attempt CMake/test to record the current red state. If the compiler is still missing, record the environment blocker.

### Task 2: Core P1 Implementation

**Files:**
- Create: `include/shuati/common/logging.h`
- Create: `src/common/logging.cpp`
- Create: `include/shuati/common/json_response.h`
- Create: `src/common/json_response.cpp`
- Create: `include/shuati/app/app_config.h`
- Create: `src/app/app_config.cpp`
- Create: `include/shuati/app/server.h`
- Create: `src/app/server.cpp`
- Modify: `src/app/main.cpp`
- Modify: `CMakeLists.txt`

- [x] Implement `LogLevel` with `DEBUG`, `INFO`, `WARN`, and `ERROR`.
- [x] Implement append-only file logging with directory creation, timestamp, level, and level filtering.
- [x] Implement `makeJsonResponse(code, message, dataJson)` and JSON string escaping.
- [x] Implement `AppConfig::loadFromFile`, including defaults for P1 fields.
- [x] Implement cpp-httplib route registration for `/api/health`, static file mount, 404 JSON, and access logging.
- [x] Update `main.cpp` to load `config/app.yaml`, configure loggers, and start the service.

### Task 3: Static Frontend

**Files:**
- Create: `public/index.html`
- Create: `public/assets/styles.css`
- Create: `public/assets/app.js`

- [x] Add a restrained white tool-style homepage.
- [x] Use JavaScript to call `/api/health` and show service status.
- [x] Keep the page minimal; no framework and no marketing-style hero.

### Task 4: Docs, Verification, And Git

**Files:**
- Modify: `config/app.yaml`
- Modify: `SPEC.md`
- Modify: `docs/tech-logs/2026-07-02.md`
- Modify: `docs/superpowers/plans/2026-07-02-p1-minimal-service.md`

- [x] Add `logs.level` to the sample config and document the default.
- [x] Update the P1 checklist in `SPEC.md`.
- [ ] Update the technical log with implementation notes, commands, and blockers.
- [x] Run `git diff --check`.
- [x] Run CMake/build/tests if `g++` is available; otherwise capture the exact blocker.
- [x] Commit and push to GitHub.
