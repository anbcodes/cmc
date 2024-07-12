#!/bin/bash

set -e
set -o pipefail

pushd $(dirname $0) > /dev/null

echo "Setting up dev env for linux"

echo "Installing libraries..."
sudo apt-get install libssl-dev libcurl4-openssl-dev

mkdir -p lib

if [ ! -d lib/wgpu ]; then
  echo "Downloading wgpu debug binaries"

  wget https://github.com/gfx-rs/wgpu-native/releases/download/v0.19.4.1/wgpu-linux-x86_64-debug.zip -O wgpu.zip
  mkdir -p lib/wgpu
  pushd lib/wgpu
  unzip ../../wgpu.zip
  popd
  rm -rf wgpu.zip
fi

if [ ! -d "lib/glfw" ]; then
  echo "Downloading glfw..."

  wget https://github.com/glfw/glfw/archive/7b6aead9fb88b3623e3b3725ebb42670cbe4c579.zip -O glfw.zip
  unzip glfw.zip
  mv glfw-* ./lib/glfw
  rm -rf glfw.zip
fi

if [ ! -d lib/cglm ]; then
  echo "Downloading cglm..."

  git clone https://github.com/recp/cglm.git lib/cglm
fi

if [ ! -d lib/libdeflate ]; then
  echo "Downloading libdeflate..."

  wget 'https://github.com/ebiggers/libdeflate/archive/refs/tags/v1.20.zip' -O libdeflate.zip
  unzip libdeflate.zip
  mv libdeflate-* ./lib/libdeflate
  rm libdeflate.zip
fi

echo "Done! Next get required minecraft files by running ./import-data.sh"

popd > /dev/null
