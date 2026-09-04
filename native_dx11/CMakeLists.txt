cmake_minimum_required(VERSION 3.20)
project(VencordArabicLiquidDX11 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This frontend is Windows-only")
endif()
if(NOT CMAKE_SIZEOF_VOID_P STREQUAL "8")
    message(FATAL_ERROR "This frontend is x64-only")
endif()

if(NOT DEFINED LIQUIDDX11_DIR)
    set(LIQUIDDX11_DIR "${CMAKE_CURRENT_SOURCE_DIR}/liquidDX11")
endif()

set(APP_DIR "${LIQUIDDX11_DIR}/examples/example_win32_directx11")

add_executable(VencordArabicLiquidDX11 WIN32
    main.cpp
    "${LIQUIDDX11_DIR}/imgui.cpp"
    "${LIQUIDDX11_DIR}/imgui_draw.cpp"
    "${LIQUIDDX11_DIR}/imgui_tables.cpp"
    "${LIQUIDDX11_DIR}/imgui_widgets.cpp"
    "${LIQUIDDX11_DIR}/misc/freetype/imgui_freetype.cpp"
    "${LIQUIDDX11_DIR}/backends/imgui_impl_dx11.cpp"
    "${LIQUIDDX11_DIR}/backends/imgui_impl_win32.cpp"
    "${APP_DIR}/src/glass/backdrop.cpp"
    "${APP_DIR}/src/glass/glass.cpp"
)

target_compile_features(VencordArabicLiquidDX11 PRIVATE cxx_std_17)
target_include_directories(VencordArabicLiquidDX11 PRIVATE
    "${LIQUIDDX11_DIR}"
    "${LIQUIDDX11_DIR}/backends"
    "${LIQUIDDX11_DIR}/thirdparty/freetype/include"
    "${APP_DIR}/src"
)
target_compile_definitions(VencordArabicLiquidDX11 PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(VencordArabicLiquidDX11 PRIVATE /W3 /utf-8)
endif()

target_link_libraries(VencordArabicLiquidDX11 PRIVATE
    d3d11
    d3dcompiler
    dxgi
    dwmapi
    winmm
    user32
    gdi32
    shell32
    ole32
    advapi32
    comdlg32
    "${LIQUIDDX11_DIR}/thirdparty/freetype/win64/freetype.lib"
)
