/*
 * memcpy_fast.c
 *
 *  Created on: 2019-08-09
 *
 */

#include <stdio.h>
#include <string.h>
#include "types.h"

#define MEMCPY_TEST


static void *memcpy_unaligned4(void *to, const void *from, size_t n);
static void *memcpy_aligned8(void *to, const void *from, size_t n);

void *memcpy_fast(void *to, const void *from, size_t n) {
	void *xto = to;

	if (!n)
		return xto;

	if ((((u32)to & 7) == ((u32)from & 7)) && (16 <= n)){
		u8 tmp = (u32)to & 7;
		if (tmp) {
			u8 offset = 8 - tmp;
			memcpy_unaligned4(to, from, offset);
			memcpy_aligned8((u8*)((u32)to + offset), (u8*)((u32)from + offset), n - offset);
		} else {
			memcpy_aligned8((u8*)to, (u8*)from, n);
		}
	} else {
		memcpy_unaligned4(to, from, n);
	}
	return xto;
}


/*
 * The Cortex-M4 processor supports unaligned access only for the following instructions:
 *   LDR, LDRT
 *   LDRH, LDRHT
 *   LDRSH, LDRSHT
 *   STR, STRT
 *   STRH, STRHT
 * All other load and store instructions generate a UsageFault exception if they perform an
 * unaligned access, and therefore their accesses must be address aligned.
 */
static void *memcpy_unaligned4(void *to, const void *from, size_t n) {
    void *xto = to;
    size_t temp;

    if (!n)
        return xto;

    temp = n >> 2;
    if (temp) {
        long *lto = to;
        const long *lfrom = from;
#if 0
//        for (; temp; temp--)
//            *lto++ = *lfrom++;	// LDM,STM not support unaligned access
//        for (; temp; temp--)
//            lto[temp-1] = lfrom[temp-1];
//        to = lto;
//        from = lfrom;
#else
		to = lto + temp;
		from = lfrom + temp;
        for (temp--; temp; temp--)
            lto[temp] = lfrom[temp];
        lto[0] = lfrom[0];
#endif
    }
    if (n & 2) {
        short *sto = to;
        const short *sfrom = from;
        *sto++ = *sfrom++;
        to = sto;
        from = sfrom;
    }
    if (n & 1) {
        char *cto = to;
        const char *cfrom = from;
        *cto = *cfrom;
    }
    return xto;
}

static void *memcpy_aligned8(void *to, const void *from, size_t n) {
    void *xto = to;
    size_t temp;

//    ASSERT(!((u32)to & 7));
//    ASSERT(!((u32)from & 7));

    if (!n)
        return xto;

    temp = n >> 3;
    if (temp) {
        long long *dto = to;
        const long long *dfrom = from;
        for (; temp; temp--)
            *dto++ = *dfrom++;		// LDRD, STRD
        to = dto;
        from = dfrom;
    }
    if (n & 4) {
    	long *lto = to;
		const long *lfrom = from;
		*lto++ = *lfrom++;			// LDM,STM
		to = lto;
		from = lfrom;
	}
    if (n & 2) {
        short *sto = to;
        const short *sfrom = from;
		*sto++ = *sfrom++;
        to = sto;
        from = sfrom;
    }
    if (n & 1) {
        char *cto = to;
        const char *cfrom = from;
        *cto = *cfrom;
    }
    return xto;
}


#if defined(MEMCPY_TEST)

#define TEST_BUFF_SIZE		10240

void memcpy_test(void) {

	static u8 src[TEST_BUFF_SIZE + 16] __attribute__((aligned(8)));
	static u8 dest[TEST_BUFF_SIZE + 16] __attribute__((aligned(8))) = {0};

	for (u32 i = 0; i < TEST_BUFF_SIZE+16; i++) {
		src[i] = i;
	}

	__disable_irq();

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

	RCC_ClocksTypeDef rccClock;
	RCC_GetClocksFreq(&rccClock);

	TIM_TimeBaseInitTypeDef  TIM_TimeBaseInitStruct;
	TIM_TimeBaseStructInit(&TIM_TimeBaseInitStruct);
	TIM_TimeBaseInitStruct.TIM_Period = 1000000000 - 1;
	TIM_TimeBaseInitStruct.TIM_Prescaler = (u16)(((rccClock.PCLK1_Frequency * 2) / 1000000) - 1);
	TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM5, &TIM_TimeBaseInitStruct);
	TIM_Cmd(TIM5, ENABLE);

	u32 t1, t2, t3, t4;
	for (u32 a = 0; a < 8; a++) {
		for (u32 b = 0; b < 8; b++) {
			for (u32 c = 0; c < 8; c++) {
				memset(dest, 0, TEST_BUFF_SIZE + 8);
				TIM5->CNT = 0;
				t1 = TIM5->CNT;
				memcpy_fast(dest + a, src + b, TEST_BUFF_SIZE + c);
				t2 = TIM5->CNT;
				int ret = memcmp(dest + a, src + b, TEST_BUFF_SIZE + c);

				t3 = TIM5->CNT;
				memcpy(dest + a, src + b, TEST_BUFF_SIZE + c);
				t4 = TIM5->CNT;

				printf("[%s][%d,%d,%d] %d,%d\n", (!ret) ? "-OK-" : "XXXX", a, b, c, t2 - t1, t4 - t3);
			}
		}
	}

	TIM_Cmd(TIM5, DISABLE);

	__enable_irq();
}

#endif	// #if defined(MEMCPY_TEST)

