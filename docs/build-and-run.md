# ビルドと実行

対応 4 ボードのビルド・書込み・実行手順をまとめる。**ボードごとに生成ツールも
起動方法も違う**ので、まず下の一覧で自分のボードの行を確認すること。

移植の経緯や設計判断は各ボードの移植ドキュメント（末尾のリンク）を参照。

---

## 0. ボード別サマリ

| ボード | プロジェクト | preset | ELF | 構成生成 | 実行方法 |
|---|---|---|---|---|---|
| NUCLEO-H563ZI | `nucleo_h563zi/sample1` | `Debug` | `H563ZI.elf` | classic CubeMX（`.ioc`） | フラッシュ書込み＋リセット |
| NUCLEO-H533RE | `nucleo_h533re/sample1` | `Debug` | `H533.elf` | classic CubeMX（`.ioc`） | フラッシュ書込み＋リセット |
| NUCLEO-C562RE | `nucleo_c562re/sample1` | `debug_GCC_NUCLEO-C562RE` | `C562RE.elf` | **STM32CubeMX2**（`.ioc2`） | フラッシュ書込み＋リセット |
| STM32N6570-DK | `stm32n6570_dk/sample1/**FSBL**` | `Debug` | `N6570DK_FSBL.elf` | classic CubeMX（`.ioc`） | **SRAM ロード＋OpenOCD で起動** |

要注意点：

- **C562RE は preset 名が違う**（`Debug` ではない）。STM32C5 は classic CubeMX では
  扱えず STM32CubeMX2＋CMSIS-Toolbox の別系統。
- **N6570-DK はビルドディレクトリが 1 段深い**（`sample1/FSBL`）。内蔵フラッシュが
  無く、`STM32_Programmer_CLI -w ... -g` では起動しない（§4）。

---

## 1. 事前準備

### clone

```bash
git clone --recursive https://github.com/exshonda/asp3_stm32cube.git
# 既存 clone: git submodule update --init --recursive
```

### 生成物の復元（clone 直後は必須）

`Drivers/`・`cmake/`・`*.ld`・`CMakePresets.json` 等は構成生成ツールの出力で
`.gitignore` 対象。**clone 直後はビルドできない**ので、対象ボードの
`.ioc` / `.ioc2` からコード生成して復元する。手順は §2〜§4 の各ボード節、
ホスト環境の初期構築は [host-setup.md](host-setup.md) を参照。

### ツールの実体パス（この PC での例）

| ツール | パス |
|---|---|
| arm-none-eabi-gcc 13.2.1 | `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.2 Rel1\bin` |
| classic STM32CubeMX 6.17 | `C:\sw\ST\STM32CubeMX\STM32CubeMX.exe` |
| STM32CubeMX2 1.1.1 | `C:\Users\honda\AppData\Local\STMicroelectronics\STM32CubeMX2_1.1.1` |
| **STM32_Programmer_CLI 2.23.0** | `C:\sw\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304\tools\bin\` |
| OpenOCD 0.12.0 | `C:\sw\openocd_v0.12.0\bin\openocd.exe` |

> ⚠️ **standalone の STM32CubeProgrammer 2.20.0（`C:\sw\ST\STM32CubeProgrammer`）は
> STM32C5 / STM32N6 を識別できない**（`Error: Cannot identify the device`）。
> 上表の **CubeIDE 2.2.0 同梱 2.23.0** を使うこと。H5 系は 2.20.0 でも動く。

以降 `PROG` は 2.23.0 の `STM32_Programmer_CLI.exe` を指すものとする。

### 接続確認とシリアルポートの特定

```bash
"$PROG" -l          # Board Name と STLink Virtual COM Port の COM 番号が出る
```

COM 番号はホスト・USB ポートごとに変わるので、**毎回 `-l` で確認する**のが確実。
この PC での実績値は C562RE=COM27、N6570-DK=COM30。

### シリアルの読み方（Windows）

Windows では `stty` / `cat /dev/ttyACM0` が使えない。**pyserial**（3.5 で確認済み）を使う：

```bash
python -c "
import serial,sys,time
s=serial.Serial('COM27',115200,timeout=0.2)
end=time.time()+30
while time.time()<end:
    d=s.read(4096)
    if d: sys.stdout.write(d.decode('ascii','replace')); sys.stdout.flush()
