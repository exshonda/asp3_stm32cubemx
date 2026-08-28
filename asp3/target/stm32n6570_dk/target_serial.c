/*
 * シリアルインタフェースドライバのターゲット依存部（非TECS版専用）
 *
 * $Id: target_serial.h 289 2021-08-05 14:44:10Z ertl-komori $
 */

/*
 *  STM32N6570-DK の ST-LINK VCP は USART1（PE5=TX / PE6=RX，115200bps）．
 *  USART インスタンスと IRQn は target_serial.h の SIO_USART / SIO_USART_IRQn
 *  の 1 ブロックに集約してある（差し替えはそこだけ）．
 *
 *  H5 ターゲットは BSP（stm32h5xx_nucleo.h の BSP_COM_Init / hcom_uart[]）と
 *  HAL の割込み API（HAL_UART_Receive_IT 等）に依存していたが，C5 移植で
 *  「BSP・HAL に依存せず USART レジスタ直叩き＋LL」に切り替えた
 *  （docs/porting-c562re.md §4）．N6 も同じ方針を踏襲する．
 *  CubeMX 生成側に任せるのはピン設定・クロック供給・ボーレート設定
 *  （USART1 を Asynchronous 115200 8N1 で有効化）までで，
 *  送受信割込みの制御はすべて本ファイルが握る．
 *
 *  NVIC の優先度設定・割込み許可はカーネルが行う（target_serial.cfg の
 *  CFG_INT / DEF_INH）．H5 版にあった HAL_NVIC_SetPriority()／
 *  HAL_NVIC_EnableIRQ() 相当の操作はカーネルの管理と二重になるため行わない．
 */

#include <stdint.h>
#include "t_stddef.h"
#include "target_serial.h"
#include "target_syssvc.h"

/*
 *  レジスタ・ビット名の差異吸収
 *
 *  新しい世代の USART では TXE/RXNE 系のビット名に FIFO 用の別名
 *  （TXE_TXFNF / RXNE_RXFNE）が使われる．どちらの名前で定義されていても
 *  ビルドできるようにする．
 */
#if defined(USART_ISR_TXE_TXFNF)
#define SIO_ISR_TXE		USART_ISR_TXE_TXFNF
#else
#define SIO_ISR_TXE		USART_ISR_TXE
#endif
#if defined(USART_ISR_RXNE_RXFNE)
#define SIO_ISR_RXNE	USART_ISR_RXNE_RXFNE
#else
#define SIO_ISR_RXNE	USART_ISR_RXNE
#endif
#if defined(USART_CR1_TXEIE_TXFNFIE)
#define SIO_CR1_TXEIE	USART_CR1_TXEIE_TXFNFIE
#else
#define SIO_CR1_TXEIE	USART_CR1_TXEIE
#endif
#if defined(USART_CR1_RXNEIE_RXFNEIE)
#define SIO_CR1_RXNEIE	USART_CR1_RXNEIE_RXFNEIE
#else
#define SIO_CR1_RXNEIE	USART_CR1_RXNEIE
#endif

/*
 *  VCP に接続された USART（SIO_USART）は target_serial.h で定義している．
 */

struct sio_port_control_block
{
    USART_TypeDef *usart;           /* USART レジスタ */
    intptr_t exinf;                 /* 拡張情報 */
    bool_t opened;                  /* オープン済みか */
    bool_t rdy_snd;                 /* 送信可能コールバック許可 */
    bool_t rdy_rcv;                 /* 受信通知コールバック許可 */
    uint8_t rcv_buf[256];           /* 受信バッファ */
    uint32_t rcv_wpos;              /* 受信バッファ書き込み位置 */
    uint32_t rcv_rpos;              /* 受信バッファ読み込み位置 */
};

/*
 *  SIOポート管理ブロックのエリア
 */
static SIOPCB siopcb_table[TNUM_PORT] = {
    {SIO_USART, 0, false, false, false, {0}, 0, 0},
};

/*
 *  SIOポートIDから管理ブロックを取り出すためのマクロ
 */
#define INDEX_SIOP(siopid)	((uint_t)((siopid) - 1))
#define get_siopcb(siopid)	(&(siopcb_table[INDEX_SIOP(siopid)]))

#define RCV_BUF_SIZE(p_siopcb)	((uint32_t) sizeof((p_siopcb)->rcv_buf))

