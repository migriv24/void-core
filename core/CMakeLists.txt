cmake_minimum_required(VERSION 3.20)
project(voidcore C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── The core library (pure C, C ABI) ────────────────────────────────────────
set(VC_SOURCES
  vendor/cJSON.c
  src/util/ids.c
  src/util/log.c
  src/glyph/glyph.c
  src/tags/tag.c
  src/model/spirit.c
  src/model/rune.c
  src/model/mantle.c
  src/model/binding.c
  src/dispatch/args.c
  src/dispatch/codec.c
  src/dispatch/dispatch.c
  src/dispatch/dispatch_util.c
  src/dispatch/verbs_query.c
  src/dispatch/verbs_edit.c
  src/dispatch/verbs_graph.c
  src/dispatch/verbs_lifecycle.c
  src/dispatch/verbs_script.c
  src/dispatch/undo.c
  src/dispatch/journal.c
  src/dispatch/lifecycle.c
  src/scripts/voidscript.c
  src/scripts/vs_interp.c
  src/scripts/vs_expr.c
  src/vc_manager.c
)

add_library(voidcore SHARED ${VC_SOURCES})
target_include_directories(voidcore
  PUBLIC  include
  PRIVATE src vendor
)
# VC_DLL: this is a shared-lib build => use dllexport/dllimport on _WIN32.
# VC_BUILD_DLL: we are *building* the lib (export). Consumers get dllimport.
# CJSON_HIDE_SYMBOLS: keep cJSON internal — only vc_* symbols are exported.
target_compile_definitions(voidcore
  PUBLIC  VC_DLL
  PRIVATE VC_BUILD_DLL CJSON_HIDE_SYMBOLS
)
if(NOT MSVC)
  target_compile_options(voidcore PRIVATE -Wall -Wextra)
endif()
if(NOT MSVC AND NOT APPLE)
  # keep the runtime self-contained (no libgcc dependency) — see
  # local_compilation_discovery.md. Apple clang has no libgcc to staticize.
  target_link_options(voidcore PRIVATE -static-libgcc)
endif()
if(WIN32)
  target_link_libraries(voidcore PRIVATE bcrypt) # OS CSPRNG for id minting
endif()

# ── C smoke test (links the lib) ─────────────────────────────────────────────
add_executable(vc_smoke tests/smoke.c)
target_link_libraries(vc_smoke PRIVATE voidcore)
