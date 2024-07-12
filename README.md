# cmc

## TODO
- Optimize `create_chunk_and_light_data_packet`
- Optimize `chunk_section_update_mesh`

![cmc-1](https://github.com/anbcodes/cmc/assets/31807975/58914050-79d8-4e23-8efa-4880a7b58348)

## Project setup

### Debian Linux

If you are on debian linux run `./dev-setup.sh` and `./import-data.sh`.

To build run
- `mkdir build && cd build`
- `cmake ..`
- `cmake --build . -j8`

To run
- Create a Minecraft profile and set a custom Java executable to the path of `custommc.sh`
- Run that Minecraft profile which creates a file called `./run-with-token.sh`
- Edit that file to change the server hostname and port.
- Run `./run-with-token.sh`

### Generic instructions

- `mkdir lib`
- Download the prebuilt WGPU libraries from
  https://github.com/gfx-rs/wgpu-native/releases and put the extracted zip in
  the `lib/wgpu` directory
- Download the GLFW sources into `lib/glfw` from the version pointed to from
  https://github.com/gfx-rs/wgpu-native/tree/trunk/examples/vendor
- From the main directory, run `git clone https://github.com/recp/cglm.git -o ./lib/cglm` to
  get cglm. It works as a header-only library.
- Retrieve `blocks.json` from the server jar with the following and place it in
  `/data`.

```
java -DbundlerMainClass=net.minecraft.data.Main -jar paper-1.20.6-145.jar --reports
```

- To load default client resources, copy your client jar from
  `~/.minecraft/versions` to `/data` and extract it with `gunzip 1.20.6.jar`. To
  clean things up you can `rm *.class` as these files are not needed.
- To compile and run follow the instructions above

## Features

- Offline and online mode support
- Encryption support
- Basic rendering
- Building and breaking blocks
- Player movement

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
