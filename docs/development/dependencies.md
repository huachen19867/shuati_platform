# Ubuntu Development Dependencies

This project targets Ubuntu on WSL or Linux.

## Required Packages

Install the baseline toolchain and libraries:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  pkg-config \
  libgtest-dev \
  googletest \
  default-libmysqlclient-dev \
  libssl-dev \
  docker.io \
  unzip \
  curl \
  python3-pip \
  python3-venv \
  python3-requests
```

Why each dependency exists:

- `build-essential`: provides `g++`, `make`, and core C/C++ build tools.
- `cmake`: project build system.
- `pkg-config`: helps CMake or scripts locate system libraries.
- `libgtest-dev`, `googletest`: Google Test based unit tests.
- `default-libmysqlclient-dev`: MySQL 8.0 client headers and library for later database code.
- `libssl-dev`: required by many HTTP/security integrations and may be used by future HTTPS/token helpers.
- `docker.io`: Docker sandbox runtime for judge workers.
- `unzip`: testcase zip validation and extraction.
- `curl`: API smoke tests and dependency diagnostics.
- `python3-pip`, `python3-venv`: Playwright browser smoke setup.
- `python3-requests`: reusable API smoke tests.

## Runtime Libraries And Browser Assets

- Password hashing uses the system `libcrypt` implementation through `crypt_r`
  and stores `$2b$10$...` bcrypt hashes. CMake links `crypt` explicitly.
- Existing `sha256$salt$digest` hashes are still verified so early MVP data can
  be migrated by a later password change or rehash flow.
- The code editor loads Ace.js `1.32.9` from the cdnjs URL embedded in
  `public/index.html`: `https://cdnjs.cloudflare.com/ajax/libs/ace/1.32.9/ace.js`.
  Keep this version pinned when changing the frontend.

## Playwright Smoke Dependency

The browser smoke test is intentionally kept as a plain Python script:

```bash
python3 -m pip install --user playwright
python3 -m playwright install chromium
sudo python3 -m playwright install-deps chromium
scripts/web_smoke_p5.py
```

If Playwright is not installed, the script exits with the install hint instead
of silently skipping the check. If Chromium cannot start because libraries such
as `libnspr4.so` are missing, run the `install-deps` command from an interactive
sudo terminal and rerun the smoke.

## Current Environment Note

On 2026-07-02, this WSL environment had `cmake`, `gcc`, `libgtest-dev`, and `libssl-dev`, but did not have `g++`/`build-essential`. CMake therefore fails with:

```text
No CMAKE_CXX_COMPILER could be found.
```

Run `scripts/install_ubuntu_deps.sh` or the package command above before building.

## No-Sudo C++ Toolchain Fallback

If the WSL user belongs to `sudo` but the current agent cannot provide the
interactive sudo password, bootstrap a user-local C++ toolchain instead:

```bash
scripts/bootstrap_local_gcc.sh
```

This downloads Ubuntu `g++`/GCC 13 packages with `apt download`, extracts them
under `$HOME/.local/toolchains/gcc-13-ubuntu24.04`, and links `g++` and `c++`
into `$HOME/.local/bin`. Ubuntu's default `~/.profile` already adds that
directory to `PATH` when it exists.

The fallback does not mark Debian packages as system-installed. When you have
an interactive terminal, installing `build-essential` with sudo is still the
clean system-level setup.

## Build And Test

After dependencies are installed:

```bash
cmake -S . -B build -DSHUATI_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
