#!/usr/bin/env bash
set -euo pipefail

toolchain_dir="${1:-$HOME/.local/toolchains/gcc-13-ubuntu24.04}"
bin_dir="$HOME/.local/bin"
tmp_dir="$(mktemp -d)"

cleanup() {
  rm -rf "$tmp_dir"
}
trap cleanup EXIT

mkdir -p "$toolchain_dir" "$bin_dir"

packages=(
  build-essential
  cpp
  cpp-13
  cpp-13-x86-64-linux-gnu
  cpp-x86-64-linux-gnu
  gcc
  gcc-13
  gcc-13-base
  gcc-13-x86-64-linux-gnu
  gcc-x86-64-linux-gnu
  g++
  g++-13
  g++-13-x86-64-linux-gnu
  g++-x86-64-linux-gnu
  libgcc-13-dev
  libstdc++-13-dev
)

(
  cd "$tmp_dir"
  apt download "${packages[@]}"
)

find "$tmp_dir" /var/cache/apt/archives \
  -maxdepth 1 \
  -type f \
  \( -name "build-essential*.deb" \
    -o -name "cpp*.deb" \
    -o -name "gcc*.deb" \
    -o -name "g++*.deb" \
    -o -name "libgcc-13-dev*.deb" \
    -o -name "libstdc++-13-dev*.deb" \) \
  -exec dpkg-deb -x {} "$toolchain_dir" \;

ln -sfn "$toolchain_dir/usr/bin/g++" "$bin_dir/g++"
ln -sfn "$toolchain_dir/usr/bin/g++" "$bin_dir/c++"

echo "Local C++ toolchain installed:"
"$bin_dir/g++" --version | head -n 1
echo
echo "Compiler links:"
echo "  $bin_dir/g++"
echo "  $bin_dir/c++"
echo
echo 'If a new shell cannot find g++, make sure $HOME/.local/bin is in PATH.'