"
```

送信（sample1 の `r` でタスク切替）は `s.write(b'r')`。
**リセット直後の出力を取りたい場合は、受信を先に開始してから書込み・起動する**。

---

## 2. NUCLEO-H563ZI / H533RE（classic CubeMX・フラッシュ起動）

### 生成

`nucleo_h563zi/sample1/H563ZI.ioc`（または `nucleo_h533re/sample1/H533.ioc`）を
STM32CubeMX で開いて `GENERATE CODE`。FW パッケージ未導入ならダウンロードを許可する。

> ヘッドレス生成（`STM32CubeMX.exe -q script.txt`）も **Windows なら通る**
> （§4 の N6 で実証）。従来「不可」としていたのは Linux の `HeadlessException` が理由。

### ビルド

```bash
cd nucleo_h563zi/sample1        # または nucleo_h533re/sample1
cmake --preset Debug
cmake --build build/Debug       # → build/Debug/H563ZI.elf
```

### 書込みと実行

```bash
"$PROG" -c port=SWD reset=HWrst -w "$(cygpath -w $PWD/build/Debug/H563ZI.elf)" -v -rst
```

シリアル（115200 8N1）にバナー → `Sample program starts (exinf = 0).` →
`task1 is running (NNN)`。`r` を送ると task1→2→3 が切り替わる。

---

## 3. NUCLEO-C562RE（STM32CubeMX2・フラッシュ起動）

STM32C5 は classic CubeMX の MCU DB に存在せず、**STM32CubeMX2** を使う。
SDK も CMSIS パック（HAL2）で、生成物の構成が H5 系と全く違う
（`Core/`・`Drivers/` ではなく `stm32c5xx_dfp/` `stm32c5xx_drivers/` `generated/hal/`）。

### 生成（CLI で完結する）

```bash
CUBE=/c/Users/honda/AppData/Local/STMicroelectronics/STM32CubeMX2_1.1.1/resources/cube-wrapper/0.10.3/bin
export PATH="$CUBE:$PATH"     # cube は自分自身を PATH から再起動するので必須
unset ELECTRON_RUN_AS_NODE    # ★ これが無いと CubeMX2 が無音で即終了する

# バックエンド（GUI）を起動し、listen ポートを調べる
/c/Users/honda/AppData/Local/STMicroelectronics/STM32CubeMX2_1.1.1/stm32cubemx2-1.1.1.exe &
PORT=<listen ポート>

cube mx ide-project generate --port $PORT \
  -p "$PWD/nucleo_c562re/sample1/C562RE.ioc2" \
  --format CMake --source include-packs-from-local \
  --destination "$PWD/nucleo_c562re/sample1" \
  --build-target ".debug_GCC+NUCLEO-C562RE" -f
```

`.ioc2` から新規に作り直す場合の周辺設定（USART2 / TIM2 / TIM5）は
[porting-c562re.md](porting-c562re.md) §3 に全コマンドがある。

### ビルド

```bash
cd nucleo_c562re/sample1
cmake --preset debug_GCC_NUCLEO-C562RE
cmake --build build/debug_GCC_NUCLEO-C562RE    # → C562RE.elf
```

### 書込みと実行

```bash
"$PROG" -c port=SWD reset=HWrst \
  -w "$(cygpath -w $PWD/build/debug_GCC_NUCLEO-C562RE/C562RE.elf)" -v -rst
```

---

## 4. STM32N6570-DK（SRAM ロード・OpenOCD で起動）

**内蔵フラッシュが無い。** 開発中は dev boot モードで AXISRAM2 にロードして実行する
（署名も外部フラッシュも不要）。

### 事前：BOOT スイッチ

**BOOT1 スイッチを 1-3（development mode）にする**（BOOT0 は不問）。
フラッシュ起動位置のままだと、外部フラッシュの既存 FSBL が起動して
SRAM に載せたイメージは実行されない。

### 生成（ヘッドレスで完結する）

```bash
cat > script.txt <<'EOS'
config load C:\home\TOPPERS\ASP3Core\asp3_stm32cube\stm32n6570_dk\sample1\N6570DK.ioc
project generate
exit
EOS
/c/sw/ST/STM32CubeMX/STM32CubeMX.exe -q script.txt
```

`STM32CubeMX.exe` は launch4j ラッパで**即 return する**（実体の `javaw.exe` が
非同期で走る）。生成物の出現を待ってから `taskkill //F //IM javaw.exe` する。
パック読み込みに 2〜5 分かかることがある。

### ビルド（ディレクトリが 1 段深い）

```bash
cd stm32n6570_dk/sample1/FSBL
cmake --preset Debug
cmake --build build/Debug        # → build/Debug/N6570DK_FSBL.elf
```

