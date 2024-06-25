## Project setup

### Debian Linux

If you are on debian linux run `./dev-setup.sh` and `./import-data.sh`.

To build run `./build.sh` and then `./cmc [username] [server ip] [port]`

### Generic instructions

- Download the prebuilt WGPU libraries from
  https://github.com/gfx-rs/wgpu-native/releases and put the extracted zip in
  the `wgpu` directory
- Download the GLFW sources from the version pointed to from
  https://github.com/gfx-rs/wgpu-native/tree/trunk/examples/vendor
- Follow the build instructions for GLFW
  https://www.glfw.org/docs/3.3/compile.html
- From the GLFW build directory, run `cmake --install . --prefix ../../GLFW`
- From the main directory, run `git clone https://github.com/recp/cglm.git` to
  get cglm. It works as a header-only library.
- Retrieve `blocks.json` from the server jar with the following and place it in
  `/data`.

```
java -DbundlerMainClass=net.minecraft.data.Main -jar paper-1.20.6-145.jar --reports
```

- To load default client resources, copy your client jar from
  `~/.minecraft/versions` to `/data` and extract it with `gunzip 1.20.6.jar`. To
  clean things up you can `rm *.class` as these files are not needed.
- To compile

```
gcc main.c framework.c chunk.c mcclient/mcapi.c cJSON.c lodepng/lodepng.c wgpu/libwgpu_native.a GLFW/lib/libglfw3.a -lm -o cmc
```

- This creates the executable `cmc`

## Debugging

If there is a core dump:

```
coredumpctl dump -r `pwd`/a.out > core && gdb a.out -c core
```

## Notes on how I got started with a WebGPU example in C

- `framework.h` and `framework.c` are from
  https://github.com/gfx-rs/wgpu-native/tree/trunk/examples/framework
- `main.c` and `shader.wsgl` are from
  https://github.com/gfx-rs/wgpu-native/tree/trunk/examples/triangle
- In `main.c` I updated the includes to their local locations:

```
#include "wgpu/webgpu.h"
#include "wgpu/wgpu.h"
// ...
#include "GLFW/include/GLFW/glfw3.h"
#include "GLFW/include/GLFW/glfw3native.h"
```

- I added

```
#define GLFW_EXPOSE_NATIVE_WAYLAND
#define GLFW_EXPOSE_NATIVE_X11
```

before

```
#include "GLFW/include/GLFW/glfw3.h"
```

- I commented out this line that causes an error (probably a mismatched wgpu
  version)

```
//  .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
```

## To figure out which texture keys there are in model files:

```
jq -r '.textures | keys[]' block/*.json | sort | uniq -c | sort -nr
```
