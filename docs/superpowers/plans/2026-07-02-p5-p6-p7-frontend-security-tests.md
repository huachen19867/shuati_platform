# P5/P6/P7 Frontend Security Tests Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver the browser-facing MVP, strengthen basic security/operations, and add reusable API/Web verification assets.

**Architecture:** Keep the frontend as a framework-free SPA served by the C++ process. Use Ace.js for the code editor instead of a handmade editor. Add backend security controls in focused C++ services where possible, and document environment-limited items honestly.

**Tech Stack:** C++17, cpp-httplib, Google Test, native HTML/CSS/JavaScript, Ace.js, curl, Python requests, Playwright.

---

### Task 1: Frontend App Shell And Ace Editor

**Files:**
- Modify: `public/index.html`
- Modify: `public/assets/styles.css`
- Modify: `public/assets/app.js`
- Test: `scripts/web_smoke_p5.py`

- [x] **Step 1: Create frontend routes**

Add hash routes for login, register, problems, problem detail, my submissions, admin problems, admin users, and admin submissions.

- [x] **Step 2: Integrate Ace.js**

Load Ace.js on the problem detail page and use it as the primary C++ source editor.

- [x] **Step 3: Implement API wiring**

Use `X-Session-Token` from localStorage, show unified `{code,message,data}` errors, and poll submission detail every 1 second until a terminal status.

- [x] **Step 4: Implement visual system**

Use a quiet white, thin-border, work-focused design with visible labels, clear focus states, and responsive layouts.

- [x] **Step 5: Verify with browser smoke**

Run the web smoke script against a local server and verify login, admin problem creation, testcase upload, Ace-backed submission, and result detail.

Result: WSL Python Playwright script is present but blocked by missing Chromium system libraries. Desktop-side Playwright MCP verified the same browser flow successfully: root login, admin problem creation, testcase upload, Ace editor mount, C++ submit, and `Accepted` result detail.

### Task 2: P6 Security And Operational Basics

**Files:**
- Modify: `include/shuati/auth/password_hasher.h`
- Modify: `src/auth/password_hasher.cpp`
- Modify: `CMakeLists.txt`
- Create: `include/shuati/common/rate_limiter.h`
- Create: `src/common/rate_limiter.cpp`
- Modify: `include/shuati/problem/testcase_service.h`
- Modify: `src/problem/testcase_service.cpp`
- Modify: `include/shuati/judge/submission_service.h`
- Modify: `src/judge/submission_service.cpp`
- Modify: `src/app/server.cpp`
- Create: `docker/cpp/Dockerfile`
- Create: `scripts/judge_docker_run_example.sh`
- Test: `tests/unit/password_hasher_test.cpp`
- Test: `tests/unit/rate_limiter_test.cpp`
- Test: update existing testcase/submission tests

- [x] **Step 1: Write failing security tests**

Cover bcrypt hash prefix/verification/legacy rejection, fixed-window limiter behavior, testcase package limits, submission interval limits, interrupted submission recovery, and expired source cleanup.

- [x] **Step 2: Implement bcrypt using system libcrypt**

Use `$2b$` hashes via `crypt_r`/`crypt`, keep legacy SHA-256 verification for existing MVP data, and link `crypt`.

- [x] **Step 3: Implement rate limits**

Apply basic register/login/upload/submit rate limiting in API routes and submission service.

- [x] **Step 4: Implement recovery and cleanup primitives**

Recover non-terminal submissions to `Pending` and delete retained source after the configured retention window.

- [x] **Step 5: Add Docker sandbox assets**

Add a C++ judge Dockerfile and a run example with `--network none`, memory, CPU, pids, read-only filesystem, and tempfs flags. Mark actual Docker execution as pending if local Docker is unavailable.

### Task 3: P7 Verification Assets And Documentation

**Files:**
- Create: `docs/development/api-curl-examples.md`
- Create: `scripts/api_smoke_p7.py`
- Create: `scripts/web_smoke_p5.py`
- Modify: `scripts/smoke_p3p4.sh`
- Modify: `SPEC.md`
- Modify: `docs/tech-logs/2026-07-02.md`

- [x] **Step 1: Add curl examples**

Document health, auth, admin problem creation, testcase upload, user submit, submission detail, and admin submissions examples.

- [x] **Step 2: Add Python requests smoke**

Create a reusable `requests` script that exercises the same API flow and exits non-zero on failure.

- [x] **Step 3: Add Playwright smoke**

Create a browser smoke that validates the frontend route flow and Ace editor presence.

- [x] **Step 4: Update SPEC checklist**

Mark completed P5/P6/P7 items, remove external visual-reference URL traces, and leave Docker/MySQL constrained items clearly pending.

- [x] **Step 5: Verify, commit, push**

Run full unit tests, API smoke, web smoke, check docs for removed reference traces, commit, and push to GitHub.
