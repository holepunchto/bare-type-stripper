cmake_minimum_required(VERSION 4.0)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)
find_package(cmake-napi REQUIRED PATHS node_modules/cmake-napi)
find_package(cmake-npm REQUIRED PATHS node_modules/cmake-npm)

project(bare_type_stripper C)

add_bare_module(bare_type_stripper)

target_sources(
  ${bare_type_stripper}
  PRIVATE
    binding.c
)

add_napi_module(bare_type_stripper_napi)

target_sources(
  ${bare_type_stripper_napi}
  PRIVATE
    binding.c
)

target_compile_definitions(
  ${bare_type_stripper_napi}
  PRIVATE
    NAPI_VERSION=9
)

resolve_node_module(bare-compat-napi compat)

target_include_directories(
  ${bare_type_stripper_napi}
  PRIVATE
    "${compat}/include"
)
