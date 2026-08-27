# file-format: 1.0.0
cmake_minimum_required(VERSION 3.30)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

include(cmake/target.cmake)
include(cmake/${TOOLCHAIN_FILE})

project(C562RE)
add_executable(C562RE)
include(cmake/files.cmake)
include(cmake/flags.cmake)
include(cmake/components.cmake)

# Stubs to be able to add compiler flag overrides, e.g. -Os
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  # Add additional source files here
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional include directories here
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional defined symbols here
)

target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional compiler flags here
)

target_link_options(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional linker flags here
)

target_link_directories(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional linker search directories here
)

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional linker libraries here
)

# ===== TOPPERS/ASP3 Core 取り込み（submodule＋ライブラリ専用モード） =====
# 生成側（STM32CubeMX2 / CMSIS-Toolbox）の CMakeLists に対する追記部分．
# CubeMX2 の再生成は既定 --diff keep_user のためこのブロックは保持される．
# 経緯：docs/porting-c562re.md

# ターゲット依存部（NUCLEO-C562RE / STM32C562 + STM32CubeC5）
set(ASP3_TARGET stm32c562_nucleo)

# STM32 協調ヘルパ（ASP3_TARGET_DIR / ASP3_CORE_DIR / ASP3_ROOT_DIR を解決）
include(../../asp3/asp3_stm32cube.cmake)

# アプリ（タスク定義）は asp3_core 同梱 sample（自作アプリに差し替え可）
# -D ASP3_APPLDIR/ASP3_APPLNAME 指定時はそれを尊重する（test_porting 等の差し替え用）
if(NOT DEFINED ASP3_APPLDIR)
    set(ASP3_APPLDIR  ${ASP3_CORE_DIR}/sample)
    set(ASP3_APPLNAME sample1)
endif()

# asp3 カーネルライブラリをライブラリ専用モードでビルド
set(ASP3_LIBRARY_ONLY ON CACHE BOOL "build asp3 as library only" FORCE)
add_subdirectory(${ASP3_CORE_DIR} asp3)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${ASP3_APPLDIR}/${ASP3_APPLNAME}.c
    ${ASP3_EXTRA_APP_C_FILES}
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${ASP3_APPLDIR}
)

#  生成側 cmake/components.cmake が plain 形式で target_link_libraries を
#  使っているため，ここも plain 形式で揃える（混在は CMake エラー）．
target_link_libraries(${CMAKE_PROJECT_NAME}
    asp3
)

# システムサービス（banner / syslog / serial / logtask 等）を追加
asp3_add_syssvc(${CMAKE_PROJECT_NAME})

# TOPPERS/ASP3 を使うための STM32 の設定
asp3_set_stm32_options(${CMAKE_PROJECT_NAME})
