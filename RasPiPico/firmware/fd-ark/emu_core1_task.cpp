/**
 * FD-ARK (RaspberryPiPico firmware)
 * Copyright (c) 2023 Harumakkin.
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <memory.h>
#include "pico/stdlib.h"
#include "fd-ark.pio.h"
 
#include "def_gpio.h"
#include "sdfat.h"
#include "global.h"
#include "gamen.h"
#include "timerobj.h"

static const bool ACTIVE = true;
static const bool NON_ACTIVE = false;
static const bool SIGNAL_H = true;
static const bool SIGNAL_L = false;

bool bEffectiveDisk = true;

struct OUTPUTPIN
{
	bool	bDiskChange;
	bool	bWriteProtect;
	bool	bReady;
	bool	bIndex;
	bool	bTrack00;
};

struct INPUTPIN
{
	bool	bDriveSelect;
	bool	bMotorON;
};

static OUTPUTPIN g_Output;
static INPUTPIN g_Input;

inline bool getGpio(const int pin)
{
	return (gpio_get(pin)==0)?false:true;
}

inline void setGpio(const int pin, const bool b)
{
	gpio_put(pin, (b)?1:0);
	return;
}

static uint8_t getStepSignalEdge(const bool b)
{
	static uint8_t steps = 0x00;
	steps = (steps << 1) & 0x02;
	steps |= (b) ? 0x01: 0x00;
	return steps;
}

static void requestTrackData(TASKCMD_PARAMS *pCmd, const int trackNo, const int sideNo, const bool bMod)
{
	sem_acquire_blocking(&pCmd->sem3);
	++pCmd->sem3cnt;
	pCmd->RequestDataTrackNo = trackNo;
	pCmd->RequestDataSideNo = sideNo;
	pCmd->bModifiedTrackData = bMod;
	sem_release(&pCmd->sem3);
	sem_acquire_blocking(&pCmd->sem2);
	pCmd->bTrackDataOk = false;
	sem_release(&pCmd->sem2);
	return;
}

static uint8_t trackNo = 0;
static uint8_t sideNo = 0;
static void intrHandler_StepSignal(uint gpio, uint32_t emask)
{
	if( gpio_get(GP_FDD_CTRL_DRIVE_SELECT_0_N) != 0 )
		return;
	if (gpio == GP_FDD_CTRL_STEP_N)	{
		gpio_set_irq_enabled(GP_FDD_CTRL_STEP_N, GPIO_IRQ_EDGE_FALL, false);
		if( gpio_get(GP_FDD_CTRL_DIRECTION_N) == 0 ) {
			if (trackNo < MAX_TRACK_NO){
				++trackNo;
			}
		}else{
			if (0 < trackNo) {
				--trackNo;
			}
		}
		g_Output.bDiskChange = NON_ACTIVE;
		sideNo = (gpio_get(GP_FDD_CTRL_SIDE1_N)==0)?1:0;
		gpio_set_irq_enabled(GP_FDD_CTRL_STEP_N, GPIO_IRQ_EDGE_FALL, true);
	}
	else if (gpio == GP_FDD_CTRL_SIDE1_N) {
		sideNo = ((emask&GPIO_IRQ_EDGE_FALL)==0)?0:1;
	}
	return;
}

void Emu_Core1Main()
{
	static const int PACKSIZE = 4;
	int numReadDataCnt = 0;
	int readDataCnt = 0;
	int writeDataCnt = -PACKSIZE;

	TimerObj delayReadySignalCnt;
	TimerObj afterStepCnt;
	TimerObj delayChangeTrackCnt;
	TimerUsObj outputIndexlCnt;

	bool bInsertDisk = false;				// 有効なディスクが選択されている
	bool bMotor = false;
	uint8_t tarckNoOld = 255;;
	uint8_t sideNoOld = 255;
	uint8_t diskChgCndOld = 0;
	bool bModifiedTrackData = false;
	bool bDriveSelectOld = false;

	g_Output.bDiskChange 	= ACTIVE;
	g_Output.bWriteProtect 	= NON_ACTIVE;
	g_Output.bReady 		= NON_ACTIVE;
	g_Output.bIndex 		= NON_ACTIVE;
	g_Output.bTrack00		= (trackNo==0) ? ACTIVE : NON_ACTIVE;

	// PIO の開始
	//		READ DATAの出力、WRITE GATEの入力、WRITE DATAの入力をそれぞれ別のステートマシンで行う。
	PIO pio = pio0;
	uint smReadData		= pio_claim_unused_sm( pio, true );
	uint smWriteGate	= pio_claim_unused_sm( pio, true );
	uint smWriteData	= pio_claim_unused_sm( pio, true );
	uint adReadData 	= pio_add_program( pio, &pio_output_read_program );
	uint adWriteGate 	= pio_add_program( pio, &pio_input_wgate_program );
	uint adWriteData 	= pio_add_program( pio, &pio_input_wdata_program );
	pio_read_write_data_program_init(
			pio,
			smReadData,	smWriteGate,	smWriteData,
			adReadData,	adWriteGate,	adWriteData,
			GP_FDD_STS_READ_DATA_N,	GP_FDD_CTRL_WRITE_GATEA_N,	GP_FDD_CTRL_WRITE_DATA_N);

	afterStepCnt.Start(1);

	requestTrackData(&g_TaskCmd, trackNo, sideNo, bModifiedTrackData);
	bool bRequestChangeTrack = false;

	gpio_set_irq_enabled_with_callback(
		GP_FDD_CTRL_STEP_N,	GPIO_IRQ_EDGE_FALL, true, intrHandler_StepSignal);
	gpio_set_irq_enabled(
		GP_FDD_CTRL_SIDE1_N, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);

	for(;;)
	{
		// ディスクの選択状態、トラックデータの有効性の確認
		sem_acquire_blocking(&g_TaskCmd.sem2);
		bInsertDisk = g_TaskCmd.bInsertDisk;
		bool bTrackDataOk = g_TaskCmd.bTrackDataOk;
		uint8_t diskChgCnd = g_TaskCmd.DiskChangeCnt;
		bool bSdWp = g_TaskCmd.bSdCardWriteProtect;
		sem_release(&g_TaskCmd.sem2);

		// ディスクが取り換えられた
		if( diskChgCndOld != diskChgCnd) {
			diskChgCndOld = diskChgCnd;
			g_Output.bDiskChange = ACTIVE;
			//
			numReadDataCnt = g_TaskCmd.TrackData.Count;
			readDataCnt = 0;
			writeDataCnt = -PACKSIZE;
			bModifiedTrackData = false;
		}

		// トラックデータが切り替えられたら、送信データのカウンタを先頭に戻す
		if (bRequestChangeTrack && bTrackDataOk) {
			bRequestChangeTrack = false;
			//
			numReadDataCnt = g_TaskCmd.TrackData.Count;
			readDataCnt = 0;
			writeDataCnt = -PACKSIZE;
			bModifiedTrackData = false;
		}

		// DRIVE SELECT 信号入力
		g_Input.bDriveSelect = !getGpio(GP_FDD_CTRL_DRIVE_SELECT_0_N);
		if( bDriveSelectOld != g_Input.bDriveSelect) {
			bDriveSelectOld = g_Input.bDriveSelect;
			g_TaskCmd.bDriveSelect = g_Input.bDriveSelect;
		}

		// MOTOR ON信号
		g_Input.bMotorON = !getGpio(GP_FDD_CTRL_MOTOR_ON_0_N);

		// モーターが停止でディスクが挿入されていれば、モーター信号ONでモーター回転を開始する
		if ( bInsertDisk && !bMotor && g_Input.bMotorON ){
			bMotor = true;
			outputIndexlCnt.Start(4000);			// 4msの間INDEXをACTIVEにする
			delayReadySignalCnt.Start(400);		// 400ms後にREADY信号をACTIVEに。
		}

		// ディスクが抜かれた、もしくは、モーター信号OFFでモーター停止、READY信号もNON_ACTIVEに。
		if ( !bInsertDisk || !g_Input.bMotorON ){
			delayReadySignalCnt.Cancel();
			bMotor = false;
		}

		// STEP、DIRECTION信号をもとに、トラック番号を更新する		
		if (tarckNoOld != trackNo || sideNoOld != sideNo) {
			if( !bRequestChangeTrack ) {
				tarckNoOld = trackNo, sideNoOld = sideNo;
				outputIndexlCnt.Cancel();			// index信号をNON_ACTIVE固定にする
				bTrackDataOk = false;
				// Core0に指定トラック、面のデータを準備するよう要求する
				requestTrackData(&g_TaskCmd, trackNo, sideNo, bModifiedTrackData);
				bRequestChangeTrack = true;
			}
		}

		// READ DATA信号の出力
		uint32_t temp = 0x00;
		for( int t = 0 ; t < PACKSIZE; ++t) {
			const int s = 8 * (PACKSIZE-1-t);
			if (readDataCnt < numReadDataCnt){
				temp |= (uint32_t)g_TaskCmd.TrackData.Binary[readDataCnt++] << s;
			}
			else{
				temp |= (uint32_t)0xff << s;
			}
		}
		if (numReadDataCnt <= readDataCnt) {
			readDataCnt = 0;
			outputIndexlCnt.Start(4000);	// 規格上は、1-8[ms]
		}

		if ( !(g_Input.bDriveSelect && bMotor && bInsertDisk && bTrackDataOk) ) {
			temp = 0xffffffff;
		}
		pio_sm_put_blocking(pio, smReadData, temp);
		
		// WRITE GATE/WRITE DATA信号を入力して、現在のトラックデータに合成する
		const uint32_t wgGate = pio_sm_get_blocking(pio, smWriteGate);
		const uint32_t wgData = pio_sm_get_blocking(pio, smWriteData);
		for( int t = 0; t < PACKSIZE; ++t) {
			uint8_t gateDt = (wgGate>>((PACKSIZE-1-t)*8)) & 0xff;
			uint8_t dataDt = (wgData>>((PACKSIZE-1-t)*8)) & 0xff;
			if (0 <= writeDataCnt && writeDataCnt < numReadDataCnt) {
				uint8_t *p = &g_TaskCmd.TrackData.Binary[writeDataCnt];
				const uint8_t v = (*p & gateDt) | (dataDt & (gateDt^0xff));
				// 代入実行の判断を直前に行うのは条件分岐による処理時間のムラを最小にするため
				if (g_Input.bDriveSelect && g_Output.bReady && bTrackDataOk && !g_Output.bWriteProtect && !bSdWp) {
					*p = v;
					bModifiedTrackData |= (gateDt != 0xff) ? true : false;
				}
			}
			++writeDataCnt;
		}
		if (numReadDataCnt <= writeDataCnt) {
			writeDataCnt = 0;
		}

		// 信号出力
		g_Output.bTrack00		= (trackNo==0) ? ACTIVE : NON_ACTIVE;
		g_Output.bReady			= (bMotor && bInsertDisk && delayReadySignalCnt.IsTimeout()) ? ACTIVE : NON_ACTIVE;
		g_Output.bIndex			= (bMotor && bInsertDisk && bTrackDataOk && afterStepCnt.IsTimeout() && outputIndexlCnt.IsEffective() && !outputIndexlCnt.IsTimeout()) ? ACTIVE : NON_ACTIVE;
		g_Output.bWriteProtect	= (bSdWp) ? ACTIVE : NON_ACTIVE;
		if ( g_Input.bDriveSelect ) {
			setGpio(GP_FDD_STS_TRACK00_N,		!g_Output.bTrack00);
			setGpio(GP_FDD_STS_READY_N,			!g_Output.bReady);
			setGpio(GP_FDD_STS_INDEX_N,			!g_Output.bIndex);
			setGpio(GP_FDD_STS_WRITE_PROTECT_N, !g_Output.bWriteProtect);
			setGpio(GP_FDD_STS_DISK_CHANGE_N,	!g_Output.bDiskChange);
		}
		else{
			// DRIVESELECTがOFF場合、全ての出力はOFFとする
			setGpio(GP_FDD_STS_TRACK00_N,		SIGNAL_H);
			setGpio(GP_FDD_STS_DISK_CHANGE_N,	SIGNAL_H);
			setGpio(GP_FDD_STS_WRITE_PROTECT_N, SIGNAL_H);
			setGpio(GP_FDD_STS_READY_N,			SIGNAL_H);
			setGpio(GP_FDD_STS_INDEX_N,			SIGNAL_H);
		}
	}
	return;
}

