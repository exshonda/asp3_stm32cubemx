cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Compiler options
set(STM32_MCU_FLAGS  "-mcpu=cortex-m55 -mfpu=fpv5-d16 -mfloat-abi=hard -mcmse " )

#Linker options
set(STM32_LINKER_SCRIPT STM32N657XX_AXISRAM2_fsbl.ld)
set(STM32_LINKER_OPTION  )

# Include toolchain file
include("../gcc-arm-none-eabi.cmake")

# Set the project name
set(CMAKE_PROJECT_NAME N6570DK_FSBL)
project(${CMAKE_PROJECT_NAME})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Include mx-generated 
include("mx-generated.cmake")


# In order to use CMake for cross-compiling
set(CMAKE_CXX_COMPILER_FORCED "true") 

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE 
    # Add user defined symbols
)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined libraries
)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()










# ===== TOPPERS/ASP3 Core 取り込み（submodule＋ライブラリ専用モード） =====
# 既存ボード（nucleo_h533re / nucleo_c562re）と同型の統合ブロック．
# STM32N6 は内蔵フラッシュを持たないため，本プロジェクトは CubeMX の
# **FSBL コンテキスト単体**（AXISRAM2 に全部載せて dev boot で実行）である．
# 経緯：asp3/asp3_core/docs/dev/stm32-integration.md

# ターゲット依存部（STM32N6570-DK / STM32N657X0H3Q + STM32Cube HAL）
set(ASP3_TARGET stm32n6570_dk)

# STM32 協調ヘルパ（ASP3_TARGET_DIR / ASP3_CORE_DIR / ASP3_ROOT_DIR を解決）
include(${CMAKE_CURRENT_LIST_DIR}/../../../asp3/asp3_stm32cube.cmake)

# アプリ（タスク定義）は asp3_core 同梱 sample（自作アプリに差し替え可）
# -D ASP3_APPLDIR/ASP3_APPLNAME 指定時はそれを尊重する（test_porting 等の差し替え用）
if(NOT DEFINED ASP3_APPLDIR)
    set(ASP3_APPLDIR  ${ASP3_CORE_DIR}/sample)
    set(ASP3_APPLNAME sample1)
endif()

# asp3 カーネルライブラリをライブラリ専用モードでビルド
set(ASP3_LIBRARY_ONLY ON CACHE BOOL "build asp3 as library only" FORCE)
add_subdirectory(${ASP3_CORE_DIR} asp3)

# 最終実行ファイルにタスク本体を追加
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${ASP3_APPLDIR}/${ASP3_APPLNAME}.c
    ${ASP3_EXTRA_APP_C_FILES}
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${ASP3_APPLDIR}
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# ★ CubeMX 生成側 mx-generated.cmake が plain 形式で target_link_libraries を
#   使っているため，追記側も plain で揃えること（keyword 形式を混ぜると CMake エラー）
target_link_libraries(${CMAKE_PROJECT_NAME}
    asp3
)

# システムサービス（banner / syslog / serial / logtask 等）を追加
asp3_add_syssvc(${CMAKE_PROJECT_NAME})

# TOPPERS/ASP3 を使うための STM32 の設定（現状なし）
asp3_set_stm32_options(${CMAKE_PROJECT_NAME})

# TOPPERS/ASP3 のチェックを行う（シンボルがGCされるとエラーになるので省略）
#asp3_cfg_check(${CMAKE_PROJECT_NAME})
