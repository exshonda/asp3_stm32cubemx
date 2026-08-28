#
#  ターゲット依存部のCMake定義（STM32N6570-DK / STM32N657X0 + STM32Cube_FW_N6）
#
#  外部（SDK）ターゲットのパス解決規約（asp3_core PORTING_GUIDE「外部ターゲット」）：
#   - 共通arch（arch/arm_m_gcc/common）は asp3_core サブモジュール側＝ASP3_ROOT_DIR
#   - チップ依存部（stm32n6xx_stm32cube）・ターゲット依存部は本リポジトリ側
#     ＝CMAKE_CURRENT_LIST_DIR 相対
#
#  STM32N6 は classic STM32CubeMX 対応（C5 の CubeMX2/CMSIS-Toolbox とは違い，
#  H5 と同じ Core/ + Drivers/ + cmake/stm32cubemx/ レイアウトになる）．
#  したがってインクルードパスの構成は stm32h533_nucleo に倣う．
#
set(ARCHDIR ${ASP3_ROOT_DIR}/arch/arm_m_gcc)
get_filename_component(CHIPDIR ${CMAKE_CURRENT_LIST_DIR}/../../arch/arm_m_gcc/stm32n6xx_stm32cube ABSOLUTE)
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
#  インクルードディレクトリ（classic CubeMX 生成の Core/ + Drivers/）
#
#  BSP（Drivers/BSP/STM32N6570-DK）は意図的に含めない：C5 移植で確立した方針
#  どおり，ターゲット依存部は BSP_COM_Init/hcom_uart[] に依存せず，CMSIS
#  デバイスヘッダ（stm32n6xx.h）と LL だけを使う（docs/porting-c562re.md §4）．
#
#  N6 の CubeMX 生成レイアウトは H5 系と違う：`Core/` が無く `Inc/`・`Src/` は
#  FSBL/ の下，`Drivers/` は FSBL/ の 1 つ上（プロジェクト直下）に置かれる．
#  ビルドは stm32n6570_dk/sample1/FSBL で行うため CMAKE_SOURCE_DIR は <proj>/FSBL．
#
list(APPEND ASP3_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/Inc
    ${CMAKE_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/../Drivers/CMSIS/Device/ST/STM32N6xx/Include
    ${CMAKE_SOURCE_DIR}/../Drivers/CMSIS/Include
    ${TARGETDIR}
)

list(APPEND ASP3_COMPILE_DEFS
    USE_FULL_LL_DRIVER
    USE_HAL_DRIVER
    STM32N657xx
    $<$<CONFIG:Debug>:DEBUG>
    USE_TIM_AS_HRT
    TOPPERS_FPU_ENABLE
    TOPPERS_FPU_LAZYSTACKING
    TOPPERS_FPU_CONTEXT
)

#
#  Cortex-M55 + FPU（fpv5-d16 / hard）
#
#  一次情報：STM32Cube_FW_N6_V1.1.1/Projects/STM32N6570-DK/Templates/Template/
#  STM32CubeIDE/FSBL/.cproject の
#    ...option.fpu.*      value=...fpu.value.fpv5-d16
#    ...option.floatabi.* value=...floatabi.value.hard
#    ...c.compiler.option.mcmse.* value="true"   ← -mcmse
#  および Template.ioc の Mcu.ContextProject=FullSecure．
#
#  Cortex-M55 の FPU は FPv5・倍精度（D16）なので fpv5-sp-d16 ではなく
#  fpv5-d16．-mcpu=cortex-m55 は既定で +mve.fp を含み，gcc 13.2.1 で
#  __ARM_FEATURE_MVE=3 が定義される（実測）．これにより asp3_core の
#  core_support.S にある MVE VPR 退避／復帰コード（#ifdef __ARM_FEATURE_MVE）
#  が有効になる（asp3_core/target/mps3_an547_gcc と同じ経路）．
#
#  ★-mcmse は必須．STM32N6 の CMSIS デバイスヘッダは
#    stm32n657xx.h:275-277  #if __ARM_FEATURE_CMSE == 3U → #define CPU_IN_SECURE_STATE
#    stm32n657xx.h:4093-    #if defined(CPU_IN_SECURE_STATE) で
#                           TIM2/TIM5/USART1 等の別名を _S（Secure）ビューに，
#                           それ以外（:38067 の #else）で _NS ビューに解決する．
#  CubeMX 生成側は FullSecure＝-mcmse でビルドされるため，asp3 側で -mcmse を
#  付けないと同じ "TIM2" が別アドレス（NS 別名）を指し，初期化した周辺と
#  ドライバが食い違う．gcc 13.2.1 で -mcpu=cortex-m55 -mcmse がビルド可能
#  かつ CPU_IN_SECURE_STATE が定義されることは実測で確認済み．
#
list(APPEND ASP3_COMPILE_OPTIONS
    -mcpu=cortex-m55
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    -mcmse
    -ffunction-sections
    -fdata-sections
)

#
#  cfg1_out（使い捨てELF）の最小リンク：arch（Cortex-M55/fpv5-d16）＋ crt0回避．
#  CubeMX 実リンカスクリプトは不要（nm でのシンボル値抽出のみ）．
#
list(APPEND ASP3_LINK_OPTIONS
    -mcpu=cortex-m55
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    -nostartfiles
    -nostdlib
    #  CubeMX ツールチェーン（cmake/gcc-arm-none-eabi.cmake）は
    #  CMAKE_EXE_LINKER_FLAGS に -Wl,--gc-sections をグローバル付与する．
    #  cfg1_out（オフセット抽出用ELF）では TOPPERS_magic_number 等の未参照
    #  シンボルがGCで消えると cfg パス2が失敗するため，後勝ちで無効化する
    #  （asp3_core 側の gc-sections 除去はツールチェーンのグローバル付与には
    #  効かないため，ここで打ち消す）．最終 exe は CubeMX 側でGC有効のまま．
    #  ★この行は絶対に消さないこと．
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
