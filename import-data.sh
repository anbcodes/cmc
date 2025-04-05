#!/bin/bash

set -e
set -o pipefail

version="1.21.5"

declare -A version_hashes
version_hashes[1.21.5]="e6ec2f64e6080b9b5d9b471b291c33cc7f509733"
version_hashes[1.21.4]="4707d00eb834b446575d89a61a11b5d548d8c001"
version_hashes[1.21]="450698d1863ab5180c25d7c804ef0fe6369dd1ba"

pushd $(dirname $0) > /dev/null

rm -rf ./data

mkdir -p ./data

if [ ! -f "$HOME/.minecraft/versions/$version/$version.jar" ]; then
  echo "You must launch minecraft $version at least once!"
  exit 1
fi

echo "Extracting client jar..."

pushd data

unzip "$HOME/.minecraft/versions/$version/$version.jar" "data/*" "assets/*"

popd

if [ ! -f ".cache/server$version.jar" ]; then
  echo "Downloading $version server jar..."

  mkdir -p .cache

  wget https://piston-data.mojang.com/v1/objects/${version_hashes[$version]}/server.jar -O .cache/server$version.jar
fi

pushd .cache

echo "Extracting reports from server jar"

java -DbundlerMainClass=net.minecraft.data.Main -jar server$version.jar --reports

cp ./generated/reports/blocks.json ../data

popd

echo "Extracting packet ids..."
node extractPacketIDs.js

popd