### 実行

```bash
# リポジトリルートで
./scripts/run_n6.sh stm32n6570_dk/sample1/FSBL/build/Debug/N6570DK_FSBL.elf
```

> ⚠️ **`STM32_Programmer_CLI -w ... -g <entry>` では起動しない。**
> "Start operation achieved successfully" と報告するが CPU は ROM の待機ループ
> （PC=`0x18003xxx`）から動かない。`scripts/run_n6.sh` は
> **ロードを Programmer、起動を OpenOCD**（halt → VTOR/MSP/xPSR/PC 設定 → resume）
> に分けている。詳細は [porting-n6570dk.md](porting-n6570dk.md) §2。

デバッグで OpenOCD を直接使う場合は `scripts/openocd-n6-windows.cfg`
（**`-ap-num 1` が必須**）。gdb は `target extended-remote 127.0.0.1:3333` と
**IPv4 を明示**する。

---

## 5. アプリの差し替え（test_porting 等）

アプリは asp3_core 標準の `-D` 機構で差し替えられる（既定は `sample/sample1`）。
`CORE` は `asp3/asp3_core` の絶対パス。

```bash
# 例：NUCLEO-H533RE
cd nucleo_h533re/sample1
CORE=$PWD/../../asp3/asp3_core
cmake --preset Debug -B build/TestPorting \
  -DASP3_APPLDIR=$CORE/test/porting -DASP3_APPLNAME=test_porting \
  -DASP3_EXTRA_APP_C_FILES=$CORE/test/porting/tap.c
cmake --build build/TestPorting
```

ボードごとに preset 名と `CORE` の相対深さが違う：

| ボード | preset | `CORE` |
|---|---|---|
| H563ZI / H533RE | `Debug` | `$PWD/../../asp3/asp3_core` |
| C562RE | `debug_GCC_NUCLEO-C562RE` | `$PWD/../../asp3/asp3_core` |
| N6570-DK | `Debug` | `$PWD/../../../asp3/asp3_core` |

シリアルに `1..6` / `ok 1`〜`ok 6` / `# 6/6 passed` が出れば合格。
**4 ボードすべてで 6/6 パスを確認済み。**

---

## 6. 機能テスト全件（testexec）

```bash
python scripts/testexec_stm32.py --board nucleo_h563zi          # 標準36本
python scripts/testexec_stm32.py --board nucleo_h563zi sem1     # 単発
python scripts/testexec_stm32.py --rejudge                      # 保存ログ再判定のみ
```

**対応は H5 系（`nucleo_h563zi` / `nucleo_h533re`）のみ。** 現状のスクリプトは
`<board>/sample1` ＋ `cmake --preset Debug` ＋「Programmer で書いてリセット」を
前提にしているため、**C562RE（preset 名が違う）と N6570-DK（ビルド位置と起動方法が違う）
には未対応**。結果の見方・既知の非 PASS は [verification.md](verification.md) を参照。

---

## 7. 動かないとき

1. まず**バイナリ検査**（書込み前にできる・原因の大半がここで分かる）
   - `arm-none-eabi-nm *.elf | grep _kernel_vector_table` →
     「エントリ数×4 以上の 2 のべき乗」境界か
   - `arm-none-eabi-objdump -d *.elf | grep -A1 exc_return_const` →
     **H5/C5 は `0xffffffbc`、N6 は `0xfffffffd`**（TrustZone の有無で逆になる）
2. **自分のコードを外した最小スタブで起動機構だけを試す**。N6 では
   「4 命令のスタブすら動かない」ことで、原因が ASP3 側でないと即断できた。
3. 切り分け手順は `.claude/skills/porting-asp3-to-stm32/checklists/bringup-debug.md`、
   落とし穴の一覧は同 `reference/vector-vtor-pitfalls.md`。

---

## 関連ドキュメント

| 内容 | 場所 |
|---|---|
| ホスト PC の初期セットアップ | [host-setup.md](host-setup.md) |
| 検証状況・テスト再実行手順 | [verification.md](verification.md) |
| NUCLEO-C562RE 移植（CubeMX2 / HAL2） | [porting-c562re.md](porting-c562re.md) |
| STM32N6570-DK 移植（M55 / Secure / SRAM 起動） | [porting-n6570dk.md](porting-n6570dk.md) |
| 残課題 | [TODO.md](TODO.md) |
| 新ボード追加・デバッグの作業ガイド | `.claude/skills/porting-asp3-to-stm32/` |
