# P0 Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the project foundation: directories, CMake, `.gitignore`, vendored `cpp-httplib`, dependency documentation, example config, and first Google Test based unit tests.

**Architecture:** Keep the first slice tiny: `shuati_core` is a C++ library for reusable code, `shuati_server` is the service entry, and `shuati_tests` validates core behavior. P0 includes a minimal config parser so the test suite has real code to exercise.

**Tech Stack:** C++17, CMake, cpp-httplib v0.48.0, Google Test, MySQL client dev package prepared for later phases.

---

### Task 1: Project Skeleton

**Files:**
- Create: `.gitignore`
- Create: `src/app/main.cpp`
- Create: `include/shuati/common/config.h`
- Create: `src/common/config.cpp`
- Create: `tests/unit/config_test.cpp`
- Create: `config/app.yaml`
- Create: `docs/development/dependencies.md`
- Create: `scripts/install_ubuntu_deps.sh`
- Create: `third_party/cpp-httplib/README.md`
- Create: `third_party/cpp-httplib/httplib.h`

- [x] Create directories for source, headers, tests, config, data, logs, scripts, docs, and third-party dependencies.
- [x] Add `.gitignore` entries for CMake build output, logs, runtime data, editor caches, and temporary judging files.
- [x] Add tracked `.gitkeep` files only where an otherwise-empty runtime directory must exist.

### Task 2: Test First For Config Parser

**Files:**
- Create: `tests/unit/config_test.cpp`

- [x] Write Google Test cases for parsing nested sections, integers, booleans, quoted strings, comments, and missing keys.
- [x] Attempt CMake/test before implementation; configuration failed earlier because the C++ compiler is missing.

### Task 3: Minimal CMake And Config Implementation

**Files:**
- Create: `CMakeLists.txt`
- Create: `include/shuati/common/config.h`
- Create: `src/common/config.cpp`
- Create: `src/app/main.cpp`

- [x] Add `shuati_core`, `shuati_server`, and `shuati_tests` targets.
- [x] Implement enough config parsing to satisfy the tests.
- [x] Keep parser scope explicit: simple YAML-like `section.key` lookup, not full YAML.

### Task 4: Third-Party And Dependency Docs

**Files:**
- Create: `third_party/cpp-httplib/httplib.h`
- Create: `third_party/cpp-httplib/README.md`
- Create: `docs/development/dependencies.md`
- Create: `scripts/install_ubuntu_deps.sh`

- [x] Download `httplib.h` from official `yhirose/cpp-httplib` release `v0.48.0`.
- [x] Record source URL, release URL, version, and update process.
- [x] Document Ubuntu dependency installation and current environment gaps.

### Task 5: Verify And Commit

- [x] Run `git diff --check`.
- [ ] Run CMake configure/build/tests after `build-essential` is installed.
- [x] If dependencies are missing, record the blocker and the exact install command.
- [x] Update `SPEC.md` P0 checklist and technical log.
- [x] Commit and push to GitHub.
