/**
 * FD-ARK (RaspberryPiPico firmware)
 * Copyright (c) 2023 Harumakkin.
 * SPDX-License-Identifier: MIT
 */
// https://spdx.org/licenses/

#include <stdio.h>
#include <memory.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

 
#include "def_gpio.h"
#include "sdfat.h"
//
#include "global.h"
#include "ctrlfd_core0_task.h"
#include "ctrlfd_core1_task.h"
#include "gamen.h"
#include "timerobj.h"

static bool checkWriteProtect()
{
	gpio_put(GP_FDD_CTRL_DRIVE_SELECT_0_N, 0);
	sleep_ms(1);
	bool b = (gpio_get(GP_FDD_STS_WRITE_PROTECT_N)) ? false : true;
	gpio_put(GP_FDD_CTRL_DRIVE_SELECT_0_N, 1);
	return b;
}

static void requestFddControl(const TASKCMD cmd)
{
	sem_acquire_blocking(&g_TaskCmd.sem1);
	g_TaskCmd.cmds.Push(cmd);
	sem_release(&g_TaskCmd.sem1);
	return;
}

static const char *pSPACE = "                                ";

class GamenCtrlFD_FdSelect : public IGamen
{
private:
	enum class MEMOSTS
	{
		CHAKING,
		STORED,
		EMPTY,
	};
private:
	int encBinTemp;
	int curFdNo;
	int curFdNoOld;
	bool bSdWpOld;
	TimerObj delayDirCnt;
	TimerObj resetEnc;
	MEMOSTS memoSts, memoStsOld;
	int swOld;
	static const int SIZE_COMMENT = 32;
	char diskComment[SIZE_COMMENT+1] ;
private:
	GAMENINFO *pGamenInfo;

private:
	void encsFdNo(int encStep, int *pNo)
	{
		if( resetEnc.IsEffective() ) {
			if( resetEnc.IsTimeout() ) {
				resetEnc.Cancel();
				encBinTemp = 0;
			}
			else {
				return;
			}
		}

		// 選択FD番号の変更
		// エンコーダEC12E2420801は、１クリックで4増減する
		encBinTemp += encStep;
		if( 4 <= encBinTemp ){
			if( *pNo < MAX_FD_NO )
				++*pNo;
			resetEnc.Start(10);
		}
		else if( encBinTemp <= -4 ){
			if( MIN_FD_NO < *pNo )
				--*pNo;
			resetEnc.Start(10);
		}
		return;
	}

public:
	GamenCtrlFD_FdSelect()
	{
		encBinTemp = 0;
		curFdNo = MIN_FD_NO;
		curFdNoOld = -1;
		bSdWpOld = false;
		memoSts = MEMOSTS::CHAKING;
		memoStsOld = memoSts;
		swOld = 0;
		pGamenInfo = nullptr;
		return;
	}
	void Init(GAMENINFO *pG)
	{
		pGamenInfo = pG;
		return;
	}
	void Start()
	{
		curFdNoOld = -1;
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
		if ((swOld != sw) || (curFdNoOld != curFdNo) || (memoStsOld != memoSts) || (bSdWpOld != bSdWp)) {
			swOld = sw;
			curFdNoOld = curFdNo;
			memoStsOld = memoSts;
			bSdWpOld = bSdWp;
			const char *pMemoSts = "chking..";
			if (memoSts==MEMOSTS::EMPTY)
				pMemoSts = "(no file";
			else if (memoSts==MEMOSTS::STORED)
				pMemoSts = diskComment;
			char str[64+1];
			sprintf(str, "%c: %04d %-*s",
				(curFdNo<MAX_FD_NO)?'\x01':' ',		// x01 ↑
				curFdNo,								// DiskNo.
				8, pMemoSts);							// Comment

			char buff[32];
			memcpy(buff, pSPACE, 32);
			memcpy(&buff[0], &str[0], 16);
			buff[16] = (MIN_FD_NO<curFdNo)?'\x02':' ';		// x02 ↓
			if( bSdWp ) {
				memcpy(&buff[17], ": [   ] [\x03" "FD]", 13);
			}
			else {
				memcpy(&buff[17], ": [\x04" "FD] [\x03" "FD]", 13);
			}
			g_Lcd.Home();
			g_Lcd.PutStrings((uint8_t*)buff, 32);

			// [←FD]画面へ
			if (!(sw&ExtIO::SW_SW1)){
				if( bSdWp ) {
					beep = BEEPSOUND::ERROR;
				}
				else {
					pGamenInfo->SelectedFdNo = curFdNo;
					pGamenInfo->GamenNo = GAMENINFO::GAMENNO::CTRLFD_READ_FD;
					beep = BEEPSOUND::CLICK;
				}
			} 
			// [→FD]画面へ
			else if (!(sw&ExtIO::SW_SW2)){
				if (memoSts==MEMOSTS::EMPTY || checkWriteProtect()) {
					beep = BEEPSOUND::ERROR;
				}
				else{
					pGamenInfo->SelectedFdNo = curFdNo;
					pGamenInfo->GamenNo = GAMENINFO::GAMENNO::CTRLFD_WRITE_FD;
					beep = BEEPSOUND::CLICK;
				}
			} 
		}

		// memo.txtを読み込む -> diskComment, memoSts
		if (delayDirCnt.IsTimeout()) {
			delayDirCnt.Cancel();
			g_ExtIo.SetLED(ExtIO::LED_SD, ExtIO::LEDON);
			const bool b = ReadStoredMark(curFdNo, SIZE_COMMENT+1, diskComment);
			memoSts = (b) ? MEMOSTS::STORED : MEMOSTS::EMPTY;
			sleep_ms(5);
			g_ExtIo.SetLED(ExtIO::LED_SD, false/*点灯*/);
		}

		// 操作音
		if (beep != BEEPSOUND::NONE)
			BeepSound(beep);
		return;
	}
};

