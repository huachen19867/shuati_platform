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
