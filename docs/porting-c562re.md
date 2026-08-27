# NUCLEO-C562RE（STM32C562RE）への ASP3 移植

**状態：実機動作確認済み（sample1 動作・test_porting 6/6）** — 2026-08-28

STM32C5 は既存の H5 系ボードと**ツールチェーンの前提が根本的に違う**。
`.claude/skills/porting-asp3-to-stm32/checklists/new-board.md` は classic
STM32CubeMX を前提にしているため、そのままでは使えない。差分をここに記録する。

---

## 1. 実機・デバイスの確認済み情報

| 項目 | 値 |
|---|---|
| Board Name | `NUCLEO-C562RE`（MB2213） |
| ST-LINK | STLINK-V3EC、FW `V3J16M9` |
| Device ID / Rev | `0x44E` / Rev Y（`STM32C5x`） |
| コア | Cortex-M33 r0p4、**単精度 FPU**、144MHz（`TZ-disabled` / `NO_TZ`） |
| Flash / SRAM | 512KB / 128KB |
| **TrustZone** | **非搭載**（オプションバイトに TZEN が無い。CubeMX2 も `trustzone: off`） |
| VCP | **USART2（PA2=TX / PA3=RX、AF7）**、115200bps、Windows では **COM27** |
| HSE / SYSCLK | 24MHz / **144MHz** |
| HRT / 割込み通知タイマ | **TIM2 / TIM5（どちらも 32bit）**、1MHz 刻み（PSC=143） |
| `TMAX_INTNO` | **81 + 16 = 97**（最大 IRQn は `LPDMA2_CH3_IRQn = 81`） |

---

## 2. 既存 2 ボードとの決定的な違い

| 項目 | H5 系（H563ZI / H533RE） | **C5（C562RE）** |
|---|---|---|
| 構成生成ツール | classic STM32CubeMX（`.ioc`） | **STM32CubeMX2 1.1.1**（`.ioc2`）。classic CubeMX の MCU DB（`DB.6.0.140`）に `STM32C562` は 1 件も無い |
| SDK | STM32Cube_FW_H5（classic HAL + LL） | **CMSIS パック**（`stm32c5xx_dfp` / `stm32c5xx_hal_drivers`）。HAL は **HAL2**＝classic HAL と別 API（`hal_uart_handle_t` / `HAL_UART_SetConfig` …） |
| 生成プロジェクト | `Core/` + `Drivers/` + `cmake/stm32cubemx/` | **CMSIS-Toolbox**（`csolution`/`cproject` YAML）から CMake を書き出す。`stm32c5xx_dfp/` `stm32c5xx_drivers/` `arch/cmsis/` `generated/hal/` `misc/` `user_modifiable/` に分かれる |
| コード生成 | GUI 必須（ヘッドレス不可） | **CLI で完全自動化できる**（§3） |
| Nucleo BSP | `BSP_COM_Init` / `hcom_uart[]` に依存 | 使わない（HAL2 非依存方針） |
| 書込み CLI | CubeProgrammer 2.20.0 で可 | **2.20.0 は識別不能**。CubeIDE 2.2.0 同梱の **2.23.0** を使う |

### 移植方針

**C5 ターゲットは HAL2 の API に依存しない。** LL とレジスタ直接操作＋CMSIS
デバイスヘッダだけを使い、CubeMX2 生成コードに任せるのはクロック・ピン・
ボーレート・プリスケーラの初期化（`mx_system_init()`）までとした。
HAL2 は新しく、API が今後動く可能性があるため、移植層を巻き込まないため。

---

## 3. CubeMX2 のヘッドレス運用（重要・classic CubeMX と違う唯一の朗報）

CubeMX2 には `cube` CLI が同梱されており、**プロジェクト作成から CMake 生成まで
すべて CLI でできる**（classic CubeMX は GUI 必須だった）。

```bash
CUBE=/c/Users/honda/AppData/Local/STMicroelectronics/STM32CubeMX2_1.1.1/resources/cube-wrapper/0.10.3/bin
export PATH="$CUBE:$PATH"      # cube は自分自身を PATH から再起動するので必須
unset ELECTRON_RUN_AS_NODE     # ★ 下記の地雷。必須
```

### ⚠️ 地雷：`ELECTRON_RUN_AS_NODE=1`

Claude Code / VS Code 拡張から起動したシェルには `ELECTRON_RUN_AS_NODE=1` が
入っている。CubeMX2 は Electron アプリなので、この環境変数があると
**GUI もバックエンドも「Node として起動」して即 exit 0 する**（無音で失敗し、
ログには `Could not initialize contribution TypeError: ... reading 'onChanged'`
しか出ない）。`unset ELECTRON_RUN_AS_NODE` を忘れないこと。