class GamenCtrlFD_ReadFDD : public IGamen
{
private:
	GAMENINFO *pGamenInfo;
	int swOld, sequenceNo;
public:
	GamenCtrlFD_ReadFDD()
	{
		pGamenInfo = nullptr;
		return;
	}
	void Init(GAMENINFO *pG)
	{
		pGamenInfo = pG;
		return;
	}
	void Start()
	{
		char str[32+1];
		sprintf(str,
			"%04d Read FDD?  " " [YES]  [NO]    ",
			pGamenInfo->SelectedFdNo);
		g_Lcd.Home();
		g_Lcd.PutStrings((uint8_t*)str, 32);
		swOld = 0;
		sequenceNo = 0;
		sleep_ms(300);
		// キーが押されていたら解放されるまでここで待つ。
		g_ExtIo.WaitFreeInput(ExtIO::SW_SW1|ExtIO::SW_SW2|ExtIO::SW_SW3);
		return;
	}
	void Main()
	{
		uint8_t sw = 0;
		g_ExtIo.GetInput(&sw, nullptr);

		BEEPSOUND beep = BEEPSOUND::NONE;

		// 実行の問い合わせ
		if (sequenceNo == 0 ) {
			if (swOld != sw) {
				swOld = sw;
				// [No], Cancel
				if ( !(sw&(ExtIO::SW_SW2)) || !(sw&(ExtIO::SW_SW3)) ) {
					pGamenInfo->GamenNo = GAMENINFO::GAMENNO::CTRLFD_TOP_MENU;	// 選択画面へ戻る
					beep = BEEPSOUND::CLICK;
				}
				// [YES]
				if ( !(sw&ExtIO::SW_SW1)) {
					sequenceNo = 1;
					beep = BEEPSOUND::CLICK;
				}
				//
			}
		}
		// 読み込み実行
		else if (sequenceNo == 1) {
			requestFddControl(TASKCMD::FDDCTRL_MOTOR_ON);
			sleep_ms(500);
			requestFddControl(TASKCMD::FDDCTRL_MOVE_TO_TRACK00);
			sleep_ms(1000);
			for (int tr = MIN_TRACK_NO; tr <= MAX_TRACK_NO; ++tr) {
				for (int side = 0; side < 2; ++side) {

					requestFddControl(
						(side==0)
						? TASKCMD::FDDCTRL_CHANGE_TO_SIDE_0 
						: TASKCMD::FDDCTRL_CHANGE_TO_SIDE_1);

					// LCD表示
					char str[32+1];
					sprintf(str,
						"%04d Reading FDD" "TRACK=%02d-%d      ",
						pGamenInfo->SelectedFdNo, tr, side);
					g_Lcd.Home();
					g_Lcd.PutStrings((uint8_t*)str, 32);

					// 読み込みを指示する
					sem_acquire_blocking(&g_TaskCmd.sem2);
					g_TaskCmd.bTrackDataOk = false;;
					sem_release(&g_TaskCmd.sem2);
					requestFddControl(TASKCMD::FDDCTRL_START_READ_TRACK);
					g_ExtIo.SetLED(ExtIO::LED_FD, ExtIO::LEDON);

					// 読み込み完了を待つ
					bool bWait = true;
					while(bWait) {
						sleep_ms(1);
						sem_acquire_blocking(&g_TaskCmd.sem2);
						if (g_TaskCmd.bTrackDataOk)
							bWait = false;
						sem_release(&g_TaskCmd.sem2);
					}
					g_ExtIo.SetLED(ExtIO::LED_FD, ExtIO::LEDOFF);

					// SDへ保存する
					g_ExtIo.SetLED(ExtIO::LED_SD, ExtIO::LEDON);
					MakeDirWork(pGamenInfo->SelectedFdNo);
					memcpy(g_TaskCmd.TrackData.Signature, "FD-ARK", 6);
					g_TaskCmd.TrackData.Code[0] = 'a';
					g_TaskCmd.TrackData.Code[1] = 0x00;
					WriteFileTrackData(
						pGamenInfo->SelectedFdNo, tr, side,
						reinterpret_cast<uint8_t*>(&g_TaskCmd.TrackData),
						sizeof(g_TaskCmd.TrackData) - sizeof(g_TaskCmd.TrackData.Binary) + g_TaskCmd.TrackData.Count);
					g_ExtIo.SetLED(ExtIO::LED_SD, ExtIO::LEDOFF);
				}
				sleep_ms(300);
				requestFddControl(TASKCMD::FDDCTRL_NEXT_TRACK);
			}
			if (!CheckExitStoredMark(pGamenInfo->SelectedFdNo)) {
				WriteStoredMark(pGamenInfo->SelectedFdNo);
			}
			sequenceNo = 2;
		}
		// 読み込み完了メッセージ表示
		else if (sequenceNo == 2) {
			char str[32+1];
			sprintf(str,
				"%04d Read FDD   " "Complete!       ",
				pGamenInfo->SelectedFdNo);
			g_Lcd.Home();
			g_Lcd.PutStrings((uint8_t*)str, 32);
			sequenceNo = 3;
			beep = BEEPSOUND::READWRITE_COMPLETE;
			sleep_ms(1000);
			requestFddControl(TASKCMD::FDDCTRL_MOTOR_OFF);
		}
		// 読み込み完了メッセージ消去待ち
		else if (sequenceNo == 3) {
			if (swOld != sw) {
				swOld = sw;
				if (!(sw&ExtIO::SW_SW3)) {
					pGamenInfo->GamenNo = GAMENINFO::GAMENNO::CTRLFD_TOP_MENU;	// Disk No 選択画面へ戻る
					beep = BEEPSOUND::CLICK;
				}
			}
		}
		// 操作音
		if (beep != BEEPSOUND::NONE)
			BeepSound(beep);
		return;
	}
};


