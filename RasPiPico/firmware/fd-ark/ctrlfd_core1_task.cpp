/**
 * FD-ARK (RaspberryPiPico firmware)
 * Copyright (c) 2023 Harumakkin.
 * SPDX-License-Identifier: MIT
 */
// https://spdx.org/licenses/

#include <stdio.h>
#include "pico/stdlib.h"
#include "def_gpio.h"
#include "fd-ark.pio.h"
#include "global.h"
#include "ctrlfd_core1_task.h"

static void ctrlFddMotorOn()
{
	gpio_put(GP_FDD_CTRL_DRIVE_SELECT_0_N, 0);
	gpio_put(GP_FDD_CTRL_MOTOR_ON_0_N, 0 );
	sleep_ms(500);
	return;
}

static void ctrlFddMotorOff()
{
	gpio_put(GP_FDD_CTRL_MOTOR_ON_0_N, 1 );
	gpio_put(GP_FDD_CTRL_DRIVE_SELECT_0_N, 1);
	return;
}

static void ctrlMoveHeadOneTrack()
{
	// step plus
	gpio_put(GP_FDD_CTRL_STEP_N, 0);
	sleep_us(2);
	gpio_put(GP_FDD_CTRL_STEP_N, 1);
	// 1-step完了後に4ms待つこと。
	sleep_ms(4);
	return;
}

static void ctrlMoveToTrack00()
{
	gpio_put(GP_FDD_CTRL_DIRECTION_N, 1/*外側へ*/);
	sleep_ms(4);

	for(;;){
		if( gpio_get(GP_FDD_STS_TRACK00_N) == 0 )
			break;
		ctrlMoveHeadOneTrack();
	}
	// seek完了後に18ms待つこと。
	sleep_ms(18);
	return;
}

static void ctrlSide(const uint32_t no)	// 1=side-0, 0=side-1  ← 直観と反するので注意
{
	gpio_put(GP_FDD_CTRL_SIDE1_N, no);
	sleep_us(100);
	return;
}

static void divideCode(const uint32_t v32, uint8_t *pIndex8, uint8_t *pData8)
{
	*pIndex8 =
		((v32 >> (15- 7)) & 0x0080) | ((v32 >> (13- 6)) & 0x0040) |
		((v32 >> (11- 5)) & 0x0020) | ((v32 >> ( 9- 4)) & 0x0010) | 
		((v32 >> ( 7- 3)) & 0x0008) | ((v32 >> ( 5- 2)) & 0x0004) |
		((v32 >> ( 3- 1)) & 0x0002) | ((v32 >> ( 1- 0)) & 0x0001);
	*pData8 = 
		((v32 >> (14- 7)) & 0x0080) | ((v32 >> (12- 6)) & 0x0040) |
		((v32 >> (10- 5)) & 0x0020) | ((v32 >> ( 8- 4)) & 0x0010) | 
		((v32 >> ( 6- 3)) & 0x0008) | ((v32 >> ( 4- 2)) & 0x0004) |
		((v32 >> ( 2- 1)) & 0x0002) | ((v32 >> ( 0- 0)) & 0x0001);
	return;
}

static void mergeCode(uint32_t *pTmp32, uint8_t dt, uint8_t gt)
{
	*pTmp32 = 
		(((uint32_t)gt&0x80)<<(15-7)) | (((uint32_t)dt&0x80)<<(14-7)) |
		(((uint32_t)gt&0x40)<<(13-6)) | (((uint32_t)dt&0x40)<<(12-6)) |
		(((uint32_t)gt&0x20)<<(11-5)) | (((uint32_t)dt&0x20)<<(10-5)) |
		(((uint32_t)gt&0x10)<<( 9-4)) | (((uint32_t)dt&0x10)<<( 8-4)) |
		(((uint32_t)gt&0x08)<<( 7-3)) | (((uint32_t)dt&0x08)<<( 6-3)) |
		(((uint32_t)gt&0x04)<<( 5-2)) | (((uint32_t)dt&0x04)<<( 4-2)) |
		(((uint32_t)gt&0x02)<<( 3-1)) | (((uint32_t)dt&0x02)<<( 2-1)) |
		(((uint32_t)gt&0x01)<<( 1-0)) | (((uint32_t)dt&0x01)<<( 0-0));
	*pTmp32 <<= 16;
	return;
}