/*
 *  USART の有効化（UE / TE / RE）
 *
 *  classic HAL（HAL_UART_Init）は UE を立てるが，CubeMX の生成順や
 *  MX_USART1_UART_Init() の呼ばれ方に依存したくないため，未有効なら
 *  自分で UE / TE / RE を立てる（冪等．既に UE が立っていれば何もしない）．
 *
 *  MX_* の初期化が main() の先頭で走った後（＝クロックとピンが設定済み）
 *  にしか呼ばれない前提．レジスタアクセスはクロック供給後なので安全．
 */
Inline void
sio_usart_enable(USART_TypeDef *usart)
{
    if ((usart->CR1 & USART_CR1_UE) == 0U) {
        usart->CR1 |= USART_CR1_TE | USART_CR1_RE;
        usart->CR1 |= USART_CR1_UE;
    }
}

/*
 * SIOドライバの初期化
 */
void sio_initialize(intptr_t exinf)
{
    for (uint_t i = 0; i < TNUM_PORT; i++) {
        siopcb_table[i].exinf = exinf;
        siopcb_table[i].opened = false;
        siopcb_table[i].rdy_snd = false;
        siopcb_table[i].rdy_rcv = false;
        siopcb_table[i].rcv_wpos = 0;
        siopcb_table[i].rcv_rpos = 0;
        sio_usart_enable(siopcb_table[i].usart);
    }
}

/*
 * SIOドライバの終了処理
 */
void sio_terminate(intptr_t exinf)
{
    uint_t	i;
    SIOPCB	*p_siopcb;

    for (i = 0; i < TNUM_PORT; i++) {
        p_siopcb = &(siopcb_table[i]);
        if (p_siopcb->opened) {
            /*
             *  オープンされているSIOポートのクローズ
             */
            sio_cls_por(p_siopcb);
        }
    }
}

/*
 * SIOポートのオープン
 */
SIOPCB *sio_opn_por(ID siopid, intptr_t exinf)
{
    SIOPCB *p_siopcb;

    if (siopid > TNUM_PORT) {
        return NULL;
    }

    p_siopcb = get_siopcb(siopid);
    /* すでにオープンされている場合はNULLを返す */
    if (p_siopcb->opened) {
        return NULL;
    }

    p_siopcb->exinf = exinf;
    p_siopcb->rcv_wpos = 0;
    p_siopcb->rcv_rpos = 0;
    p_siopcb->rdy_snd = false;
    p_siopcb->rdy_rcv = false;
    p_siopcb->opened = true;

    /*
     *  受信は常に割込みで取り込み，リングバッファに溜める（受信通知
     *  コールバックの許可状態とは独立）．送信割込みは送信可能コール
     *  バックが許可されている間だけ有効にする（sio_ena_cbr）．
     */
    p_siopcb->usart->ICR = USART_ICR_ORECF | USART_ICR_NECF
                         | USART_ICR_FECF | USART_ICR_PECF;
    p_siopcb->usart->CR1 &= ~SIO_CR1_TXEIE;
    p_siopcb->usart->CR1 |= SIO_CR1_RXNEIE;

    return p_siopcb;
}

/*
 * SIOポートのクローズ
 */
void sio_cls_por(SIOPCB *p_siopcb)
{
    p_siopcb->usart->CR1 &= ~(SIO_CR1_TXEIE | SIO_CR1_RXNEIE);
    p_siopcb->rdy_snd = false;
    p_siopcb->rdy_rcv = false;
    p_siopcb->opened = false;
}

/*
 * SIOポートへの文字送信
 *
 *  送信データレジスタが空いていれば送信して true，空いていなければ
 *  false を返す（false のとき上位は送信可能コールバックを許可して待つ）．
 */
bool_t sio_snd_chr(SIOPCB *p_siopcb, char ch)
{
    if ((p_siopcb->usart->ISR & SIO_ISR_TXE) == 0U) {
        return false;
    }
    p_siopcb->usart->TDR = (uint8_t) ch;
    return true;
}

/*
 * SIOポートからの文字受信
 */
