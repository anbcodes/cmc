#!/bin/bash

set -e 
set -o pipefail

pushd $(dirname $0) > /dev/null

echo "Setting up dev env for linux"

if [ ! -d wgpu ]; then
  echo "Downloading wgpu debug binaries"

  wget https://github.com/gfx-rs/wgpu-native/releases/download/v0.19.4.1/wgpu-linux-x86_64-debug.zip -O wgpu.zip
  mkdir -p wgpu
  pushd wgpu
  unzip ../wgpu.zip
  popd
  rm -rf wgpu.zip
fi

if [ ! -d "GLFW" ]; then
  rm -rf ./glfw

  echo "Downloading glfw..."

  wget https://github.com/glfw/glfw/archive/7b6aead9fb88b3623e3b3725ebb42670cbe4c579.zip -O glfw.zip
  unzip glfw.zip
  mv glfw-* ./GLFW-src
  rm -rf glfw.zip

  echo "Building glfw..."

  pushd GLFW-src
  sudo apt install xorg-dev libwayland-dev libxkbcommon-dev wayland-protocols extra-cmake-modules

  cmake -S . -B build

  pushd build
  make -j
  cmake --install . --prefix ../../GLFW
  popd
  popd
fi

if [ ! -d cglm ]; then
  echo "Downloading cglm..."

  git clone https://github.com/recp/cglm.git
fi

echo "Done! Next get required minecraft files by running ./import-data.sh"

popd > /dev/null