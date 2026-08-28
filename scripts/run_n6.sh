#!/bin/bash
#
#  STM32N6570-DK に ELF をロードして実行する（dev boot モード専用）
#
#  N6 には内蔵フラッシュが無く，dev boot モードでは ROM が待機ループを回っている．
#  STM32_Programmer_CLI の -g は "Start operation achieved successfully" と報告するが
#  実際には CPU を ROM ループから引き剥がせない（PC は 0x18003xxx のまま）．
#  そのためロードは Programmer，起動は OpenOCD（halt→VTOR/MSP/xPSR/PC 設定→resume）
#  で行う．詳細は docs/porting-n6570dk.md．
#
#  使い方: scripts/run_n6.sh <elf>
#
set -eu
ELF="$1"
PROG="/c/sw/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304/tools/bin/STM32_Programmer_CLI.exe"
OPENOCD="/c/sw/openocd_v0.12.0/bin/openocd.exe"
CFG="$(dirname "$0")/openocd-n6-windows.cfg"

#  ベクタテーブル先頭（= 初期MSP と リセットエントリ）を ELF から取る
VECT=0x34180400
MSP=$(arm-none-eabi-objdump -s --start-address=$VECT --stop-address=$((VECT+4)) "$ELF" \
        | awk '/^ 34180400/{print "0x" substr($2,7,2) substr($2,5,2) substr($2,3,2) substr($2,1,2)}')
ENTRY=$(arm-none-eabi-readelf -h "$ELF" | awk '/Entry point/{print $4}')

echo "== load  $ELF (MSP=$MSP ENTRY=$ENTRY)"
"$PROG" -c port=SWD mode=UR -w "$(cygpath -w "$ELF")" -v | grep -iE "verified|error"

echo "== start via OpenOCD (AP1)"
"$OPENOCD" -f "$CFG" -c "init; halt; mww 0xE000ED08 $VECT; reg msp $MSP; reg xPSR 0x01000000; reg pc $ENTRY; resume; exit"
