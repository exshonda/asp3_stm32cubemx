/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 * 
 *  Copyright (C) 2006-2020 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 * 
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．
 *  (1) 本ソフトウェアをソースコードの形で利用する場合には，上記の著作
 *      権表示，この利用条件および下記の無保証規定が，そのままの形でソー
 *      スコード中に含まれていること．
 *  (2) 本ソフトウェアを，ライブラリ形式など，他のソフトウェア開発に使
 *      用できる形で再配布する場合には，再配布に伴うドキュメント（利用
 *      者マニュアルなど）に，上記の著作権表示，この利用条件および下記
 *      の無保証規定を掲載すること．
 *  (3) 本ソフトウェアを，機器に組み込むなど，他のソフトウェア開発に使
 *      用できない形で再配布する場合には，次のいずれかの条件を満たすこ
 *      と．
 *    (a) 再配布に伴うドキュメント（利用者マニュアルなど）に，上記の著
 *        作権表示，この利用条件および下記の無保証規定を掲載すること．
 *    (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
 *        報告すること．
 *  (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
 *      害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
 *      また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
 *      由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを
 *      免責すること．
 * 
 *  本ソフトウェアは，無保証で提供されているものである．上記著作権者お
 *  よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的
 *  に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェ
 *  アの利用により直接的または間接的に生じたいかなる損害に関しても，そ
 *  の責任を負わない．
 * 
 *  $Id: stm32cubemx.h 289 2021-08-05 14:44:10Z ertl-komori $
 */

/*
 *  STM32 Cube MX サポートモジュール
 */

#ifndef TOPPERS_STM32CUBEMX_H
#define TOPPERS_STM32CUBEMX_H

/*
 *  コアのクロック周波数
 *
 *  ボード側 CubeMX（N6570DK.ioc）で確定したクロック構成：
 *    - HSE は 24MHz（CubeMX の board DB．FW_N6 の stm32n6xx_hal_conf.h にある
 *      48MHz は board=custom テンプレートの既定値で DK には当たらない）
 *    - PLL1 のソースは HSE ではなく HSI(64MHz)．PLLN=25 → VCO 1600MHz →
 *      IC1/IC2 で CPU 800MHz，AHB DIV2 で HCLK/PCLK1 = 200MHz
 *    - TIM のカーネルクロックは 400MHz（RCC_PERIPHCLK_TIM / TIMPRES_DIV1＝2×PCLK1）
 *      ＝ TIM2/TIM5 のプリスケーラは __LL_TIM_CALC_PSC(400000000, 1000000) → 399．
 *      SystemCoreClock（＝CPU の 800MHz）から計算すると 500kHz 刻みになり誤り．
 *
 *  CPU_CLOCK_HZ にはコアの実クロック（800MHz）を入れる．なお本ターゲットは
 *  USE_TIM_AS_HRT なので core_timer.c（CPU_CLOCK_HZ を使う唯一のカーネル側
 *  コード）はビルドされず，実質 sil_dly_nse 用の値である．
 */
#define CPU_CLOCK_HZ    800000000UL

/*
 *  微少時間待ちのための定義（本来はSILのターゲット依存部）
 */
/*
 *  sil_dly_nse の較正値
 *
 *  TODO(calib)：実機の dlynse テストで較正する（未実施．下記は H5 ターゲットの
 *  値を暫定的に流用したもの）．N6 はコアが Cortex-M55 で最大 800MHz と
 *  H5/C5 より大幅に速く，I/D キャッシュも持つため，H5 の値のままではまず
 *  合わない．値を小さくすると遅延が伸びる＝安全側．
 */
#define SIL_DLY_TIM1    64
#define SIL_DLY_TIM2    50

#endif /* TOPPERS_STM32CUBEMX_H */
