cmake_minimum_required(VERSION 3.20)
project(NumOSMathWeb LANGUAGES C CXX)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "numos_math_wasm must be configured through emcmake")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
get_filename_component(NUMOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# Exact same Giac and libtommath mathematical closure as emulator_web.
file(GLOB TOMMATH_SOURCES CONFIGURE_DEPENDS
    "${NUMOS_ROOT}/lib/libtommath/*.c")
list(FILTER TOMMATH_SOURCES EXCLUDE REGEX "/native_random_stubs\\.c$")
add_library(numos_math_tommath STATIC ${TOMMATH_SOURCES})
target_include_directories(numos_math_tommath PUBLIC
    "${NUMOS_ROOT}/lib/libtommath")
target_compile_options(numos_math_tommath PRIVATE -O1 -w)

file(GLOB GIAC_CANDIDATES CONFIGURE_DEPENDS
    "${NUMOS_ROOT}/lib/giac/src/*.cc"
    "${NUMOS_ROOT}/lib/giac/src/*.cpp"
    "${NUMOS_ROOT}/lib/giac/src/*.c")
set(GIAC_SOURCES)
foreach(source IN LISTS GIAC_CANDIDATES)
    get_filename_component(base "${source}" NAME)
    if(base MATCHES "^(kdisplay|k_|fx|ti|Xcas|Fl_|Graph|hist)" OR
       base STREQUAL "test.cpp")
        continue()
    endif()
    list(APPEND GIAC_SOURCES "${source}")
endforeach()
add_library(numos_math_giac STATIC ${GIAC_SOURCES})
target_include_directories(numos_math_giac PUBLIC
    "${NUMOS_ROOT}/lib/giac"
    "${NUMOS_ROOT}/lib/giac/src"
    "${NUMOS_ROOT}/lib/libtommath")
target_compile_definitions(numos_math_giac PRIVATE
    HAVE_CONFIG_H GIAC_KHICAS NO_GUI GIAC_GENERIC EMBEDDED UMAP DOUBLEVAL NO_BSD
    _LIBCPP_ENABLE_CXX17_REMOVED_BINDERS)
target_compile_options(numos_math_giac PRIVATE
    -O1 -w -fexceptions -fwasm-exceptions -include unistd.h -UEMCC
    -Wno-register -Wno-c++11-narrowing -ffunction-sections -fdata-sections)
target_link_libraries(numos_math_giac PUBLIC numos_math_tommath)

add_executable(numos_math_wasm
    "${NUMOS_ROOT}/src/math/giac/GiacEngine.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NumosMathSession.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/numos_math_angle_state.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/numos_math_exports.cpp")
target_include_directories(numos_math_wasm PRIVATE
    "${NUMOS_ROOT}/src"
    "${NUMOS_ROOT}/lib/giac"
    "${NUMOS_ROOT}/lib/giac/src"
    "${NUMOS_ROOT}/lib/libtommath")
target_compile_definitions(numos_math_wasm PRIVATE
    NUMOS_MATH_HEADLESS_WASM=1
    NATIVE_SIM HAVE_CONFIG_H DOUBLEVAL GIAC_KHICAS NO_GUI GIAC_GENERIC
    EMBEDDED UMAP NO_BSD _LIBCPP_ENABLE_CXX17_REMOVED_BINDERS)
target_compile_options(numos_math_wasm PRIVATE
    -fexceptions -fwasm-exceptions -include unistd.h -UEMCC
    -Wno-register -Wno-c++11-narrowing -ffunction-sections -fdata-sections)
target_link_libraries(numos_math_wasm PRIVATE
    numos_math_giac numos_math_tommath)

string(TOLOWER "${CMAKE_BUILD_TYPE}" NUMOS_CONFIG_LOWER)
set(NUMOS_OUTPUT_DIR "${NUMOS_ROOT}/out/wasm-math/${NUMOS_CONFIG_LOWER}")
file(MAKE_DIRECTORY "${NUMOS_OUTPUT_DIR}")
set_target_properties(numos_math_wasm PROPERTIES
    OUTPUT_NAME "numos-math-runtime"
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${NUMOS_OUTPUT_DIR}")

target_link_options(numos_math_wasm PRIVATE
    --no-entry
    -fwasm-exceptions
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sEXPORT_NAME=createNumosMathModule
    -sENVIRONMENT=web,worker
    -sFILESYSTEM=0
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=33554432
    -sMAXIMUM_MEMORY=536870912
    -sSTACK_SIZE=1048576
    -sEXIT_RUNTIME=0
    "-sINCOMING_MODULE_JS_API=['locateFile','print','printErr','onAbort','wasmBinary']"
    "-sEXPORTED_FUNCTIONS=['_malloc','_free','_numos_math_create','_numos_math_initialize','_numos_math_evaluate','_numos_math_simplify','_numos_math_calculus','_numos_math_solve','_numos_math_compile_1d','_numos_math_compile_2d','_numos_math_evaluate_1d','_numos_math_evaluate_many_1d','_numos_math_evaluate_2d','_numos_math_evaluate_grid_2d','_numos_math_dispose_handle','_numos_math_set_angle_mode','_numos_math_get_angle_mode','_numos_math_set_variable','_numos_math_remove_variable','_numos_math_clear_variables','_numos_math_reset','_numos_math_diagnostics','_numos_math_shutdown']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','UTF8ToString','HEAPF64']"
    -Wl,--gc-sections)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(numos_math_wasm PRIVATE -O0 -g2)
    target_link_options(numos_math_wasm PRIVATE
        -O0 -g2 -sASSERTIONS=2 -sSAFE_HEAP=1)
else()
    target_compile_options(numos_math_wasm PRIVATE -O2)
    target_link_options(numos_math_wasm PRIVATE -O2 -sASSERTIONS=1)
endif()
