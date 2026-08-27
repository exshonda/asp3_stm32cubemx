/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 * 
 *  Copyright (C) 2016-2020 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: target_timer.c 292 2021-10-11 12:27:17Z ertl-komori $
 */

/*
 *		タイマドライバ（TIM用）
 *		 TIM2をフリーランニング（高分解能タイマ本体），TIM5を割込み通知用に
 *		 使用する（H5 ターゲットと同じ構成．どちらも32ビットカウンタ）．
 *
 *  周辺の初期化（クロック供給・プリスケーラ・ワンショット(OPM)設定）は
 *  STM32CubeMX2 生成の初期化コードが行う．ここでは HAL2 の API には依存せず，
 *  LL とレジスタ直接操作だけでカウンタの起動・停止と割込み処理を行う．
 */

#include "kernel_impl.h"
#include "time_event.h"
#include "target_timer.h"
#include <sil.h>

/*
 *  CubeMX2 生成側（HAL）の tick インクリメント関数．HAL のヘッダを取り込むと
 *  HAL2 の API 変更に引きずられるため，宣言だけをここに置く．
 */
extern void HAL_IncTick(void);

/*
 * タイマの起動処理
 */
void
target_hrt_initialize(intptr_t exinf)
{
	/*
	 *  高分解能タイマ本体（TIM2）：フリーランニングで動かし続ける．
	 *  更新割込みは使わない（カウンタ読出しのみ）．
	 */
	LL_TIM_DisableIT_UPDATE(TIM2);
	LL_TIM_SetCounter(TIM2, 0);
	LL_TIM_EnableCounter(TIM2);

	/*
	 *  割込み通知用（TIM5）：ワンショット（OPM）で使用する．
	 *  カウンタは target_hrt_set_event() で都度起動するのでここでは止めておく．
	 */
	LL_TIM_DisableCounter(TIM5);
	LL_TIM_EnableOnePulseMode(TIM5);
	LL_TIM_ClearFlag_UPDATE(TIM5);
	LL_TIM_EnableIT_UPDATE(TIM5);
}

/*
 * タイマの停止処理
 */
void
target_hrt_terminate(intptr_t exinf)
{
	LL_TIM_DisableIT_UPDATE(TIM5);
	LL_TIM_DisableCounter(TIM5);
	LL_TIM_DisableCounter(TIM2);
}

/*
 *  タイマ割込みハンドラ
 */
void
target_hrt_handler(void)
{
	/*
	 *  更新割込み要求をクリアする．OPM のためカウンタは自動で停止している．
	 */
	LL_TIM_ClearFlag_UPDATE(TIM5);
	LL_TIM_DisableCounter(TIM5);

	/*
	 *  高分解能タイマ割込みを処理する．
	 */
	signal_time();
}

/*
 *  SysTick 割込みハンドラ
 *
 *  カーネルは SysTick を使わない（USE_TIM_AS_HRT）が，HAL の時間待ち
 *  （HAL_Delay 等）が使う tick を進めるために CubeMX2 生成側の tick
 *  インクリメント関数を呼ぶ．
 *  TODO(CubeMX2)：HAL2 での tick インクリメント関数名を確認して合わせる
 *  （classic HAL は HAL_IncTick()）．
 */
void
target_systick_handler(void)
{
	HAL_IncTick();
}
