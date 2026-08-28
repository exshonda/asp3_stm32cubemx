/*
 * シリアルインタフェースドライバのターゲット依存部（非TECS版専用）
 *
 * $Id: target_serial.h 289 2021-08-05 14:44:10Z ertl-komori $
 */

#ifndef TOPPERS_TARGET_SERIAL_H
#define TOPPERS_TARGET_SERIAL_H

#include "stm32cubemx.h"

/*
 *  N6 ターゲットは HAL の API に依存せず，CMSIS デバイスヘッダ（レジスタ定義・
 *  IRQn_Type）と LL だけを使う．SIO_USART / SIO_USART_IRQn を参照するため
 *  ここで先に取り込む．
 */
#include "stm32n6xx.h"
#include "stm32n6xx_ll_usart.h"

/*
 *  ============================================================
 *  ★VCP に接続された USART の定義（差し替えるのはこの1ブロックだけ）
 *  ============================================================
 *
 *  STM32N6570-DK（MB1939）の ST-LINK VCP は USART1（PE5=TX / PE6=RX，AF7）．
 *  一次情報：STM32Cube_FW_N6_V1.1.1/Drivers/BSP/STM32N6570-DK/
 *            stm32n6570_discovery.h:239-254
 *              #define COM1_UART        USART1
 *              #define COM1_TX_PIN      GPIO_PIN_5 / COM1_TX_GPIO_PORT GPIOE
 *              #define COM1_RX_PIN      GPIO_PIN_6 / COM1_RX_GPIO_PORT GPIOE
 *              #define COM1_TX_AF       GPIO_AF7_USART1
 *  割込み番号：stm32n657xx.h:215  USART1_IRQn = 159
 *
 *  TODO(board)：ボード側 CubeMX プロジェクト（別担当）で有効化した USART が
 *  USART1 でなかった場合は，この2行を書き換えるだけで追従できる．
 *  ボーレートは 115200 8N1 を想定（CubeMX 側で設定．本ファイルは触らない）．
 */
#define SIO_USART			USART1
#define SIO_USART_IRQn		USART1_IRQn

/*
 * SIO割込みハンドラ登録のための定数
 */
#define INHNO_USART		(SIO_USART_IRQn + 16)	/* 割込みハンドラ番号 */
#define INTNO_USART		(SIO_USART_IRQn + 16)	/* 割込み番号 */
#define INTPRI_USART	(TMAX_INTPRI - 1)   /* 割込み優先度 */
#define INTATR_USART	TA_NULL             /* 割込み属性 */

/*
 *  シリアルポート数の定義
 */
#define TNUM_PORT 1

/*
 *  コールバックルーチンの識別番号
 */
#define SIO_RDY_SND    1U        /* 送信可能コールバック */
#define SIO_RDY_RCV    2U        /* 受信通知コールバック */

#ifndef TOPPERS_MACRO_ONLY

/*
 *  シリアルI/Oポート管理ブロックの定義
 */
typedef struct sio_port_control_block    SIOPCB;

/*
 *  SIOドライバの初期化
 */
extern void sio_initialize(EXINF exinf);

/*
 *  SIOドライバの終了処理
 */
extern void sio_terminate(EXINF exinf);

/*
 *  SIOの割込みハンドラ
 */
extern void sio_handler(void);

/*
 *  SIOポートのオープン
 */
extern SIOPCB *sio_opn_por(ID siopid, EXINF exinf);

/*
 *  SIOポートのクローズ
 */
extern void sio_cls_por(SIOPCB *p_siopcb);

/*
 *  SIOポートへの文字送信
 */
extern bool_t sio_snd_chr(SIOPCB *p_siopcb, char c);

/*
 *  SIOポートからの文字受信
 */
extern int_t sio_rcv_chr(SIOPCB *p_siopcb);

/*
 *  SIOポートからのコールバックの許可
 */
extern void sio_ena_cbr(SIOPCB *p_siopcb, uint_t cbrtn);

/*
 *  SIOポートからのコールバックの禁止
 */
extern void sio_dis_cbr(SIOPCB *p_siopcb, uint_t cbrtn);

/*
 *  SIOポートからの送信可能コールバック
 */
extern void sio_irdy_snd(EXINF exinf);

/*
 *  SIOポートからの受信通知コールバック
 */
extern void sio_irdy_rcv(EXINF exinf);

#endif /* TOPPERS_MACRO_ONLY */

#endif /* TOPPERS_TARGET_SERIAL_H */
