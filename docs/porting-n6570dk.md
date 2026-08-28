# STM32N6570-DK（STM32N657X0）への ASP3 移植

**状態：実機動作確認済み（sample1 動作・test_porting 6/6）** — 2026-08-28

STM32N6 は既存3ボードと**ブート方式が根本的に違う**（内蔵フラッシュが無い）。
`.claude/skills/porting-asp3-to-stm32/checklists/new-board.md` の「書込み → リセット」の
流れがそのままでは通用しない。

---

## 1. 実機・デバイスの確認済み情報

| 項目 | 値 |
|---|---|
| Board Name | `STM32N6570-DK` |
| ST-LINK | SN `003F00213234510E37333934`、FW `V3J15M6` |
| Device ID / Rev | `0x486` / Rev B（`STM32N6xx`） |
| コア | **Cortex-M55 r1p1**、FPv5 倍精度＋MVE、**800MHz** |
| VCP | **USART1（PE5=TX / PE6=RX、AF7）**、115200bps、Windows では **COM30** |
| HSE / SYSCLK | 24MHz / 800MHz（PLL1 のソースは HSE ではなく **HSI 64MHz**） |
| HCLK / PCLK1 | 200MHz |
| **TIM カーネルクロック** | **400MHz**（`RCC_PERIPHCLK_TIM` / `TIMPRES_DIV1` ＝ 2×PCLK1） |
| HRT / 割込み通知 | TIM2 / TIM5（**どちらも 32bit**）、1MHz 刻み＝**PSC 399** |
| `TMAX_INTNO` | **194 + 16 = 210**（最大 IRQn は `LTDC_UP_ERR_IRQn = 194`） |
| **TrustZone** | **搭載・Secure 実行**（`TOPPERS_ENABLE_TRUSTZONE` を**定義する**） |

---

## 2. ⚠️ 最大の落とし穴：`STM32_Programmer_CLI -g` では CPU が起動しない

N6 には内蔵フラッシュが無く、開発中は **dev boot モード**（BOOT1 スイッチ 1-3。BOOT0 は不問）で
**AXISRAM2 にイメージをロードして実行**する。署名も外部フラッシュも不要。

ところが：

```bash
STM32_Programmer_CLI -c port=SWD mode=UR -w app.elf -v -g 0x3418D469
#   → "Download verified successfully" / "Start operation achieved successfully"
#   → だが CPU は動かない
```

**`-g` は成功と報告するが、実際には CPU を ROM の待機ループから引き剥がせない。**
OpenOCD で halt して PC を読むと `0x18003514`（**ブート ROM 内**）のままだった。

### 切り分けの決め手

4 命令だけの最小スタブ（RAM にマーカを書いて無限ループ）を `-w` + `-g` でロードしても
マーカが書かれなかった。**ASP3 のコードとは無関係に `-g` が効いていない**と確定できた。
一方 OpenOCD で `reg pc` を設定して `step` すると Reset_Handler が正しく進む
（`0x3418d46a → 0x3418d46c → …`）ので、CPU もイメージも健全だった。

> 教訓：「動かない」ときは、まず**自分のコードを外した最小スタブ**で
> 起動機構そのものを試す。ASP3 側を疑い続けると時間を溶かす。

### 正しい起動手順（`scripts/run_n6.sh` に実装済み）

ロードは Programmer、**起動は OpenOCD** で行う：

```bash
scripts/run_n6.sh stm32n6570_dk/sample1/FSBL/build/Debug/N6570DK_FSBL.elf
```

中身は要するに以下：

```bash
# 1. ロード（実行はしない。-g は付けない）
STM32_Programmer_CLI -c port=SWD mode=UR -w app.elf -v

# 2. OpenOCD で halt → VTOR/MSP/xPSR/PC を設定 → resume
openocd -f scripts/openocd-n6-windows.cfg \
  -c "init; halt; \
      mww 0xE000ED08 0x34180400; \
      reg msp 0x34200000; \
      reg xPSR 0x01000000; \
      reg pc <entry>; \
      resume; exit"
```

`MSP` はベクタテーブル先頭のワード、`<entry>` は `readelf -h` の Entry point（thumb ビット込み）。

### ⚠️ OpenOCD は `-ap-num 1`

