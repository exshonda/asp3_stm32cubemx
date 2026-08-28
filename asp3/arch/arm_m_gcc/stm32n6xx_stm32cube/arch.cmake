#
#  アーキ依存部のCMake定義（STM32N6xx / Cortex-M55 + STM32Cube HAL）
#
#  外部（SDK）ターゲットのパス解決規約（asp3_core PORTING_GUIDE「外部ターゲット」）：
#   - 共通arch（arch/arm_m_gcc/common）は asp3_core サブモジュール側＝ARCHDIR
#   - チップ依存部（stm32n6xx_stm32cube）は本リポジトリ側＝CHIPDIR
#  ARCHDIR/CHIPDIR/TARGETDIR は target.cmake で設定済み．
#
#  start.S（_kernel_start）は含めない：リセットは CubeMX 生成の Reset_Handler が
#  握り，main() が sta_ker() を呼ぶ（FSP/RASC と同方針）．ベクタテーブルの
#  リセットエントリも Reset_Handler を指す（target_kernel.py 参照）．SIO は
#  チップ内蔵シリアル（chip_serial.c）ではなく target_serial.c が供給．
#

list(APPEND ASP3_SYMVAL_TABLES
    ${ARCHDIR}/common/core_sym.def
)

list(APPEND ASP3_OFFSET_TRB_FILES
    ${ARCHDIR}/common/core_offset.py
)

list(APPEND ASP3_INCLUDE_DIRS
    ${CHIPDIR}
    ${ARCHDIR}/common
    ${ASP3_ROOT_DIR}/arch/gcc
)

#
#  ★TrustZone（TOPPERS_ENABLE_TRUSTZONE）は「定義する」．
#
#  H5／C5 では定義しないのが正解だったが，STM32N6 は事情が逆である．根拠：
#
#   1. STM32N6 は TrustZone-M 搭載（SAU あり）．
#      Drivers/CMSIS/Device/ST/STM32N6xx/Include/stm32n657xx.h:271
#        #define __SAUREGION_PRESENT  1U
#      （FW パッケージ STM32Cube_FW_N6_V1.1.1．以下同じ）
#
#   2. STM32N6 のユーザコード（FSBL）は **Secure 状態で動く**のが ST の既定．
#      - Projects/STM32N6570-DK/Templates/Template/Template.ioc:
#          Mcu.ContextProject=FullSecure
#      - Projects/STM32N6570-DK/Templates/Template/STM32CubeIDE/FSBL/.cproject:
#          com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.compiler.option.mcmse
#            value="true"      （＝ -mcmse でコンパイル）
#          defaults オプション文字列にも "|| Secure ||" が入る
#
#   3. Secure 状態で例外を取ると，ハードウェアが LR に載せる EXC_RETURN は
#      S=1(bit6)・ES=1(bit0) すなわち 0xFFFFFFFD になる．
#      arch 共通部は TOPPERS_ENABLE_TRUSTZONE の有無でこの値を切り替える
#      （asp3_core/arch/arm_m_gcc/common/arm_m.h:63-67）．定義しないと
#      0xFFFFFFBC（非Secure用）になり，Secure から取った例外の復帰値として
#      不正になる．H5/C5 とは向きが逆なので注意．
#      同じ Cortex-M55・Secure ブートの asp3_core/target/mps3_an547_gcc も
#      TOPPERS_ENABLE_TRUSTZONE を定義している（target.cmake:49）．
#      これにより SecureFault（EXCNO_SECURE=7）の優先度設定と許可も有効になる
#      （core_kernel_impl.c:229,239）．
#
#  つまり本チップは
#  .claude/skills/porting-asp3-to-stm32/reference/vector-vtor-pitfalls.md §2
#  の表の「Secure ブート（例: RP2350/pico2_arm）＝定義する」側に入る．
#  同 §2 が「定義しない」としているのは TZEN 無効の STM32H5 の話であって，
#  Secure 実行の N6 では結論が逆になる．H5/C5 の arch.cmake をそのまま
#  流用しないこと．
#
#  ■検証方法（実機ブリングアップ時に必ず実施）
#    arm-none-eabi-objdump -d <app>.elf | grep -A1 exc_return_const
#      → 0xfffffffd が入っていること（C5/H5 では 0xffffffbc が正解だった）．
#    ベクタテーブルの整列は
#      arm-none-eabi-nm <app>.elf | grep _kernel_vector_table
#      → アドレスが 1024 バイト境界（下位 10bit が 0）であること
#        （TMAX_INTNO=210 ⇒ 211エントリ×4=844 ⇒ 1024）．
#
#  TOPPERS_CORTEX_M55 は各ターゲットの識別用で arch 共通部は参照しない
#  （分岐は __TARGET_ARCH_THUMB で行う．>=5 が ARMv8-M）．
#  __TARGET_FPU_FPV4_SP は N6 では定義しない：N6 の FPU は FPv5・倍精度
#  （fpv5-d16）であり単精度専用ではない．なお同マクロは asp3_core の
#  コード側からは参照されていない（core_design.txt の記述のみ）．
#
list(APPEND ASP3_COMPILE_DEFS
    TOPPERS_CORTEX_M55
    TOPPERS_ENABLE_TRUSTZONE
    __TARGET_ARCH_THUMB=5
)

list(APPEND ASP3_ARCH_C_FILES
    ${ARCHDIR}/common/core_kernel_impl.c
    ${ARCHDIR}/common/core_support.S
)
