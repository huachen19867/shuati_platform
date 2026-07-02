#!/usr/bin/env bash
set -euo pipefail

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

echo "Dependencies installed. You may need to add your user to the docker group:"
echo "  sudo usermod -aG docker $USER"
echo "Then log out and back in for Docker group changes to take effect."