`STM32_Programmer_CLI -l` は `Access Port Number : 3` と表示するが、**Cortex-M55 が見えるのは AP1**。
AP0/2/3/4 では `Cortex-M PARTNO 0x0 is unrecognized` で examine に失敗する。
設定は `scripts/openocd-n6-windows.cfg`。OpenOCD 0.12.0 で M55 r1p1 を認識できる。

その他の注意：
- gdb は `target extended-remote 127.0.0.1:3333` と**IPv4 を明示**する（`:3333` だと
  IPv6 側を掴んで `Remote communication error / 10061` になる）。
- OpenOCD はプローブを排他するので、起動中は Programmer を使わない。
- **RAM 上のデバッグ用スタブを ASP3 の RAM 領域（0x341C0000〜0x34200000）に置かない。**
  .bss/スタックを汚して原因の切り分けを難しくする（実際に一度やらかした）。

### 参考：将来フラッシュ起動に移す場合（今回は不要）

```bash
STM32_SigningTool_CLI.exe -bin FSBL.bin -nk -of 0x80000000 -t fsbl -o FSBL-trusted.bin -hv 2.3
STM32_Programmer_CLI.exe -c port=SWD -el ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr \
                        -w FSBL-trusted.bin 0x70000000
# BOOT0=1-2, BOOT1=1-2 に切替えてリセット
```

署名ツール・外部ローダとも CubeIDE 2.2.0 同梱の CubeProgrammer 2.23.0 の `tools/bin/` にある。

---

## 3. メモリ配置（AXISRAM2）

ST の `Templates/Template` の `.ld` をそのまま使用（`.ld` 改変は不要）。

| 領域 | アドレス | サイズ |
|---|---|---|
| ROM（`.isr_vector`/`.text`/`.rodata`） | `0x34180400` | 255 KB |
| RAM（`.data`/`.bss`/stack/heap） | `0x341C0000` | 256 KB |

どちらも AXISRAM2（1MB）の上半分。先頭 `0x400` はブートヘッダ用。
`_kernel_vector_table` は 211 エントリ×4=844B で **1024 バイト境界**が必要だが、余裕で満たせる。

---

## 4. 実装内容

### チップ層 `asp3/arch/arm_m_gcc/stm32n6xx_stm32cube/`

- 識別マクロ `TOPPERS_STM32N6XX_STM32CUBE`、`TOPPERS_CORTEX_M55`、`__TARGET_ARCH_THUMB=5`
- **`TOPPERS_ENABLE_TRUSTZONE` を定義する**（H5/C5 とは逆）

### ⚠️ TrustZone は H5/C5 と逆＝「定義する」

`arm_m.h:63` は `__TARGET_ARCH_THUMB >= 5 && !defined(TOPPERS_ENABLE_TRUSTZONE)` のとき
`EXC_RETURN = 0xFFFFFFBC` を選ぶ。N6 は **Secure 実行**なのでハードウェアが生成する
EXC_RETURN は S/ES がセットされた `0xFFFFFFFD` であり、定義しないと不整合になる。

根拠：`stm32n657xx.h:271` `__SAUREGION_PRESENT 1U`、ST テンプレートの
`Mcu.ContextProject=FullSecure`、`.cproject` の `option.mcmse value="true"`。
同じ M55・Secure の `asp3_core/target/mps3_an547_gcc/target.cmake:49` も定義している。

**検証**：`objdump -d *.elf | grep -A1 exc_return_const` → **`0xfffffffd`**（実測確認済み）。
H5/C5 は `0xffffffbc`。

### ⚠️ `-mcmse` が必須

`stm32n657xx.h:275-277` は `__ARM_FEATURE_CMSE == 3U` のときだけ `CPU_IN_SECURE_STATE` を
定義し、それによって **`TIM2` / `TIM5` / `USART1` などの別名が Secure ビュー（`_S`）か
NonSecure ビュー（`_NS`）かに振り分けられる**。CubeMX 側は FullSecure（`-mcmse`）で
ビルドされるので、asp3 側にも `-mcmse` が無いと**同じ `TIM2` が別アドレスを指し**、
CubeMX が初期化した周辺と ASP3 のドライバが食い違う（症状は「無言で動かない」）。

Cortex-M55 のフラグ一式：

```
-mcpu=cortex-m55 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard -mcmse
```

### ターゲット層 `asp3/target/stm32n6570_dk/`

