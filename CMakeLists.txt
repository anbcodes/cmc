cmake_minimum_required(VERSION 3.22)

project(cmc)

add_subdirectory(lib/libdeflate)
add_subdirectory(lib/glfw)
add_subdirectory(lib/cglm)

add_executable(cmc src/main.c
  src/framework.c
  src/chunk.c
  src/mcapi.c
  src/nbt.c
  src/datatypes.c
  src/protocol.c
  src/cJSON.c
  src/lodepng/lodepng.c
)

target_link_libraries(cmc PUBLIC "${CMAKE_SOURCE_DIR}/lib/wgpu/libwgpu_native.a")
target_link_libraries(cmc PUBLIC crypto)
target_link_libraries(cmc PUBLIC curl)
target_link_libraries(cmc PUBLIC libdeflate_static)
target_link_libraries(cmc PUBLIC glfw)
target_link_libraries(cmc PUBLIC cglm)

target_include_directories(cmc PUBLIC "${CMAKE_SOURCE_DIR}/lib/libdeflate")
target_include_directories(cmc PUBLIC "${CMAKE_SOURCE_DIR}/lib/glfw/include")
target_include_directories(cmc PUBLIC "${CMAKE_SOURCE_DIR}/lib/cglm/include")
target_include_directories(cmc PUBLIC "${CMAKE_SOURCE_DIR}/lib/wgpu")

