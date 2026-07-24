cmake_minimum_required(VERSION 3.20)
project(NumOSEmulatorWeb LANGUAGES C CXX)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "emulator_web must be configured through emcmake")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(NUMOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${NUMOS_ROOT}/scripts/emulator_sources.py"
            --repo "${NUMOS_ROOT}"
            --cmake "${CMAKE_BINARY_DIR}/generated/emulator_sources.cmake"
    RESULT_VARIABLE SOURCE_RESULT
)
if(NOT SOURCE_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not derive emulator_pc source closure")
endif()
include("${CMAKE_BINARY_DIR}/generated/emulator_sources.cmake")

set(LVGL_SOURCE_DIR "${NUMOS_ROOT}/.pio/libdeps/emulator_pc/lvgl")
if(NOT EXISTS "${LVGL_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "LVGL is not installed. Run: pio pkg install -e emulator_pc")
endif()
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_PATH "${NUMOS_ROOT}/src/lv_conf.h" CACHE FILEPATH "" FORCE)
add_subdirectory("${LVGL_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/lvgl" EXCLUDE_FROM_ALL)
target_compile_definitions(lvgl PUBLIC
    LV_CONF_INCLUDE_SIMPLE
    LV_USE_DRAW_SW_ASM=0
    LV_DRAW_SW_ASM_NONE=0
    LV_DRAW_SW_ASM_NEON=1
    LV_DRAW_SW_ASM_HELIUM=2
    LV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB
)

file(GLOB TOMMATH_SOURCES CONFIGURE_DEPENDS "${NUMOS_ROOT}/lib/libtommath/*.c")
# Native-only unreachable-provider link shims are not part of the real web
# libtommath closure.
list(FILTER TOMMATH_SOURCES EXCLUDE REGEX "/native_random_stubs\\.c$")
add_library(numos_tommath STATIC ${TOMMATH_SOURCES})
target_include_directories(numos_tommath PUBLIC "${NUMOS_ROOT}/lib/libtommath")
target_compile_options(numos_tommath PRIVATE -O1 -w)

file(GLOB GIAC_CANDIDATES CONFIGURE_DEPENDS
    "${NUMOS_ROOT}/lib/giac/src/*.cc"
    "${NUMOS_ROOT}/lib/giac/src/*.cpp"
    "${NUMOS_ROOT}/lib/giac/src/*.c"
)
set(GIAC_SOURCES)
foreach(source IN LISTS GIAC_CANDIDATES)
    get_filename_component(base "${source}" NAME)
    if(base MATCHES "^(kdisplay|k_|fx|ti|Xcas|Fl_|Graph|hist)" OR
       base STREQUAL "test.cpp")
        continue()
    endif()
    list(APPEND GIAC_SOURCES "${source}")
endforeach()
add_library(numos_giac STATIC ${GIAC_SOURCES})
target_include_directories(numos_giac PUBLIC
    "${NUMOS_ROOT}/lib/giac"
    "${NUMOS_ROOT}/lib/giac/src"
    "${NUMOS_ROOT}/lib/libtommath"
)
target_compile_definitions(numos_giac PRIVATE
    HAVE_CONFIG_H GIAC_KHICAS NO_GUI GIAC_GENERIC EMBEDDED UMAP DOUBLEVAL NO_BSD
    _LIBCPP_ENABLE_CXX17_REMOVED_BINDERS
)
target_compile_options(numos_giac PRIVATE
    -O1 -w -fexceptions -fwasm-exceptions -include unistd.h -UEMCC
    -Wno-register -Wno-c++11-narrowing -ffunction-sections -fdata-sections
)
target_link_libraries(numos_giac PUBLIC numos_tommath)

add_executable(emulator_web ${NUMOS_EMULATOR_SOURCES})
target_include_directories(emulator_web PRIVATE
    "${NUMOS_ROOT}/src"
    "${NUMOS_ROOT}/lib/giac"
    "${NUMOS_ROOT}/lib/giac/src"
    "${NUMOS_ROOT}/lib/libtommath"
)
target_compile_definitions(emulator_web PRIVATE
    NATIVE_SIM _USE_MATH_DEFINES LV_CONF_INCLUDE_SIMPLE
    LV_USE_DRAW_SW_ASM=0 LV_DRAW_SW_ASM_NONE=0 LV_DRAW_SW_ASM_NEON=1
    LV_DRAW_SW_ASM_HELIUM=2 LV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB
    HAVE_CONFIG_H DOUBLEVAL GIAC_KHICAS NO_GUI GIAC_GENERIC EMBEDDED UMAP NO_BSD
    _LIBCPP_ENABLE_CXX17_REMOVED_BINDERS
)
target_compile_options(emulator_web PRIVATE
    -fexceptions -fwasm-exceptions -include unistd.h -UEMCC
    -Wno-register -Wno-c++11-narrowing -ffunction-sections -fdata-sections
)
target_link_libraries(emulator_web PRIVATE lvgl numos_giac numos_tommath)

string(TOLOWER "${CMAKE_BUILD_TYPE}" NUMOS_CONFIG_LOWER)
set(NUMOS_OUTPUT_DIR "${NUMOS_ROOT}/out/wasm/${NUMOS_CONFIG_LOWER}")
set_target_properties(emulator_web PROPERTIES
    OUTPUT_NAME "numos-emulator"
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${NUMOS_OUTPUT_DIR}"
)

target_link_options(emulator_web PRIVATE
    -fwasm-exceptions
    -sUSE_SDL=2
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sEXPORT_NAME=createNumosModule
    -sENVIRONMENT=web
    -sFILESYSTEM=1
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=67108864
    -sMAXIMUM_MEMORY=536870912
    -sSTACK_SIZE=1048576
    -sEXIT_RUNTIME=0
    "-sEXPORTED_FUNCTIONS=['_main','_numos_is_ready','_numos_request_shutdown','_numos_send_logical_key','_numos_diagnostic_state']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','UTF8ToString']"
    -Wl,--gc-sections
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # -g3 currently trips a wasm-emscripten-finalize DWARF assertion in the
    # pinned 6.0.3 SDK for this large Giac link. -g2 retains symbol names and
    # browser stacks without generating the problematic full DWARF payload.
    target_compile_options(emulator_web PRIVATE -O0 -g2)
    target_link_options(emulator_web PRIVATE -O0 -g2 -sASSERTIONS=2 -sSAFE_HEAP=1)
else()
    target_compile_options(emulator_web PRIVATE -O2)
    target_link_options(emulator_web PRIVATE -O2 -sASSERTIONS=1)
endif()

add_custom_command(TARGET emulator_web POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/index.html" "${NUMOS_OUTPUT_DIR}/index.html"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/numos-shell.js" "${NUMOS_OUTPUT_DIR}/numos-shell.js"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/numos.css" "${NUMOS_OUTPUT_DIR}/numos.css"
)
set_property(TARGET emulator_web APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/index.html"
    "${CMAKE_CURRENT_LIST_DIR}/numos-shell.js"
    "${CMAKE_CURRENT_LIST_DIR}/numos.css"
)
