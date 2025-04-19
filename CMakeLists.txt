cmake_minimum_required(VERSION 3.22)

project(cmc)

add_subdirectory(lib/libdeflate)
add_subdirectory(lib/glfw)
add_subdirectory(lib/cglm)
add_subdirectory(lib/yyjson)

set (CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(cmc src/main.c
  src/framework.c
  src/chunk.c
#  src/mcapi.c
  src/nbt.c
  src/datatypes.c
  src/cJSON.c
  src/lodepng/lodepng.c
  src/mcapi/base.c
  src/mcapi/chunk.c
  src/mcapi/config.c
  src/mcapi/internal.c
  src/mcapi/login.c
  src/mcapi/entity.c
  src/mcapi/misc.c
  src/mcapi/player.c
  src/mcapi/protocol.c
)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)


target_link_libraries(cmc PUBLIC "${CMAKE_SOURCE_DIR}/lib/wgpu/libwgpu_native.a")
target_link_libraries(cmc PUBLIC crypto)
target_link_libraries(cmc PUBLIC curl)
target_link_libraries(cmc PUBLIC libdeflate_static)
target_link_libraries(cmc PUBLIC glfw)
target_link_libraries(cmc PUBLIC cglm)
target_link_libraries(cmc PRIVATE yyjson)


target_include_directories(cmc PUBLIC "${CMAKE_SOURCE_DIR}/lib/libdeflate")
target_include_directories(cmc PUBLIC "${CMAKE_SOURCE_DIR}/lib/glfw/include")
target_include_directories(cmc PUBLIC "${CMAKE_SOURCE_DIR}/lib/cglm/include")
target_include_directories(cmc PUBLIC "${CMAKE_SOURCE_DIR}/lib/wgpu")
