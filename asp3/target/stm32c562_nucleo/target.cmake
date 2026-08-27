#
#  ターゲット依存部のCMake定義（NUCLEO-C562RE / STM32C562 + STM32CubeC5）
#
#  外部（SDK）ターゲットのパス解決規約（asp3_core PORTING_GUIDE「外部ターゲット」）：
#   - 共通arch（arch/arm_m_gcc/common）は asp3_core サブモジュール側＝ASP3_ROOT_DIR
#   - チップ依存部（stm32c5xx_stm32cube）・ターゲット依存部は本リポジトリ側
#     ＝CMAKE_CURRENT_LIST_DIR 相対
#
set(ARCHDIR ${ASP3_ROOT_DIR}/arch/arm_m_gcc)
get_filename_component(CHIPDIR ${CMAKE_CURRENT_LIST_DIR}/../../arch/arm_m_gcc/stm32c5xx_stm32cube ABSOLUTE)
set(TARGETDIR ${CMAKE_CURRENT_LIST_DIR})

#
#  コンフィギュレーション関連（Ruby .trb → Python .py へ変換済み）
#
list(APPEND ASP3_CFG_FILES
    ${TARGETDIR}/target_kernel.cfg
)

list(APPEND ASP3_KERNEL_CFG_TRB_FILES
    ${TARGETDIR}/target_kernel.py
)

list(APPEND ASP3_CHECK_TRB_FILES
    ${TARGETDIR}/target_check.py
)

#
#  インクルードディレクトリ
#
#  STM32CubeMX2（CMSIS-Toolbox ベース）が生成するプロジェクトの構成に合わせる．
#  classic CubeMX の Core/ + Drivers/ ではなく，CMSIS パックを展開した
#  stm32c5xx_dfp（CMSIS デバイスヘッダ）・stm32c5xx_drivers（HAL2 と LL）・
#  arch/cmsis（CMSIS Core）・generated/hal（mx_*.c の生成初期化）に分かれる．
#
#  C5 ターゲットは HAL2 の API には依存せず，LL とレジスタ直接操作＋CMSIS
#  デバイスヘッダ（stm32c5xx.h）だけを使う．
#
list(APPEND ASP3_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/generated/hal
    ${CMAKE_SOURCE_DIR}/stm32c5xx_dfp/Include
    ${CMAKE_SOURCE_DIR}/stm32c5xx_drivers/hal
    ${CMAKE_SOURCE_DIR}/stm32c5xx_drivers/ll
    ${CMAKE_SOURCE_DIR}/stm32c5xx_drivers/utils
    ${CMAKE_SOURCE_DIR}/arch/cmsis/CMSIS/Core/Include
    ${TARGETDIR}
)

list(APPEND ASP3_COMPILE_DEFS
    USE_FULL_LL_DRIVER
    USE_HAL_DRIVER
    STM32C562xx
    $<$<CONFIG:Debug>:DEBUG>
    USE_TIM_AS_HRT
    TOPPERS_FPU_ENABLE
    TOPPERS_FPU_LAZYSTACKING
    TOPPERS_FPU_CONTEXT
)

#
#  Cortex-M33 + FPU（fpv5-sp-d16 / hard）— STM32C5 は単精度FPU付き Cortex-M33．
#  asp3 ライブラリと cfg1_out（オフセット抽出用ELF）に適用される．
#
list(APPEND ASP3_COMPILE_OPTIONS
    -mcpu=cortex-m33
    -mthumb
    -mfpu=fpv5-sp-d16
    -mfloat-abi=hard
    -ffunction-sections
    -fdata-sections
)

#
#  cfg1_out（使い捨てELF）の最小リンク：arch（Cortex-M33/fpv5-sp-d16）＋ crt0回避．
#  CubeMX 実リンカスクリプトは不要（nm でのシンボル値抽出のみ）．
#
list(APPEND ASP3_LINK_OPTIONS
    -mcpu=cortex-m33
    -mthumb
    -mfpu=fpv5-sp-d16
    -mfloat-abi=hard
    -nostartfiles
    -nostdlib
    #  CubeMX ツールチェーン（cmake/gcc-arm-none-eabi.cmake）は
    #  CMAKE_EXE_LINKER_FLAGS に -Wl,--gc-sections をグローバル付与する．
    #  cfg1_out（オフセット抽出用ELF）では TOPPERS_magic_number 等の未参照
    #  シンボルがGCで消えると cfg パス2が失敗するため，後勝ちで無効化する
    #  （asp3_core 側の gc-sections 除去はツールチェーンのグローバル付与には
    #  効かないため，ここで打ち消す）．最終 exe は CubeMX 側でGC有効のまま．
    -Wl,--no-gc-sections
)
list(APPEND ASP3_LINK_LIBS c gcc)

#
#  ターゲット依存部のソース（いずれも非TECS版）
#
list(APPEND ASP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
    ${TARGETDIR}/target_timer.c
    ${TARGETDIR}/target_serial.c
)

#
#  アーキ依存部（チップ層）のインクルード
#
include(${CHIPDIR}/arch.cmake)