int_t sio_rcv_chr(SIOPCB *p_siopcb)
{
    uint8_t ch;

    if (p_siopcb->rcv_wpos == p_siopcb->rcv_rpos) {
        return -1;                  /* 受信バッファが空 */
    }
    ch = p_siopcb->rcv_buf[p_siopcb->rcv_rpos];
    p_siopcb->rcv_rpos = (p_siopcb->rcv_rpos + 1) % RCV_BUF_SIZE(p_siopcb);
    return (int_t) ch;
}

/*
 * SIOポートからのコールバックの許可
 */
void sio_ena_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
    switch (cbrtn) {
    case SIO_RDY_SND:
        p_siopcb->rdy_snd = true;
        p_siopcb->usart->CR1 |= SIO_CR1_TXEIE;
        break;
    case SIO_RDY_RCV:
        p_siopcb->rdy_rcv = true;
        break;
    default:
        break;
    }
}

/*
 * SIOポートからのコールバックの禁止
 */
void sio_dis_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
    switch (cbrtn) {
    case SIO_RDY_SND:
        p_siopcb->usart->CR1 &= ~SIO_CR1_TXEIE;
        p_siopcb->rdy_snd = false;
        break;
    case SIO_RDY_RCV:
        p_siopcb->rdy_rcv = false;
        break;
    default:
        break;
    }
}

/*
 * SIOポートへのポーリング出力
 *
 * syslogの低レベル出力．カーネル起動前・CPUロック中・例外文脈でも
 * 動作するよう，HAL・stdio を一切介さず SIO_USART（ST-LINK VCP）の
 * レジスタを直接ポーリングする（porting仕様の「SIOポーリング出力」．
 * vector-vtor-pitfalls.md §3：stdio 経由だと CubeMX の syscalls.c が
 * __io_putchar 未実装のため一切出力されず，デバッグ不能になる）．
 */
static void n6_uart_fput(char c)
{
    while ((SIO_USART->ISR & SIO_ISR_TXE) == 0U) {
        /* 送信データレジスタが空くまでポーリング */
    }
    SIO_USART->TDR = (uint8_t) c;
}

/*
 * SIOポートへの文字出力
 *
 *  バナー出力は sio_initialize（ATT_INI）より先に走り得るため，
 *  ここでも未有効なら有効化する．
 */
void target_fput_log(char c)
{
    sio_usart_enable(SIO_USART);
    if (c == '\n') {
        n6_uart_fput('\r');
    }
    n6_uart_fput(c);
}

/*
 *  SIOの割込みハンドラ
 */
void sio_handler(void)
{
    SIOPCB			*p_siopcb = &(siopcb_table[0]);
    USART_TypeDef	*usart = p_siopcb->usart;
    uint32_t		isr = usart->ISR;
    uint32_t		next;

    /*
     *  受信エラー（オーバラン等）はフラグをクリアして捨てる．
     */
    if ((isr & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE)) != 0U) {
        usart->ICR = USART_ICR_ORECF | USART_ICR_NECF
                   | USART_ICR_FECF | USART_ICR_PECF;
    }

    /*
     *  受信：リングバッファに取り込む（満杯なら捨てる）．
     *  RDR の読出しで RXNE はクリアされる．
     */
    if ((isr & SIO_ISR_RXNE) != 0U) {
        uint8_t ch = (uint8_t) usart->RDR;

        next = (p_siopcb->rcv_wpos + 1) % RCV_BUF_SIZE(p_siopcb);
        if (next != p_siopcb->rcv_rpos) {
            p_siopcb->rcv_buf[p_siopcb->rcv_wpos] = ch;
            p_siopcb->rcv_wpos = next;
        }

        /*
         *  受信通知コールバックルーチンを呼び出す．
         */
        if (p_siopcb->rdy_rcv) {
            sio_irdy_rcv(p_siopcb->exinf);
        }
    }

    /*
     *  送信：送信可能コールバックルーチンを呼び出す．コールバック中で
     *  送信するデータが無くなると sio_dis_cbr() が呼ばれて TXEIE が
     *  落ちる（そうしないと TXE は立ちっぱなしなので割込みが止まらない）．
     */
    if ((isr & SIO_ISR_TXE) != 0U
                    && (usart->CR1 & SIO_CR1_TXEIE) != 0U) {
        if (p_siopcb->rdy_snd) {
            sio_irdy_snd(p_siopcb->exinf);
        }
        else {
            usart->CR1 &= ~SIO_CR1_TXEIE;
        }
    }
}
