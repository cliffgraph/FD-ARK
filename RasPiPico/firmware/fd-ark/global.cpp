/**
 * FD-ARK (RaspberryPiPico firmware)
 * Copyright (c) 2023 Harumakkin.
 * SPDX-License-Identifier: MIT
 */

#include "global.h"

LcdAQM1602Y g_Lcd;
ExtIO g_ExtIo;
bool g_bEmuMode;
TASKCMD_PARAMS g_TaskCmd;

void BeepSound(const BEEPSOUND bs)
{
	// sleep_ms(), sleep_us() ではなく、
	// busy_wait_ms(), busy_wait_us(), を使用する。
	// sleep_を使用すると、他のコアで起動しているPIOの出力が揺らいでしまう。

	switch(bs)
	{
		case BEEPSOUND::CLICK:
			for( int t = 0; t < 50; ++t){
				gpio_put(GP_BUZZER_PIN, 0);
				busy_wait_us(200);
				gpio_put(GP_BUZZER_PIN, 1);
				busy_wait_us(200);
			}
			break;
		case BEEPSOUND::ERROR:
			for (int c = 0; c < 3; ++c) {
				for (int t = 0; t < 50; ++t) {
					gpio_put(GP_BUZZER_PIN, 0);
					busy_wait_us(200);
					gpio_put(GP_BUZZER_PIN, 1);
					busy_wait_us(100);
				}
				busy_wait_ms(120);
			}
			break;
		case BEEPSOUND::READWRITE_COMPLETE:
			for (int c = 0; c < 3; ++c) {
				for (int t = 0; t < 120; ++t) {
					gpio_put(GP_BUZZER_PIN, 0);
					busy_wait_us(190-10*c);
					gpio_put(GP_BUZZER_PIN, 1);
					busy_wait_us(90-10*c);
				}
				busy_wait_ms(120);
			}
			break;
		case BEEPSOUND::INSERT_DISK:
			for (int t = 0; t < 100; ++t) {
				gpio_put(GP_BUZZER_PIN, 0);
				busy_wait_us(200);
				gpio_put(GP_BUZZER_PIN, 1);
				busy_wait_us(200);
			}
			busy_wait_ms(200);
			for (int t = 0; t < 200; ++t) {
				gpio_put(GP_BUZZER_PIN, 0);
				busy_wait_us(90);
				gpio_put(GP_BUZZER_PIN, 1);
				busy_wait_us(90);
			}
			busy_wait_ms(120);
			break;
		case BEEPSOUND:: EJECT_DISK:
			for (int t = 0; t < 200; ++t) {
				gpio_put(GP_BUZZER_PIN, 0);
				busy_wait_us(90);
				gpio_put(GP_BUZZER_PIN, 1);
				busy_wait_us(90);
			}
			busy_wait_ms(200);
			for (int t = 0; t < 100; ++t) {
				gpio_put(GP_BUZZER_PIN, 0);
				busy_wait_us(200);
				gpio_put(GP_BUZZER_PIN, 1);
				busy_wait_us(200);
			}
			busy_wait_ms(120);
			break;
		case BEEPSOUND::NONE:
		default:
			// do nothing.
			break;
	}
	return;
}

uint32_t GetAbsoluteTimeMs()
{
	return to_ms_since_boot(get_absolute_time());
}

