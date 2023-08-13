
/**
 * FD-ARK (RaspberryPiPico firmware)
 * Copyright (c) 2023 Harumakkin.
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <memory.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
 
#include "def_gpio.h"
#include "sdfat.h"
#include "global.h"
#include "gamen.h"
#include "timerobj.h"

static const char *pSPACE = "                                ";

class GamenEmuTopMenu : public IGamen
{
private:
	enum class MEMOSTS
	{
		CHAKING,
		STORED,
		EMPTY,
	};
private:
	int curTrackNo;
	int curSideNo;
	int encBinTemp;
	int curFdNo;
	int curFdNoOld;
	int selectdNoOld;
	bool bSdWpOld;
	TimerObj resetEnc;
	TimerObj delayDirCnt;
	MEMOSTS memoSts, memoStsOld;
	int swOld;
	static const int SIZE_COMMENT = 32;
	char diskComment[SIZE_COMMENT+1] ;
	int reqTrackNoFromCore1Old;
	int reqSideNoFromCore1Old;
	int reqTrackNo;
	int reqSideNo;
	bool bForceRead;
	int curTrackNoDispOld;
	int curSideNoDispOld;
	bool bDriveSelectOld;
	int sem3cntOld;

private:
	GAMENINFO *pGamenInfo;

private:
	void encsFdNo(int enc, int *pNo)
	{
		// 選択FD番号の変更
		encBinTemp += enc;
		if (4 <= encBinTemp){
			if (*pNo < MAX_FD_NO )
				++*pNo;
			encBinTemp = 0;
			resetEnc.Start(100);
		}
		else if( encBinTemp <= -3) {
			if (MIN_FD_NO < *pNo)
				--*pNo;
			encBinTemp = 0;
			resetEnc.Start(100);
		}
		if(	resetEnc.IsTimeout() ) {
			resetEnc.Cancel();
			encBinTemp = 0;
		}
		return;
	}

public:
	GamenEmuTopMenu()
	{
		curTrackNo = -1;
		curSideNo = -1;
		encBinTemp = 0;
		curFdNo = MIN_FD_NO;
		curFdNoOld = -1;
		selectdNoOld = -1;
		bSdWpOld = false;
		memoSts = MEMOSTS::CHAKING;
		memoStsOld = memoSts;
		swOld = 0;
		reqTrackNoFromCore1Old = -1;
		reqSideNoFromCore1Old = -1;
		reqTrackNo = 0;
		reqSideNo = 0;
		curTrackNoDispOld = -1;
		curSideNoDispOld = -1;
		bDriveSelectOld = false;
		sem3cntOld = -1;
		pGamenInfo = nullptr;
		bForceRead = false;
		return;
	}
	void Init(GAMENINFO *pG)
	{
		pGamenInfo = pG;
		sem_acquire_blocking(&g_TaskCmd.sem2);
		g_TaskCmd.bInsertDisk = false;
		g_TaskCmd.bTrackDataOk = false;
		g_TaskCmd.DiskChangeCnt = 0;
		g_TaskCmd.bSdCardWriteProtect = false;;
		sem_release(&g_TaskCmd.sem2);
		return;
	}
	void Start()
	{
		sem_acquire_blocking(&g_TaskCmd.sem3);
		sem3cntOld = g_TaskCmd.sem3cnt;
		sem_release(&g_TaskCmd.sem3);
		sleep_ms(300);
		// キーが押されていたら解放されるまでここで待つ。
		g_ExtIo.WaitFreeInput(ExtIO::SW_SW1|ExtIO::SW_SW2|ExtIO::SW_SW3);
		return;
	}
	void Main()
	{
		uint8_t sw = 0;
		int encStep = 0;
		g_ExtIo.GetInput(&sw, &encStep);
		const bool bSdWp = (gpio_get(GP_CTRL_WP_N)==0) ? true : false;

		// 選択FD番号の変更
		encsFdNo(encStep, &curFdNo);
		if (curFdNoOld != curFdNo){
			delayDirCnt.Start(300);
			memoSts = MEMOSTS::CHAKING;
		}

		BEEPSOUND beep = (curFdNoOld != curFdNo) ? BEEPSOUND::CLICK : BEEPSOUND::NONE;

		// LCD表示
		if ((curFdNoOld != curFdNo) || (memoStsOld != memoSts) ||
		(selectdNoOld != pGamenInfo->SelectedFdNo) || (curTrackNoDispOld != curTrackNo) ||
		(curSideNoDispOld != curSideNo)) {
			curFdNoOld = curFdNo;
			memoStsOld = memoSts;
			selectdNoOld = pGamenInfo->SelectedFdNo;
			curTrackNoDispOld = curTrackNo;
			curSideNoDispOld = curSideNo;
			const char *pMemoSts = "chking..";
			if (memoSts==MEMOSTS::EMPTY)
				pMemoSts = "(no file";
			else if (memoSts==MEMOSTS::STORED)
				pMemoSts = diskComment;
			char temp[SIZE_COMMENT+1];
			sprintf(temp, "%-32s", pMemoSts);
			char str[64+1];
			sprintf(str, "%c:%c%04d %-*s",
				(curFdNo<MAX_FD_NO)?'\x01':' ',				// x01 ↑
				(curFdNo==pGamenInfo->SelectedFdNo)?'*':' ',	// Select mark
				curFdNo,										// Disk.
				8, temp);										// Comment of line-1
			char buff[32];
			memcpy(buff, pSPACE, 32);
			memcpy(&buff[0], &str[0], 16);
			buff[16] = (MIN_FD_NO<curFdNo)?'\x02':' ';		// x02 ↓
			buff[17] = ':';
			memcpy(&buff[18], &temp[8], 14);					// Comment of line-2
			sprintf(temp, " %02d-%d", curTrackNo, curSideNo);
			memcpy(&buff[27], temp, 5);	
			g_Lcd.Home();
			g_Lcd.PutStrings((uint8_t*)buff, 32);
		}

		// SW操作
		if (swOld != sw) {
			swOld = sw;
			// 選択
			if (!(sw&ExtIO::SW_SW1)){
				if (memoSts == MEMOSTS::STORED) { 
					pGamenInfo->SelectedFdNo = curFdNo;
					beep = BEEPSOUND::INSERT_DISK;
					bForceRead = true;
					sem_acquire_blocking(&g_TaskCmd.sem2);
					g_TaskCmd.DiskChangeCnt++;
					sem_release(&g_TaskCmd.sem2);
				}else{
					beep = BEEPSOUND::ERROR;
				}
			} 
			// 選択ファイルへジャンプ
			else if ( !(sw&(ExtIO::SW_SW2)) ){
				if (pGamenInfo->SelectedFdNo == 0){
					beep = BEEPSOUND::ERROR;
				}else{
					curFdNo = pGamenInfo->SelectedFdNo;
				}
			} 
			// 選択解除(キャンセル）
			else if ( !(sw&(ExtIO::SW_SW3)) ){
				if (curFdNo == pGamenInfo->SelectedFdNo) {
					pGamenInfo->SelectedFdNo = 0;
					beep = BEEPSOUND::EJECT_DISK;
					sem_acquire_blocking(&g_TaskCmd.sem2);
					//g_TaskCmd.bTrackDataOk = false;
					g_TaskCmd.bInsertDisk = false;
					g_TaskCmd.DiskChangeCnt++;
					sem_release(&g_TaskCmd.sem2);
				}
			} 
		}

		if(bSdWpOld != bSdWp) {
			bSdWpOld = bSdWp;
			beep = (bSdWp) ? BEEPSOUND::INSERT_DISK : BEEPSOUND::EJECT_DISK;
			sem_acquire_blocking(&g_TaskCmd.sem2);
			g_TaskCmd.bSdCardWriteProtect = bSdWp;
			sem_release(&g_TaskCmd.sem2);
		}

		// memo.txtを読み込む -> diskComment, memoSts
		if (delayDirCnt.IsTimeout()) {
			delayDirCnt.Cancel();
			g_ExtIo.SetLED(ExtIO::LED_SD, ExtIO::LEDON);
			const bool b = ReadStoredMark(curFdNo, SIZE_COMMENT+1, diskComment);
			memoSts = (b) ? MEMOSTS::STORED : MEMOSTS::EMPTY;
			g_ExtIo.SetLED(ExtIO::LED_SD, ExtIO::LEDOFF);
		}

		// FDD Select lamp. etc
		const bool bDriveSelect = g_TaskCmd.bDriveSelect;
		if (bDriveSelectOld != bDriveSelect){
			bDriveSelectOld = bDriveSelect;
			g_ExtIo.SetLED(ExtIO::LED_FD, bDriveSelect);
		} 

		// 操作音
		if (beep != BEEPSOUND::NONE)
			BeepSound(beep);

		// トラックデータの切り替え要求を受ける
		sem_acquire_blocking(&g_TaskCmd.sem3);
		int reqTrackNoFromCore1 = g_TaskCmd.RequestDataTrackNo;
		int reqSideNoFromCore1 = g_TaskCmd.RequestDataSideNo;
		bool bModifiedTrackData = g_TaskCmd.bModifiedTrackData;
		int sem3cnt = g_TaskCmd.sem3cnt;
		sem_release(&g_TaskCmd.sem3);
		if ( sem3cntOld != sem3cnt ) {
			sem3cntOld = sem3cnt;
			if (!bModifiedTrackData && curTrackNo == reqTrackNoFromCore1 && curSideNo == reqSideNoFromCore1 ){
				sem_acquire_blocking(&g_TaskCmd.sem2);
				g_TaskCmd.bTrackDataOk = true;
				g_TaskCmd.bInsertDisk = (pGamenInfo->SelectedFdNo!=0)?true:false;
				sem_release(&g_TaskCmd.sem2);
			}
			else {
				reqTrackNoFromCore1Old = reqTrackNoFromCore1;
				reqSideNoFromCore1Old = reqSideNoFromCore1;
				reqTrackNo = reqTrackNoFromCore1;
				reqSideNo = reqSideNoFromCore1;
				bForceRead = true;
			}
		}

		// トラックデータのSDから読み込み
		if (curTrackNo != reqTrackNo || curSideNo != reqSideNo || bForceRead) {
			bForceRead = false;

			g_ExtIo.SetLED(ExtIO::LED_SD, ExtIO::LEDON);

			// トラックデータが書き換えられていた場合は、それを先に保存する
			if (bModifiedTrackData) {
				WriteFileTrackData(
					pGamenInfo->SelectedFdNo, curTrackNo, curSideNo,
					reinterpret_cast<uint8_t*>(&g_TaskCmd.TrackData),
					sizeof(g_TaskCmd.TrackData) - sizeof(g_TaskCmd.TrackData.Binary) + g_TaskCmd.TrackData.Count);
			}

			// トラックデータをSDから読み込む
			bool bRes = true;
			if (curTrackNo != reqTrackNo || curSideNo != reqSideNo ) {
				bRes = ReadFileTrackData(
					pGamenInfo->SelectedFdNo, reqTrackNo, reqSideNo,
					sizeof(g_TaskCmd.TrackData),
					reinterpret_cast<uint8_t*>(&g_TaskCmd.TrackData));
			}

			g_ExtIo.SetLED(ExtIO::LED_SD, ExtIO::LEDOFF);

			curTrackNo = reqTrackNo, curSideNo = reqSideNo;
			if (bRes) {
				sem_acquire_blocking(&g_TaskCmd.sem2);
				g_TaskCmd.bTrackDataOk = true;
				g_TaskCmd.bInsertDisk = (pGamenInfo->SelectedFdNo!=0)?true:false;
				sem_release(&g_TaskCmd.sem2);
				sem_acquire_blocking(&g_TaskCmd.sem3);
				g_TaskCmd.bModifiedTrackData = false;
				sem_release(&g_TaskCmd.sem3);
			}
		}
		return;
	}
};

void Emu_Core0Main()
{
	g_ExtIo.SetLED(ExtIO::LED_FD, ExtIO::LEDOFF);
	g_ExtIo.SetLED(ExtIO::LED_SD, ExtIO::LEDOFF);
	GAMENINFO gmnInfo;
	gmnInfo.GamenNo = GAMENINFO::GAMENNO::EMU_TOP_MENU;
	
	IGamen *gmns[] = {
		nullptr,
		nullptr,
		nullptr,
		new GamenEmuTopMenu(),
	};
	int num = (int)(sizeof(gmns)/sizeof(IGamen*));
	for (int t = 0; t < num; ++t){
		if (gmns[t] != nullptr ) {
			gmns[t]->Init(&gmnInfo);
		}
	}

	for(;;) {
		if (gmnInfo.GamenNoOld != gmnInfo.GamenNo) {
			gmnInfo.GamenNoOld = gmnInfo.GamenNo;
			gmns[(int)gmnInfo.GamenNo]->Start();
		}
		gmns[(int)gmnInfo.GamenNo]->Main();
	}
	return;
}