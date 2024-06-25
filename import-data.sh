#!/bin/bash

set -e 
set -o pipefail

pushd $(dirname $0) > /dev/null

rm -rf ./data

mkdir -p ./data

if [ ! -f "$HOME/.minecraft/versions/1.20.6/1.20.6.jar" ]; then
  echo "You must launch minecraft 1.20.6 at least once!"
  exit 1
fi

echo "Extracting client jar..."

pushd data

unzip "$HOME/.minecraft/versions/1.20.6/1.20.6.jar" "data/*" "assets/*"

popd

if [ ! -f ".cache/server.jar" ]; then
  echo "Downloading server jar..."

  mkdir -p .cache

  wget https://piston-data.mojang.com/v1/objects/145ff0858209bcfc164859ba735d4199aafa1eea/server.jar -O .cache/server.jar
fi

pushd .cache

echo "Extracting reports from server jar"

java -DbundlerMainClass=net.minecraft.data.Main -jar server.jar --reports

cp ./generated/reports/blocks.json ../data

popd

popd