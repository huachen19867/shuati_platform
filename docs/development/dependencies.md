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
  curl
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

## bcrypt Plan

The current Ubuntu apt cache did not expose a suitable `libbcrypt-dev` package. For the authentication phase, vendor a small bcrypt implementation under `third_party/` and record its upstream URL, version or commit, and license before use. Do not store passwords with a home-grown hash.

## Current Environment Note

On 2026-07-02, this WSL environment had `cmake`, `gcc`, `libgtest-dev`, and `libssl-dev`, but did not have `g++`/`build-essential`. CMake therefore fails with:

```text
No CMAKE_CXX_COMPILER could be found.
```

Run `scripts/install_ubuntu_deps.sh` or the package command above before building.

## Build And Test

After dependencies are installed:

```bash
cmake -S . -B build -DSHUATI_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