static void readTrackAction(TASKCMD_PARAMS *pWk)
{
	// FDD読み込み用のPIO の開始
	PIO pio = pio0;
	uint offset = pio_add_program( pio, &pio_fddread_program );
	uint state_machine = pio_claim_unused_sm( pio, true );
	pio_fddread_program_init( pio, state_machine, offset, GP_FDD_STS_READ_DATA_N, 2);

	// FDDからの読み込み
	uint8_t index8 = 0, data8 = 0, index8old = 0;
	bool bOnSampling = false;
	uint32_t count = 0;
	uint8_t firstIndex8 = 0;
	while(count < SIZE_TRACK_DATA){
		uint32_t tmp32 = pio_sm_get_blocking(pio, state_machine);
		divideCode(tmp32, &index8, &data8);
		if (((index8 & 0x81) == 0x80) || (index8old == 0xff && index8 == 0) ) {
			if (!bOnSampling) {
				bOnSampling = true;
				firstIndex8 = index8;
			}
			else {
				pWk->TrackData.Binary[count++] = data8;
				pWk->TrackData.LastIndex8 = index8;
				break;
			}								
		}
		if (bOnSampling) {
			pWk->TrackData.Binary[count++] = data8;
		}
		index8old = index8;
	}
	pWk->TrackData.Count = count;

	// PIO 停止＆削除
	pio_sm_set_enabled(pio, state_machine, false);
	pio_sm_unclaim(pio, state_machine); 
	pio_remove_program(pio, &pio_fddread_program, offset);

	// firstIndex8、index信号の最初の8bitsが格納されている
	// この8bitsのmsbからONになっているビットを数える -> cnt
	// Binary[0]以降に格納されてるread信号の不要なビット数である
	int cnt = 0;
	for( int t = 0; t < 8; ++t){
		if (((firstIndex8>>(7-t))&0x01) == 0)
			break;
		cnt++;
	}
	// cnt の分だけビットをシフトしたデータを作成する
	if ( 0 < count) { 
		for (uint32_t c = 0; c < count; ++c) {
			const uint8_t t1 = pWk->TrackData.Binary[c+0] << cnt;
			const uint8_t t2 = pWk->TrackData.Binary[c+1] >> (8-cnt);
			pWk->TrackData.Binary[c+0] = t1 | t2;
		}
		pWk->TrackData.Binary[count-1] <<= cnt;
	}
	// トラックデータの最後のバイトデータ用の、WRITE GATE信号のビットを格納する
	pWk->TrackData.LastIndex8 = (pWk->TrackData.LastIndex8^0xff) | ((0xff<<cnt)^0xff);

	// 完了を通知
	sem_acquire_blocking(&pWk->sem2);
	pWk->bTrackDataOk = true;
	sem_release(&pWk->sem2);
	return;
}

static void writeTrackAction(TASKCMD_PARAMS *pWk)
{
	// トラックへの書き込みを開始する
	// ＦＤＤ読み込み用のPIO の開始
	PIO pio = pio0;
	uint offset = pio_add_program( pio, &pio_fddwrite_program );
	uint state_machine = pio_claim_unused_sm( pio, true );
	pio_fddwrite_program_init(
			pio, state_machine, offset,
			GP_FDD_CTRL_WRITE_DATA_N, 2,
			GP_FDD_STS_INDEX_N);

	// GATE&WRITE信号を作成してPIOに渡す
	uint32_t tmp32;
	for (uint32_t t = 0; t < pWk->TrackData.Count-1; ++t) {
		mergeCode(&tmp32, pWk->TrackData.Binary[t], 0x00);
		pio_sm_put_blocking(pio, state_machine, tmp32);
	}
	mergeCode(&tmp32, pWk->TrackData.Binary[ pWk->TrackData.Count-1], pWk->TrackData.LastIndex8);
	pio_sm_put_blocking(pio, state_machine, tmp32);
	pio_sm_put_blocking(pio, state_machine, 0xFFFFFFFF);
	while(!pio_sm_is_tx_fifo_empty(pio,state_machine));

	// PIO 停止＆削除
	pio_sm_set_enabled(pio, state_machine, false);
	pio_sm_unclaim(pio, state_machine); 
	pio_remove_program(pio, &pio_fddwrite_program, offset);

	// 完了を通知
	sem_acquire_blocking(&pWk->sem2);
	pWk->bTrackDataOk = false;
	sem_release(&pWk->sem2);
	return;
}

void CtrlFD_Core1Main()
{
	for (;;) {
		TASKCMD cmd;
		sem_acquire_blocking(&g_TaskCmd.sem1);
		bool b = g_TaskCmd.cmds.Pop(&cmd);
		sem_release(&g_TaskCmd.sem1);
		if (!b || cmd == TASKCMD::NONE)
			continue; 

		switch(cmd)
		{
			case TASKCMD::FDDCTRL_MOTOR_ON:
				ctrlFddMotorOn();
				break;
			case TASKCMD::FDDCTRL_MOTOR_OFF:
				ctrlFddMotorOff();
				break;
			case TASKCMD::FDDCTRL_MOVE_TO_TRACK00:
				ctrlMoveToTrack00();
				break;
			case TASKCMD::FDDCTRL_CHANGE_TO_SIDE_0:
				ctrlSide(1/*SIDE-0*/);
				break;
			case TASKCMD::FDDCTRL_CHANGE_TO_SIDE_1:
				ctrlSide(0/*SIDE-1*/);
				break;
			case TASKCMD::FDDCTRL_PREV_TRACK:
				gpio_put(GP_FDD_CTRL_DIRECTION_N, 1/*外側へ*/);
				sleep_ms(4);
				ctrlMoveHeadOneTrack();
				sleep_ms(18);
				break;
			case TASKCMD::FDDCTRL_NEXT_TRACK:
				gpio_put(GP_FDD_CTRL_DIRECTION_N, 0/*内側へ*/);
				sleep_ms(4);
				ctrlMoveHeadOneTrack();
				sleep_ms(18);
				break;
			case TASKCMD::FDDCTRL_START_READ_TRACK:
				readTrackAction(&g_TaskCmd );
				break;
			case TASKCMD::FDDCTRL_START_WRITE_TRACK:
				writeTrackAction(&g_TaskCmd);
				break;
			default:
				break;
		}
	}
	return;
}