### 手順

```bash
# 1. バックエンド起動（GUI）。ポートは Get-NetTCPConnection 等で確認する
#    ヘッドレス（cube lifecycle-mx start-only）は本環境では port を print して
#    終了してしまい listen しなかったため、GUI プロセスのバックエンドを使う
/c/Users/honda/AppData/Local/STMicroelectronics/STM32CubeMX2_1.1.1/stm32cubemx2-1.1.1.exe &
PORT=63677   # ← 実際の listen ポートに置き換える

# 2. ボードからプロジェクト作成（.ioc2 が正本）
cube mx project create-from-board --port $PORT --cpn NUCLEO-C562RE \
  --project-location "$PWD/nucleo_c562re/sample1" --project-name C562RE

# 3. 周辺を有効化（USART2 は board pack ではピンだけ割当済み。IP は自分で enable）
I="$PWD/nucleo_c562re/sample1/C562RE.ioc2"
cube mx peripherals enable --port $PORT -p "$I" --peripheral TIM2
cube mx peripherals enable --port $PORT -p "$I" --peripheral TIM5
cube mx peripherals enable --port $PORT -p "$I" --peripheral USART2 --mode Async

# 4. パラメータ（1MHz 刻み＝PSC 143 @144MHz、TIM5 はワンショット）
#    USART2 は既定で 115200 8N1 TX_RX なので設定不要
cube mx sw-config set-parameter-value --port $PORT -p "$I" -r TIM2 \
  --parameter-path basic/time_base/clock_source_configuration/prescaler --value 143
cube mx sw-config set-parameter-value --port $PORT -p "$I" -r TIM5 \
  --parameter-path basic/time_base/clock_source_configuration/prescaler --value 143
cube mx sw-config set-parameter-value --port $PORT -p "$I" -r TIM5 \
  --parameter-path basic/time_base/counter/one_pulse_mode --value true

# 5. CMake プロジェクトを書き出す
cube mx ide-project generate --port $PORT -p "$I" --format CMake \
  --source include-packs-from-local \
  --destination "$PWD/nucleo_c562re/sample1" \
  --build-target ".debug_GCC+NUCLEO-C562RE" -f
```

- **NVIC は設定しない**。ベクタテーブルはカーネル（`target_kernel.py` 生成）が
  持ち、割込みの許可・優先度は `CFG_INT`/`DEF_INH` が行う。
- 再生成は既定 `--diff keep_user` なので、`CMakeLists.txt` の ASP3 追記ブロックと
  `main.c` の `sta_ker()` は保持される。
- 有用な調査コマンド：`cube mx pack-manager list-boards` / `list-devices`、
  `cube mx peripherals list -p <ioc2>`、
  `cube mx peripherals description --cpn STM32C562RET6 --peripheral TIM5`
  （`cnt_width` で 16/32bit が分かる）、`cube mx nvic list-interrupts`
  （並び順から最大 IRQn＝`TMAX_INTNO` が分かる）。

> **Zephyr の dts を鵜呑みにしないこと。** 当初 Zephyr の `stm32c5.dtsi` に
> TIM5 が無かったため「C5 に TIM5 は無い」と判断して TIM7（16bit）で設計したが、
> CubeMX2 の peripherals description では **TIM5 は存在し 32bit**だった。
> 一次情報は CMSIS パック（`stm32c562xx.h`）と CubeMX2 の DB。

---

## 4. 実装内容

### チップ層 `asp3/arch/arm_m_gcc/stm32c5xx_stm32cube/`

`stm32h5xx_stm32cube` の複製＋以下。

- 識別マクロ `TOPPERS_STM32C5XX_STM32CUBE`
- `TOPPERS_CORTEX_M33` / `__TARGET_ARCH_THUMB=5` / `__TARGET_FPU_FPV4_SP`（H5 と同じ）
- **`TOPPERS_ENABLE_TRUSTZONE` は定義しない**（C5 は TrustZone 非搭載。定義すると
  EXC_RETURN が Secure 用 `0xFFFFFFFD` になり仕様上不正）

### ターゲット層 `asp3/target/stm32c562_nucleo/`

`stm32h533_nucleo` の複製＋以下。

- `target_kernel.h`: **`TMAX_INTNO = 81 + 16`**
- `target_syssvc.h`: `TARGET_NAME "NUCLEO(STM32C562RE)"`
- `target.cmake`: `CHIPDIR` を C5 チップ層へ、`STM32C562xx` 定義、
  BSP 前提の `USE_NUCLEO_64` を削除、インクルードパスを CubeMX2 レイアウトに
  （`stm32c5xx_dfp/Include`・`stm32c5xx_drivers/{hal,ll,utils}`・
  `arch/cmsis/CMSIS/Core/Include`・`generated/hal`）
