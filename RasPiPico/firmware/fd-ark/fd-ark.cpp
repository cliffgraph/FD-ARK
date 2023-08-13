/**
 * FD-ARK (RaspberryPiPico firmware)
 * Copyright (c) 2023 Harumakkin.
 * SPDX-License-Identifier: MIT
 */

// https://spdx.org/licenses/

#include <stdio.h>
#include "hardware/i2c.h"
#include "pico/multicore.h"
#include "ff.h"
#include "diskio.h"
#include "def_gpio.h"
#include "global.h"
#include "ctrlfd_core0_task.h"
#include "ctrlfd_core1_task.h"
#include "emu_core0_task.h"
#include "emu_core1_task.h"
#include "title.h"

struct INITGPTABLE {
	int gpno;
	int direction;
	bool bPullup;
	int	init_value;
};

static const INITGPTABLE g_GpioInitTableCommon[] = {
	{ GP_IO_SPI_CSN_PIN,			GPIO_OUT,	false, 1, },	// SPI CS-pin for I/O-Expander
	{ GP_BUZZER_PIN,				GPIO_OUT,	true,  0, }, 	// 無音時は0にしてBZに電圧がかからないようにしておく
	{ -1,							0,			false, 0, },
};


// FD 読み書きモード用のGPIO設定
static const INITGPTABLE g_CtrlFD_GpioTable[] = {
	// control-pin for FDD.
	{ GP_FDD_CTRL_MODE_SELECT_N,	GPIO_OUT,	false, 0, },
	{ GP_FDD_CTRL_DRIVE_SELECT_0_N,	GPIO_OUT,	false, 1, },
	{ GP_FDD_CTRL_MOTOR_ON_0_N,		GPIO_OUT,	false, 1, },
	{ GP_FDD_CTRL_DIRECTION_N,		GPIO_OUT,	false, 1, },
	{ GP_FDD_CTRL_STEP_N,			GPIO_OUT,	false, 1, },
	{ GP_FDD_CTRL_SIDE1_N,			GPIO_OUT,	false, 1, },
	{ GP_FDD_CTRL_WRITE_DATA_N,		GPIO_OUT,	false, 1, },
	{ GP_FDD_CTRL_WRITE_GATEA_N,	GPIO_OUT,	false, 1, },
	{ GP_FDD_STS_INDEX_N,			GPIO_IN,	false, 0, },
	{ GP_FDD_STS_READ_DATA_N,		GPIO_IN,	false, 0, },
	{ GP_FDD_STS_READY_N,			GPIO_IN,	false, 0, },
	{ GP_FDD_STS_TRACK00_N,			GPIO_IN,	false, 0, },
	{ GP_FDD_STS_WRITE_PROTECT_N,	GPIO_IN,	false, 0, },
	{ GP_FDD_STS_DISK_CHANGE_N,		GPIO_IN,	false, 0, },
	{ GP_CTRL_WP_N,					GPIO_IN,	true,  0, },
	// control-pin for level shifter.
	{ GP_CTRL_2DIR_N,				GPIO_OUT,	false, 1, },	// PICO 2A -> 2B FDD
	{ GP_CTRL_1OE_N,				GPIO_OUT,	false, 0, },	// ---- 開通
	{ GP_CTRL_2OE_N,				GPIO_OUT,	false, 0, },	// ---- 開通	
	// eot
	{ -1,							0,			false, 0, },
};

// FDDエミュレーター用のGPIO設定
static const INITGPTABLE g_Emu_GpioTable[] = {
	// control-pin for FDD.
	{ GP_FDD_CTRL_MODE_SELECT_N,	GPIO_IN,	false, 0, },
	{ GP_FDD_CTRL_DRIVE_SELECT_0_N,	GPIO_IN,	false, 0, },
	{ GP_FDD_CTRL_MOTOR_ON_0_N,		GPIO_IN,	false, 0, },
	{ GP_FDD_CTRL_DIRECTION_N,		GPIO_IN,	false, 0, },
	{ GP_FDD_CTRL_STEP_N,			GPIO_IN,	false, 0, },
	{ GP_FDD_CTRL_SIDE1_N,			GPIO_IN,	false, 0, },
	{ GP_FDD_CTRL_WRITE_DATA_N,		GPIO_IN,	false, 0, },
	{ GP_FDD_CTRL_WRITE_GATEA_N,	GPIO_IN,	false, 0, },
	{ GP_FDD_STS_INDEX_N,			GPIO_OUT,	false, 1, },
	{ GP_FDD_STS_READ_DATA_N,		GPIO_OUT,	false, 1, },
	{ GP_FDD_STS_READY_N,			GPIO_OUT,	false, 1, },
	{ GP_FDD_STS_TRACK00_N,			GPIO_OUT,	false, 1, },
	{ GP_FDD_STS_WRITE_PROTECT_N,	GPIO_OUT,	false, 1, },
	{ GP_FDD_STS_DISK_CHANGE_N,		GPIO_OUT,	false, 1, },
	{ GP_CTRL_WP_N,					GPIO_IN,	true,  0, },
	// control-pin for level shifter.
	{ GP_CTRL_2DIR_N,				GPIO_OUT,	false, 0, },	// PICO 2A <- 2B FDD
	{ GP_CTRL_1OE_N,				GPIO_OUT,	false, 1, },	// -||- 不通
	{ GP_CTRL_2OE_N,				GPIO_OUT,	false, 0, },	// ---- 開通	
	// eot
	{ -1,							0,			false, 0, },
};

static void setupGpio(const INITGPTABLE pTable[] )
{
	for (int t = 0; pTable[t].gpno != -1; ++t) {
		const int no = pTable[t].gpno;
		gpio_init(no);
		gpio_set_dir(no, pTable[t].direction);
		if( pTable[t].direction == GPIO_OUT)
			gpio_put(no, pTable[t].init_value);
		if (pTable[t].bPullup)
			gpio_pull_up(no);
	}
	return;
}

static bool timerproc_fot_ff(repeating_timer_t *rt)
{
	disk_timerproc();
	return true;
}

#include "pico/stdlib.h"
int main()
{
	stdio_init_all();
	setupGpio(g_GpioInitTableCommon);

	static repeating_timer_t tim;
	add_repeating_timer_ms (1/*ms*/, timerproc_fot_ff, nullptr, &tim);

	disk_initialize(0);

	sleep_ms(200);

	// Inirialize LCD
	g_Lcd.Initialize(i2c0, 0x3E, GP_SDA_PIN, GP_SCL_PIN);
	g_Lcd.SendCG();
	g_Lcd.Clear();

	// Display title
	g_Lcd.PutStrings(reinterpret_cast<const uint8_t*>(pTITLENAME), TITLENAME_SIZE);

	// Extender-I/O
	g_ExtIo.Initialize(GP_IO_SPI_CSN_PIN);

	// Determine the Mode by mode-switch
	const uint8_t portDt = g_ExtIo.GetPortRaw();
	g_bEmuMode = (portDt & ExtIO::SW_MODE) ? true : false;
	const char *pMode = (g_bEmuMode) ? "FD Emurator Mode" : "FDD Control Mode";
	g_Lcd.PutStrings(reinterpret_cast<const uint8_t*>(pMode), 16);
	sleep_ms(2000);

	// 
	if (g_bEmuMode) {
		setupGpio(g_Emu_GpioTable);
		multicore_launch_core1(Emu_Core1Main);
		Emu_Core0Main();
	} else {
		setupGpio(g_CtrlFD_GpioTable);
		multicore_launch_core1(CtrlFD_Core1Main);
		CtrlFD_Core0Main();
	}
	
	return 0;
}