- `target_kernel.h`: `TMAX_INTNO = 194 + 16`
- `target_timer.{h,c}`: TIM2＋TIM5。**OPM の API は N6 では
  `LL_TIM_SetOnePulseMode(TIM5, LL_TIM_ONEPULSEMODE_SINGLE)`**（C5 の
  `LL_TIM_EnableOnePulseMode` は N6 に存在しない。ファミリごとに違うので毎回 grep すること）
- `target_serial.{h,c}`: USART1 のレジスタ直接操作（C5 版の方針を踏襲）
- `stm32cubemx.h`: `CPU_CLOCK_HZ = 800000000UL`。`SIL_DLY_TIM1/2` は **H5 の値の流用（未較正）**
- `target.cmake`: インクルードパスは **N6 の CubeMX レイアウト**に合わせる（下記）

### asp3_core は無変更

arch 共通部は `TOPPERS_CORTEX_M33` ではなく `__TARGET_ARCH_THUMB` で分岐しており、
さらに `core_support.S` には `#ifdef __ARM_FEATURE_MVE` の VPR 退避/復帰が既にある。
M55・Secure の既存ターゲット `mps3_an547_gcc` も同梱されている。**submodule の変更は不要**。

---

## 5. ボードプロジェクト `stm32n6570_dk/sample1/`

classic STM32CubeMX で生成（C5 の CubeMX2 とは別系統）。**ヘッドレスで生成できる**：

```bash
/c/sw/ST/STM32CubeMX/STM32CubeMX.exe -q script.txt
#   script.txt: config load <abs>/N6570DK.ioc / project generate / exit
```

`reference/cubemx-generation.md` の「ヘッドレス不可」は Linux の `HeadlessException` が
理由だった。**Windows では通る**。周辺の有効化はスクリプトからできないので、
`.ioc`（プレーンテキスト）を直接編集して TIM2/TIM5/NVIC/USART1 を書き、`config load` させる。

- **TrustZone ダイアログは出ない**（`.ioc` の `Mcu.ContextProject` 既定が `FullSecure`）。
- **単一プロジェクトにする鍵**：`ProjectManager.ProjectStructure` で `FSBL:true` /
  `Appli:false` / `ExtMemLoader:false`。これで FSBL だけが生成される。

### ディレクトリ構成（H5 系と違う）

```
stm32n6570_dk/sample1/
├── N6570DK.ioc                ← 正本（コミット）
├── Drivers/                   ← HAL + CMSIS（FSBL の 1 つ上）
└── FSBL/                      ← ★ここが実プロジェクト（ビルドはここで）
    ├── CMakeLists.txt         ← ASP3 統合ブロックを追記（コミット）
    ├── Inc/ Src/ Startup/     ← Core/ は無い
    └── STM32N657XX_AXISRAM2_fsbl.ld
```

`CMAKE_SOURCE_DIR` は `<proj>/FSBL` なので、`target.cmake` のインクルードパスは
`${CMAKE_SOURCE_DIR}/Inc` と `${CMAKE_SOURCE_DIR}/../Drivers/...` になる。

ビルド：

```bash
cd stm32n6570_dk/sample1/FSBL
cmake --preset Debug && cmake --build build/Debug     # → N6570DK_FSBL.elf
```

---

## 6. 検証結果

書込み前のバイナリ検査：

- `_kernel_vector_table` = `0x34192c00` → **1024 バイト境界**（OK）
- `exc_return_const` = **`0xFFFFFFFD`**（Secure 実行として正しい）

実機（ROM 29.5% / RAM 6.0%）：

- バナー `TOPPERS/ASP3 Kernel Release 3.7.2 for STM32N6570-DK(STM32N657X0)` →
  `Sample program starts (exinf = 0).` → `task1 is running (NNN)` の周期出力
- `r` 送信で `#rot_rdq(three priorities)` → task1 → task2 → task3 と切替
- **test_porting：`1..6` / `ok 1`〜`ok 6` / `# 6/6 passed`**

### 未実施

- **testexec（機能テスト全 36 本）**。`scripts/testexec_stm32.py` は
  「Programmer で書いてリセット」前提なので、N6 の起動方式（`scripts/run_n6.sh`）に
  対応させる必要がある。
- **`SIL_DLY_TIM1/2` の較正**（実機 dlynse テスト）。M55 800MHz＋I/D キャッシュでは
  H5 の値は合わないはず。
- **キャッシュ（I/D）の扱い**。ASP3 側では何もしていない。DMA を使わない範囲では
  問題ないと考えているが未検証。