- `target_timer.{h,c}`: TIM2＋TIM5（H5 と同構成）。HAL ハンドル（`htim2`/`htim5`）と
  `HAL_TIM_*` への依存を排除して LL 化。**OPM の API 名は C5 LL では
  `LL_TIM_EnableOnePulseMode(TIMx)`**（H5 の `LL_TIM_SetOnePulseMode` は無い）
- `target_serial.{h,c}`: **USART2 レジスタ直接操作に全面書換え**
  - BSP（`hcom_uart[]`）と `HAL_UART_*` 依存を排除
  - 受信は常時 RXNE 割込みでリングバッファへ、送信は送信可能コールバック許可中のみ
    TXEIE を有効化（TXE は立ちっぱなしなので必須）
  - `TXE_TXFNF`/`RXNE_RXFNE` 等のビット名の世代差を `#if defined` で吸収
  - NVIC の優先度設定・許可はしない（`CFG_INT`/`DEF_INH` でカーネルが管理。
    H5 版の `HAL_NVIC_*` 呼出しは二重管理だった）
  - **HAL2 は init 時に UE を立てない**（初回送受信時の `UART_CheckEnabledState()`
    で有効化する設計）。そのため `sio_usart_enable()` で UE/TE/RE を自分で立てる
- `stm32cubemx.h`: `SIL_DLY_TIM1/2` は **H5 の値の流用のまま（未較正）**

### ボードプロジェクト `nucleo_c562re/sample1/`

コミット対象は **4 ファイルだけ**（他は生成物＝`.gitignore`）:

| ファイル | 役割 |
|---|---|
| `C562RE.ioc2` | CubeMX2 設定の**正本** |
| `CMakeLists.txt` | 生成物＋末尾に ASP3 統合ブロックを追記 |
| `main.c` | `mx_system_init()` の後に `sta_ker()` |
| `main.h` | 生成物（現状無改変） |

**地雷**：生成側 `cmake/components.cmake` が `target_link_libraries` を
**plain 形式**で使っているため、追記側も plain で揃える必要がある
（keyword 形式 `PUBLIC` を混ぜると CMake エラー）。

---

## 5. ビルド・書込み・検証

```bash
cd nucleo_c562re/sample1
cmake --preset debug_GCC_NUCLEO-C562RE
cmake --build build/debug_GCC_NUCLEO-C562RE       # → C562RE.elf
```

書込み（**CLI は 2.23.0**。standalone 2.20.0 は C5 を識別できない）:

```bash
PROG='/c/sw/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304/tools/bin/STM32_Programmer_CLI.exe'
"$PROG" -c port=SWD reset=HWrst -w "$(cygpath -w $PWD/build/debug_GCC_NUCLEO-C562RE/C562RE.elf)" -v -rst
```

シリアルは **COM27 / 115200**。Windows では `stty`/`cat` が使えないので pyserial
（`python -c "import serial"` で 3.5 を確認済み）か `scripts/testexec_stm32.py` を使う。

### 実施済みの検証結果

書込み前のバイナリ検査:

- `_kernel_vector_table` = `0x08007000` → 98エントリ×4=392 に対し **512 バイト境界（OK）**
- `exc_return_const` = **`0xFFFFFFBC`**（TrustZone 非搭載として正しい）

実機:

- バナー `TOPPERS/ASP3 Kernel Release 3.7.2 for NUCLEO(STM32C562RE)` →
  `System logging task is started on port 1.` → `Sample program starts (exinf = 0).` →
  `task1 is running (NNN)` の 1 秒周期出力
- `r` 送信で `#rot_rdq(three priorities)` → task1 → task2 → task3 と切替
  （＝ディスパッチと受信割込みが動作）
- **test_porting：`1..6` / `ok 1`〜`ok 6` / `# 6/6 passed`**

### 未実施

- **testexec（機能テスト全 36 本）**。`scripts/testexec_stm32.py` は classic CubeMX
  レイアウト（`--board nucleo_h563zi` / `cmake --preset Debug`）前提なので、
  C5 のプリセット名（`debug_GCC_NUCLEO-C562RE`）と生成レイアウトに対応させる
  必要がある。
- **`SIL_DLY_TIM1/2` の較正**（実機 dlynse テスト）。現状 H5 の値の流用。
  testexec 対応と合わせて実施するのが自然。
