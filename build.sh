#!/bin/bash

set -e 
set -o pipefail

pushd $(dirname $0) > /dev/null

gcc -Werror main.c framework.c chunk.c mcclient/mcapi.c cJSON.c lodepng/lodepng.c wgpu/libwgpu_native.a GLFW/lib/libglfw3.a -lm -o cmc

popd