class GamenCtrlFD_WriteFDD : public IGamen
{
private:
	GAMENINFO *pGamenInfo;
	int swOld, sequenceNo;
public:
	GamenCtrlFD_WriteFDD()
	{
		pGamenInfo = nullptr;
		return;
	}
	void Init(GAMENINFO *pG)
	{
		pGamenInfo = pG;
		return;
	}
	void Start()
	{
		char str[32+1];
		sprintf(str,
			"%04d Write FDD? " " [YES]  [NO]    ",
			pGamenInfo->SelectedFdNo);
		g_Lcd.Home();
		g_Lcd.PutStrings((uint8_t*)str, 32);
		swOld = 0;
		sequenceNo = 0;
		sleep_ms(300);
		// キーが押されていたら解放されるまでここで待つ。
		g_ExtIo.WaitFreeInput(ExtIO::SW_SW1|ExtIO::SW_SW2|ExtIO::SW_SW3);
		return;
	}
	void Main()
	{
		uint8_t sw = 0;
		g_ExtIo.GetInput(&sw, nullptr);

		BEEPSOUND beep = BEEPSOUND::NONE;

		// 実行の問い合わせ
		if (sequenceNo == 0 ) {
			if (swOld != sw) {
				swOld = sw;
				// [No], Cancel
				if ( !(sw&(ExtIO::SW_SW2)) || !(sw&(ExtIO::SW_SW3)) ) {
					pGamenInfo->GamenNo = GAMENINFO::GAMENNO::CTRLFD_TOP_MENU;	// 選択画面へ戻る
					beep = BEEPSOUND::CLICK;
				}
				// [YES]
				if ( !(sw&ExtIO::SW_SW1)) {
					sequenceNo = 1;
					beep = BEEPSOUND::CLICK;
				}
				//
			}
		}
		// Write FDD 実行
		else if (sequenceNo == 1) {
			requestFddControl(TASKCMD::FDDCTRL_MOTOR_ON);
			sleep_ms(500);
			requestFddControl(TASKCMD::FDDCTRL_MOVE_TO_TRACK00);
			sleep_ms(1000);
			for (int tr = MIN_TRACK_NO; tr <= MAX_TRACK_NO; ++tr) {
				for (int side = 0; side < 2; ++side) {

					requestFddControl(
						(side==0)
						? TASKCMD::FDDCTRL_CHANGE_TO_SIDE_0 
						: TASKCMD::FDDCTRL_CHANGE_TO_SIDE_1);

					// LCD表示
					char str[32+1];
					sprintf(str,
						"%04d Writing FDD" "TRACK=%02d-%d      ",
						pGamenInfo->SelectedFdNo, tr, side);
					g_Lcd.Home();
					g_Lcd.PutStrings((uint8_t*)str, 32);

					// SDから読み込む
					g_ExtIo.SetLED(ExtIO::LED_SD, ExtIO::LEDON);
					ReadFileTrackData(
						pGamenInfo->SelectedFdNo, tr, side,
						sizeof(g_TaskCmd.TrackData),
						reinterpret_cast<uint8_t*>(&g_TaskCmd.TrackData));
					g_ExtIo.SetLED(ExtIO::LED_SD, ExtIO::LEDOFF);

					// FDD への書き込みを指示する
					sem_acquire_blocking(&g_TaskCmd.sem2);
					g_TaskCmd.bTrackDataOk = true;
					sem_release(&g_TaskCmd.sem2);
					requestFddControl(TASKCMD::FDDCTRL_START_WRITE_TRACK);
					g_ExtIo.SetLED(ExtIO::LED_FD, ExtIO::LEDON);

					// 書き込み完了を待つ
					bool bWait = true;
					while(bWait) {
						sleep_ms(1);
						sem_acquire_blocking(&g_TaskCmd.sem2);
						if (!g_TaskCmd.bTrackDataOk)
							bWait = false;
						sem_release(&g_TaskCmd.sem2);
					}
					g_ExtIo.SetLED(ExtIO::LED_FD, ExtIO::LEDOFF);

				}
				sleep_ms(300);
				requestFddControl(TASKCMD::FDDCTRL_NEXT_TRACK);
			}
			sequenceNo = 2;
		}
		// 読み込み完了メッセージ表示
		else if (sequenceNo == 2) {
			char str[32+1];
			sprintf(str,
				"%04d Write FDD  " "Complete!       ",
				pGamenInfo->SelectedFdNo);
			g_Lcd.Home();
			g_Lcd.PutStrings((uint8_t*)str, 32);
			sequenceNo = 3;
			beep = BEEPSOUND::READWRITE_COMPLETE;
			sleep_ms(1000);
			requestFddControl(TASKCMD::FDDCTRL_MOTOR_OFF);
		}
		// 読み込み完了メッセージ消去待ち
		else if (sequenceNo == 3) {
			if (swOld != sw) {
				swOld = sw;
				if (!(sw&ExtIO::SW_SW3)) {
					pGamenInfo->GamenNo = GAMENINFO::GAMENNO::CTRLFD_TOP_MENU;	// Disk No 選択画面へ戻る
					beep = BEEPSOUND::CLICK;
				}
			}
		}
		// 操作音
		if (beep != BEEPSOUND::NONE)
			BeepSound(beep);
		return;
	}
};

/** core2 : UI, SD-Access
*/
void CtrlFD_Core0Main()
{
	g_ExtIo.SetLED(ExtIO::LED_SD, ExtIO::LEDOFF);
	g_ExtIo.SetLED(ExtIO::LED_FD, ExtIO::LEDOFF);
	GAMENINFO gmnInfo;
	gmnInfo.GamenNo = GAMENINFO::GAMENNO::CTRLFD_TOP_MENU;
	
	IGamen *gmns[] = {
		new GamenCtrlFD_FdSelect(), 
		new GamenCtrlFD_ReadFDD(),
		new GamenCtrlFD_WriteFDD(),
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




