/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 * 
 *  Copyright (C) 2007,2011,2015 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 * 
 *  上記著作権者は，以下の(1)～(4)の条件を満たす場合に限り，本ソフトウェ
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
 */
#ifndef TOPPERS_TARGET_TIMER_H
#define TOPPERS_TARGET_TIMER_H

/*
 *  タイマドライバ（STM32N6570-DK用）
 */
#ifdef USE_SYSTICK_AS_TIMETICK

/*
 *  プロセッサ依存部で定義する
 */
#include "core_timer.h"

#else /* USE_SYSTICK_AS_TIMETICK */
#ifdef USE_TIM_AS_HRT
#include <sil.h>

/*
 *  STM32 Low Level Driver
 */
#include "stm32n6xx_ll_tim.h"

/*
 * タイマ割込みハンドラ登録のための定数
 *
 *  H5/C5 ターゲットと同じ構成：TIM2 を高分解能タイマ本体（フリーランニング），
 *  TIM5 を割込み通知用（ワンショット）に使う．
 *
 *  STM32N657 で TIM2/TIM5 がともに 32 ビットカウンタであることは CMSIS/HAL
 *  ヘッダで確認済み（一次情報）：
 *    STM32Cube_FW_N6_V1.1.1/Drivers/CMSIS/Device/ST/STM32N6xx/Include/
 *    stm32n657xx.h:41756-41759
 *      #define IS_TIM_32B_COUNTER_INSTANCE(INSTANCE) は
 *        TIM2 / TIM3 / TIM4 / TIM5 の各 Secure・NonSecure 別名だけを真とする
 *    → 32bit は TIM2/TIM3/TIM4/TIM5 の 4 本のみ．TIM2/TIM5 はその中に入る．
 *  割込み番号も同ヘッダで確認：TIM2_IRQn=116（:172），TIM5_IRQn=119（:175）．
 */
#define INHNO_TIMER		(TIM5_IRQn + 16)	/* 割込みハンドラ番号 */
#define INTNO_TIMER		(TIM5_IRQn + 16)	/* 割込み番号 */
#define INTPRI_TIMER (TMAX_INTPRI - 1)                   /* 割込み優先度 */
#define INTATR_TIMER TA_NULL                             /* 割込み属性 */

#ifndef TOPPERS_MACRO_ONLY

/*
 * 高分解能タイマの起動処理
 */
extern void	target_hrt_initialize(intptr_t exinf);

/*
 * 高分解能タイマの停止処理
 */
extern void	target_hrt_terminate(intptr_t exinf);

/*
 * 高分解能タイマの現在のカウント値の読出し
 */
Inline HRTCNT
target_hrt_get_current(void)
{
	return((HRTCNT)LL_TIM_GetCounter(TIM2));
}

/*
 * 高分解能タイマへの割込みタイミングの設定
 *
 * 高分解能タイマを，hrtcntで指定した値カウントアップしたら割込みを発
 * 生させるように設定する．
 */
Inline void
target_hrt_set_event(HRTCNT hrtcnt)
{
	LL_TIM_SetCounter(TIM5, 0);
	LL_TIM_SetAutoReload(TIM5, hrtcnt);
	LL_TIM_EnableCounter(TIM5);
}

/*
 *  高分解能タイマ割込みの要求
 *
 */
Inline void target_hrt_raise_event(void)
{
	LL_TIM_GenerateEvent_UPDATE(TIM5);
}

/*
 * 割込みタイミングに指定する最大値
 *
 *  TIM5 は 32 ビットカウンタなので上限は 2^32-1．ARR 設定と読出しの間の
 *  取りこぼしを避けるため，H5/C5 と同じく 4000000002U（≒4.0e9，約 4000 秒
 *  ＠1MHz 刻み）に抑える．16 ビットタイマなら 60000U 等に下げる必要があるが，
 *  TIM5 は 32 ビットなのでその必要はない．
 */
#define HRTCNT_BOUND 4000000002U

/*
 * 高分解能タイマ割込みハンドラ
 */
extern void	target_hrt_handler(void);
extern void	target_systick_handler(void);

#endif /* TOPPERS_MACRO_ONLY */
#endif /* USE_TIM_AS_HRT */
#endif /* USE_SYSTICK_AS_TIMETICK */

#endif /* TOPPERS_TARGET_TIMER_H */
