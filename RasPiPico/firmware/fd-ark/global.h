#pragma once
#include "pico/multicore.h"
#include "lcd.h"
#include "extio.h"
#include "trackdata.h"

enum class TASKCMD
{
	NONE = 0,
	FDDCTRL_MOTOR_ON,				// FDD モーターを回転させる
	FDDCTRL_MOTOR_OFF,				// FDD モーターを停止させる
	FDDCTRL_MOVE_TO_TRACK00,		// FDD のヘッドをTRAC0へ移動させる
	FDDCTRL_CHANGE_TO_SIDE_0,		// FDD のアクセス面を SIDE 0 に変更する
	FDDCTRL_CHANGE_TO_SIDE_1,		// FDD のアクセス面を SIDE 1 に変更する
	FDDCTRL_PREV_TRACK,				// FDD のヘッドを前（外側）トラックに移動させる
	FDDCTRL_NEXT_TRACK,				// FDD のヘッドを次（内側）トラックに移動させる
	FDDCTRL_START_READ_TRACK,		// 現在のSIDE、トラック位置で、トラックの読み込みを開始する
	FDDCTRL_START_WRITE_TRACK,		// 現在のSIDE、トラック位置で、トラックの書き込み開始する
};

const int MIN_FD_NO = 1;
const int MAX_FD_NO = 9999;
const int MIN_TRACK_NO = 0;
//const int MAX_TRACK_NO = 81;
const int MAX_TRACK_NO = 80;


struct TASKCMDCUE
{
	static const int MAX_CUE_SZ = 10;
	int	readIndex, writeIndex;
	int num;
	TASKCMD	cmd[MAX_CUE_SZ];
	TASKCMDCUE() : readIndex(0), writeIndex(0), num(0){ return; }
	void Push(const TASKCMD	c)
	{
		if (MAX_CUE_SZ <= num) {
			return;
		}
		cmd[writeIndex] = c;
		if ( ++writeIndex == MAX_CUE_SZ)
			writeIndex = 0;
		++num;
		return;
	}
	bool Pop(TASKCMD *pCmd)
	{
		if (num == 0 ) {
			return false;
		}
		*pCmd = cmd[readIndex];
		if ( ++readIndex == MAX_CUE_SZ)
			readIndex = 0;
		--num;
		return true;
	}
};

struct TASKCMD_PARAMS
{
	// for CtrlFD mode
	semaphore_t sem1;
	TASKCMDCUE	cmds;

	// CtrlFD/Emurator mode
	semaphore_t sem2;
	bool bInsertDisk;
	bool bTrackDataOk;
	uint8_t DiskChangeCnt;
	bool bSdCardWriteProtect;

	// for Emurator mode
	semaphore_t sem3;
	uint8_t sem3cnt;
	int RequestDataTrackNo;
	int RequestDataSideNo;
	bool bModifiedTrackData;
	//
	bool bDriveSelect;
	//
	TRACKDATA TrackData;

	//
	TASKCMD_PARAMS()
	{
	    sem_init(&sem1, 1, 1);
	    sem_init(&sem2, 1, 1);
	    sem_init(&sem3, 1, 1);
		TrackData.Count = 0;
		TrackData.LastIndex8 = 0x00;
		bInsertDisk = false;
		bTrackDataOk = false;
		DiskChangeCnt = 0;
		bSdCardWriteProtect = false;
		//
		sem3cnt = 0;
		RequestDataTrackNo = -1;
		RequestDataSideNo = -1;
		bModifiedTrackData = false;
		//
		bDriveSelect = false;
	}
};


enum class BEEPSOUND
{
	NONE,
	CLICK,
	ERROR,
	READWRITE_COMPLETE,
	INSERT_DISK,
	EJECT_DISK,
};

extern TASKCMD_PARAMS g_TaskCmd;
extern LcdAQM1602Y g_Lcd;
extern ExtIO g_ExtIo;
extern bool g_bEmuMode;

void BeepSound(const BEEPSOUND bs);
uint32_t GetAbsoluteTimeMs();