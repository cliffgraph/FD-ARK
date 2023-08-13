#pragma once
/**
 * FddPuser (RaspberryPiPico firmware)
 * Copyright (c) 2023 Harumakkin.
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型

namespace fddprsr
{

#pragma pack(push,1)
// uint16_t 以上のサイズはリトルエンディアン

// コマンドへの返信データ
struct RESPONSE
{
	uint8_t	cmd;		// CMD_RESPONSE
	uint8_t	recv_cmd;	// 受信したコマンド
	uint8_t	accept;		// 'Y':OK、'N':NG
	uint16_t status;	// bit0: 0=motor-off,1=motor-on
						// bit1: 0=ather than track00, 1=on track00
						// bit2: 0=side-0, 1=side-1
						// bit3: 0=write enable, 1=write protect
						// bit4-15: 0
	uint8_t track_no;
	uint8_t side_no;
};
// status bit.
static const uint16_t STS_MOTOR			= 0x0001;
static const uint16_t STS_TRACK00		= 0x0002;
static const uint16_t STS_SIDE			= 0x0004;
static const uint16_t STS_WRITEPROTECT	= 0x0008;

// CMD_WRITE_TRACK の返信データ
struct RESPONSE_WRITE_TRACK
{
	RESPONSE res;
	uint32_t size_mark;
	uint32_t recv_size;
};

// CMD_GET_FWVER の返信データ
struct RESPONSE_GET_FWVER
{
	RESPONSE res;
	uint8_t ver[4];		// ascii 4 chars.	ex)"1.0A"
};


// 読込トラックデータ送信データ
struct TRACKDATA
{
	uint8_t last_index8;	// 最終バイトのindex信号のビットマップ
	uint32_t count;			// data[] の格納サイズ
	uint8_t data[1];		// 先頭と末尾の計2バイトはindex信号、それ以外はREAD信号のビットマップ
};
inline size_t SIZEOF_HD_READ_TRACK()
{
	return sizeof(TRACKDATA) - sizeof(/*data[0]*/uint8_t);
}

// 読込トラックデータ送信データ
struct RESPONSE_READ_TRACK
{
	RESPONSE res;
	TRACKDATA track;
};
inline size_t SIZEOF_HD_RESPONSE_READ_TRACK()
{
	return sizeof(RESPONSE_READ_TRACK) - sizeof(/*data[0]*/uint8_t);
}
#pragma pack(pop)

static const uint8_t CMD_RESPONSE		= '0';
static const uint8_t CMD_MOTOR_ON		= '1';
static const uint8_t CMD_MOTOR_OFF		= '2';
static const uint8_t CMD_TRACK00		= '3';
static const uint8_t CMD_SIDE_0			= '4';
static const uint8_t CMD_SIDE_1			= '5';
static const uint8_t CMD_PREV_TRACK		= '6';
static const uint8_t CMD_NEXT_TRACK		= '7';
static const uint8_t CMD_READ_TRACK		= '8';
static const uint8_t CMD_WRITE_TRACK	= '9';
static const uint8_t CMD_READ_TRACK_TO_FS = 'S';
static const uint8_t CMD_WRITE_TRACK_FROM_FS = 'L';
static const uint8_t CMD_GET_FWVER		= 'v';	// firmware version.

} // namespace fddprsr
