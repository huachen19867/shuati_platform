# P2 Auth And Database Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the database/account MVP: MySQL schema, fixed-size pool primitive, super admin bootstrap, username/password auth, session expiry, logout, RBAC, and super-admin role management.

**Architecture:** Keep persistence behind repositories so unit tests do not require a running MySQL daemon. P2 ships `schema.sql` and a fixed-size pool primitive now; the service uses an in-memory user repository for the runnable MVP until MySQL server credentials are provisioned. Session expiry is handled in the auth service and covered by regression tests.

**Tech Stack:** C++17, cpp-httplib, Google Test, OpenSSL libcrypto for MVP SHA-256 password hashing, MySQL 8 schema SQL.

---

### Task 1: Tests First

**Files:**
- Create: `tests/unit/role_test.cpp`
- Create: `tests/unit/session_manager_test.cpp`
- Create: `tests/unit/auth_service_test.cpp`
- Create: `tests/unit/connection_pool_test.cpp`
- Modify: `CMakeLists.txt`

- [x] Add role parse/serialize and permission tests.
- [x] Add session TTL regression tests, including expired-token rejection.
- [x] Add registration, login, logout, duplicate-user, bad-password, and super-admin role-management tests.
- [x] Add fixed-size pool tests covering acquire/release and timeout.
- [x] Run tests before implementation and record the red state.

### Task 2: Account Domain

**Files:**
- Create: `include/shuati/auth/role.h`
- Create: `src/auth/role.cpp`
- Create: `include/shuati/auth/password_hasher.h`
- Create: `src/auth/password_hasher.cpp`
- Create: `include/shuati/auth/user_repository.h`
- Create: `src/auth/user_repository.cpp`
- Create: `include/shuati/auth/session_manager.h`
- Create: `src/auth/session_manager.cpp`
- Create: `include/shuati/auth/auth_service.h`
- Create: `src/auth/auth_service.cpp`

- [x] Implement `UserRole` as `user`, `admin`, `super_admin`.
- [x] Implement salted SHA-256 password hashing as the P2 MVP; keep bcrypt scheduled for P6.
- [x] Implement in-memory user repository with unique usernames and role updates.
- [x] Implement session manager that stores token hashes, supports configurable TTL, and invalidates expired sessions.
- [x] Implement auth service for register, login, logout, `currentUser`, bootstrap super admin, and super-admin-only role updates.

### Task 3: Database Structure And Pool Primitive

**Files:**
- Create: `sql/schema.sql`
- Create: `include/shuati/db/connection_pool.h`

- [x] Add MySQL tables for users, sessions, and submissions placeholder-friendly account metadata.
- [x] Add a tested fixed-size generic pool primitive for later MySQL connection handles.
- [x] Document that MySQL server/client library is not available in this WSL yet, so repository wiring remains behind the interface.

### Task 4: HTTP Routes And Config

**Files:**
- Modify: `include/shuati/app/app_config.h`
- Modify: `src/app/app_config.cpp`
- Modify: `include/shuati/app/server.h`
- Modify: `src/app/server.cpp`
- Modify: `src/app/main.cpp`
- Modify: `config/app.yaml`

- [x] Load database, security, and bootstrap super-admin config.
- [x] Wire an auth service into the HTTP server.
- [x] Add routes: register, login, logout, me, list users, and update user role.
- [x] Use unified JSON responses and `X-Session-Token` headers for the MVP.
- [x] Return 401 for missing/expired sessions and 403 for insufficient roles.

### Task 5: Docs, Verification, Git

**Files:**
- Modify: `SPEC.md`
- Modify: `docs/tech-logs/2026-07-02.md`
- Modify: `docs/superpowers/plans/2026-07-02-p2-auth-database.md`

- [x] Update P2 checklist.
- [x] Record commands, MySQL boundary, session-expiry fix, and test results in the technical log.
- [x] Run `cmake`, `cmake --build`, `ctest`, and service smoke tests.
- [x] Commit and push to GitHub.
