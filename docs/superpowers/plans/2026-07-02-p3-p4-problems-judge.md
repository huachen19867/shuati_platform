# P3/P4 Problems And Judge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the MVP problem management, testcase management, submission, and judge result flow.

**Architecture:** Add focused domain services for problems, testcases, submissions, and judging. Keep runtime storage in in-memory repositories for the current no-MySQL environment, while extending `sql/schema.sql` so the MySQL boundary is clear. Use a local C++ runner when Docker is unavailable, and document the sandbox gap explicitly.

**Tech Stack:** C++17, cpp-httplib, Google Test, CMake, MySQL schema, local `g++` runner with Linux `timeout`.

---

### Task 1: Configuration And Schema

**Files:**
- Modify: `include/shuati/app/app_config.h`
- Modify: `src/app/app_config.cpp`
- Modify: `tests/unit/app_config_test.cpp`
- Modify: `sql/schema.sql`

- [x] **Step 1: Write failing config tests**

Add assertions that `judge.*` and `storage.*` fields are loaded from YAML and have conservative defaults.

- [x] **Step 2: Run config tests to verify RED**

Run: `cmake --build build/p3p4 -- -j2 && ./build/p3p4/shuati_tests --gtest_filter=AppConfigTest.*`

Expected: compile failure or test failure because the fields are not yet exposed.

- [x] **Step 3: Implement config fields**

Add `JudgeConfig` and `StorageConfig`, parse the existing `config/app.yaml` fields, and keep defaults aligned with `SPEC.md`.

- [x] **Step 4: Extend MySQL schema**

Add tables for `problems`, `testcases`, `submissions`, `submission_cases`, and `judge_tasks` with indexes for list/filter and queue claim.

- [x] **Step 5: Verify GREEN**

Run: `cmake --build build/p3p4 -- -j2 && ./build/p3p4/shuati_tests --gtest_filter=AppConfigTest.*`

Expected: all selected tests pass.

### Task 2: Problems And Testcases Domain

**Files:**
- Create: `include/shuati/problem/problem_service.h`
- Create: `src/problem/problem_service.cpp`
- Create: `include/shuati/problem/testcase_service.h`
- Create: `src/problem/testcase_service.cpp`
- Create: `tests/unit/problem_service_test.cpp`
- Create: `tests/unit/testcase_service_test.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write failing problem service tests**

Cover admin-only create/update, public list with keyword/difficulty/tag filters, and detail lookup.

- [x] **Step 2: Write failing testcase service tests**

Cover safe `.in/.out` pairing, path traversal rejection, missing pair diagnostics, and whole-package overwrite.

- [x] **Step 3: Run selected tests to verify RED**

Run: `cmake --build build/p3p4 -- -j2 && ./build/p3p4/shuati_tests --gtest_filter='ProblemServiceTest.*:TestcaseServiceTest.*'`

Expected: compile failure because services do not exist yet.

- [x] **Step 4: Implement minimal domain services**

Use mutex-protected in-memory repositories and filesystem-backed testcase writes under `storage.testcase_dir/problem_{id}`.

- [x] **Step 5: Verify GREEN**

Run: `cmake --build build/p3p4 -- -j2 && ./build/p3p4/shuati_tests --gtest_filter='ProblemServiceTest.*:TestcaseServiceTest.*'`

Expected: selected tests pass.

### Task 3: Submission Queue And Judge Runner

**Files:**
- Create: `include/shuati/judge/submission_service.h`
- Create: `src/judge/submission_service.cpp`
- Create: `include/shuati/judge/local_cpp_runner.h`
- Create: `src/judge/local_cpp_runner.cpp`
- Create: `tests/unit/submission_service_test.cpp`
- Create: `tests/unit/local_cpp_runner_test.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write failing output and runner tests**

Cover accepted output, wrong answer, compile error truncation, and per-test timing/stderr fields.

- [x] **Step 2: Write failing submission tests**

Cover creation as pending, atomic claim to running, owner/admin visibility rules, and final status aggregation.

- [x] **Step 3: Run selected tests to verify RED**

Run: `cmake --build build/p3p4 -- -j2 && ./build/p3p4/shuati_tests --gtest_filter='LocalCppRunnerTest.*:SubmissionServiceTest.*'`

Expected: compile failure because judge services do not exist yet.

- [x] **Step 4: Implement local MVP judge**

Compile submitted C++ with `g++`, run each testcase through `timeout`, compare normalized output, truncate compile/runtime diagnostics, and record memory as `0` until a Docker/cgroup runner replaces it.

- [x] **Step 5: Verify GREEN**

Run: `cmake --build build/p3p4 -- -j2 && ./build/p3p4/shuati_tests --gtest_filter='LocalCppRunnerTest.*:SubmissionServiceTest.*'`

Expected: selected tests pass.

### Task 4: HTTP API Integration

**Files:**
- Modify: `include/shuati/app/server.h`
- Modify: `src/app/server.cpp`
- Modify: `src/app/main.cpp`
- Modify: `tests/unit/server_test.cpp`

- [x] **Step 1: Write failing server route tests**

Cover problem list/detail, admin create problem, testcase upload metadata route, submit code, and owner-only submission detail.

- [x] **Step 2: Run server tests to verify RED**

Run: `cmake --build build/p3p4 -- -j2 && ./build/p3p4/shuati_tests --gtest_filter=ServerTest.*`

Expected: route tests fail with 404 or compile errors for missing service wiring.

- [x] **Step 3: Wire services into server**

Extend `configureServer` with optional problem/testcase/submission service pointers and add JSON REST routes using the existing `{code,message,data}` response format.

- [x] **Step 4: Update main service composition**

Construct problem, testcase, submission, and local runner services in `main.cpp`, and pass them to route registration.

- [x] **Step 5: Verify GREEN**

Run: `cmake --build build/p3p4 -- -j2 && ./build/p3p4/shuati_tests --gtest_filter=ServerTest.*`

Expected: selected tests pass.

### Task 5: Docs, Logs, Regression, Git

**Files:**
- Modify: `SPEC.md`
- Modify: `docs/tech-logs/2026-07-02.md`
- Modify: `docs/superpowers/plans/2026-07-02-p3-p4-problems-judge.md`

- [x] **Step 1: Update project checklist**

Mark completed P3/P4 items and explicitly leave Docker sandbox image as pending if Docker is still unavailable locally.

- [x] **Step 2: Update technical log**

Record reusable decisions, commands, paths, missing Docker/MySQL constraints, and verification commands.

- [x] **Step 3: Full regression**

Run: `cmake -S . -B build/p3p4 -DSHUATI_BUILD_TESTS=ON && cmake --build build/p3p4 -- -j2 && ctest --test-dir build/p3p4 --output-on-failure`

Expected: all tests pass.

- [x] **Step 4: HTTP smoke**

Start `build/p3p4/shuati_server --config config/app.yaml`, use curl to login as root, create a problem, upload testcases, register/login a user, submit accepted code, and query submission detail.

- [x] **Step 5: Commit and push**

Run: `git status --short`, `git add ...`, `git commit -m 'feat: add p3 p4 problem and judge flow'`, and push to `origin/main`.
