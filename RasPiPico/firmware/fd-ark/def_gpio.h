/**
 * FD-ARK (RaspberryPiPico firmware)
 * Copyright (c) 2023 Harumakkin.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "hardware/i2c.h"
#include "hardware/spi.h"


// --- Fdd Crisatl R2 ----

const uint32_t GP_FDD_CTRL_MODE_SELECT_N	= 6;	// 動作モード
													// 		2DDが挿入されているときは、この信号は意味をなさない（1.0Mモードで動作する）
													// 		2HDが挿入されているときは、L=1.6Mモード、H=2.0MBモードで動作する
const uint32_t GP_FDD_CTRL_DRIVE_SELECT_0_N	= 7;	// 
const uint32_t GP_FDD_CTRL_MOTOR_ON_0_N		= 8;	//
const uint32_t GP_FDD_CTRL_DIRECTION_N		= 9;	// ヘッドの移動方向(L=内側へ、H=外側へ)
const uint32_t GP_FDD_CTRL_STEP_N			= 10;	// １パルスでヘッドを別のトラックへ移動させる
const uint32_t GP_FDD_CTRL_WRITE_DATA_N		= 11;	// 書き込みデータ
const uint32_t GP_FDD_CTRL_WRITE_GATEA_N	= 12;	// 書き込みの指示
const uint32_t GP_FDD_CTRL_SIDE1_N			= 13;	// 書き込み面(L=Side:1, H=Side:0)

const uint32_t GP_FDD_STS_READY_N			= 28;
const uint32_t GP_FDD_STS_INDEX_N			= 27;
const uint32_t GP_FDD_STS_READ_DATA_N		= 26;
const uint32_t GP_FDD_STS_TRACK00_N			= 22;	// L=トラック0(最外周)にいる。
const uint32_t GP_FDD_STS_WRITE_PROTECT_N	= 21;
const uint32_t GP_FDD_STS_DISK_CHANGE_N		= 20;

const uint32_t GP_CTRL_1OE_N				= 14;	// 各種設定完了したら、L にして開通させる
const uint32_t GP_CTRL_2OE_N				= 15;	// 各種設定完了したら、L にして開通させる
const uint32_t GP_CTRL_WP_N					= 18;	// WRITE PROTECT SW, L=ON, H=OFF
const uint32_t GP_CTRL_2DIR_N				= 19;	// H固定とすること 2A -> 2B

const uint32_t GP_SDA_PIN 					= 16;	// GP16 = Pin.21 = SDA
const uint32_t GP_SCL_PIN 					= 17;	// GP17 = Pin.22 = SCL

const uint32_t GP_IO_SPI_CSN_PIN			= 1;	// CS for IO EXTENDER
const uint32_t GP_BUZZER_PIN				= 0;	// BUZZER

// SPI
#define SPIDEV spi0


