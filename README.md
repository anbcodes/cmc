## Project setup

* I downloaded the prebuilt WGPU libraries from https://github.com/gfx-rs/wgpu-native/releases and put the extracted zip in the `wgpu` directory
* I downloaded the GLFW sources from the version pointed to from https://github.com/gfx-rs/wgpu-native/tree/trunk/examples/vendor
* Then I followed the build instructions for GLFW https://www.glfw.org/docs/3.3/compile.html
* From the GLFW build directory, I ran `cmake --install . --prefix ../../GLFW`
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
* To compile I ran this
```
gcc main.c framework.c wgpu/libwgpu_native.a GLFW/lib/libglfw3.a -lm
```
* This creates the executable `a.out`
