## Project setup

* Download the prebuilt WGPU libraries from https://github.com/gfx-rs/wgpu-native/releases and put the extracted zip in the `wgpu` directory
* Download the GLFW sources from the version pointed to from https://github.com/gfx-rs/wgpu-native/tree/trunk/examples/vendor
* Follow the build instructions for GLFW https://www.glfw.org/docs/3.3/compile.html
* From the GLFW build directory, run `cmake --install . --prefix ../../GLFW`
* From the main directory, run `git clone https://github.com/recp/cglm.git` to get cglm. It works as a header-only library.
* To compile
```
gcc main.c framework.c wgpu/libwgpu_native.a GLFW/lib/libglfw3.a -lm
```
* This creates the executable `a.out`

## Notes on how I got started with a WebGPU example in C

* `framework.h` and `framework.c` are from https://github.com/gfx-rs/wgpu-native/tree/trunk/examples/framework
* `main.c` and `shader.wsgl` are from https://github.com/gfx-rs/wgpu-native/tree/trunk/examples/triangle
* In `main.c` I updated the includes to their local locations:
```
#include "wgpu/webgpu.h"
#include "wgpu/wgpu.h"
// ...
#include "GLFW/include/GLFW/glfw3.h"
#include "GLFW/include/GLFW/glfw3native.h"

```
* I added
```
#define GLFW_EXPOSE_NATIVE_WAYLAND
#define GLFW_EXPOSE_NATIVE_X11
```
before
```
#include "GLFW/include/GLFW/glfw3.h"
```
* I commented out this line that causes an error (probably a mismatched wgpu version)
```
//  .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
```
