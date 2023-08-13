/**
 * FD-ARK (RaspberryPiPico firmware)
 * Copyright (c) 2023 Harumakkin.
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "def_gpio.h"
#include "lcd.h"

LcdAQM1602Y::LcdAQM1602Y()
{
	m_pI2c = nullptr;
	m_Addr = 0;
	m_PinSDA = 0;
	m_SinSCL = 0;
	m_Location = 0;
	return;
}

LcdAQM1602Y::~LcdAQM1602Y()
{
	// do nothing
	return;
}

void LcdAQM1602Y::sendCmds(const SENDCMD *pData)
{
	for (int t = 0; pData[t].cmd != 0xFF; ++t){
		const uint8_t cmd[] = { 0x00, pData[t].cmd};
		i2c_write_blocking(m_pI2c, m_Addr, cmd, 2, false);
		sleep_us(pData[t].wait);
	}
	return;
}

void LcdAQM1602Y::Initialize(i2c_inst_t *pI2c, const uint8_t addr, const int pinSDA, const int pinSCL)
{
	m_pI2c = pI2c;
	m_Addr = addr;
	m_PinSDA = pinSDA;
	m_SinSCL = pinSCL;

	i2c_init(m_pI2c, 540 * 1000); 					// use i2c0 module, SCL = 540kHz
    gpio_set_function(m_PinSDA, GPIO_FUNC_I2C);		// set function of SDA_PIN=GP20 I2C
    gpio_set_function(m_SinSCL, GPIO_FUNC_I2C);		// set function of SCL_PIN=GP21 I2C
	gpio_set_pulls(m_PinSDA, true, false);			// pull-up
	gpio_set_pulls(m_SinSCL, true, false);			// pull-up
	sleep_ms(10);

	const SENDCMD initdata[] = {
		{ 0x38,	760},	// Function Set:			8bits, 2 lines, single height font, IS=0
		{ 0x39,	760},	// Function Set:			IS=1(extension instruction be selected)
		{ 0x14,	27},	// Internal OSC freq.:		BS=0:1/5 BIAS;F2 F1 F0:100(internal osc)
		{ 0x73,	27},	// Contrast Set:			(C3-C0 bit)
		{ 0x52,	27},	// ICON,Contrast Set:		Icon on, booster circuit OFF(Vdd=5V)
//		{ 0x56,	27},	// ICON,Contrast Set:		Icon on, booster circuit ON (Vdd=3.3V)
		{ 0x6C,	27},	// Follower circuit on
		{ 0x38,	760},	// Function Set:			8bits, 2 lines, single height font, IS=0
		{ 0x01,	760},	// Clear Display
		{ 0x0C,	27},	// Display ON
		{ 0xFF, 0,}		// (eot)
	};
	sendCmds(initdata);
	return;
}

void LcdAQM1602Y::Home()
{
	const SENDCMD data[] = {
		{0x02, 760},	// Return Home
		{0x80, 27},	
		{0xFF, 0},	
	};
	sendCmds(data);
	m_Location = 0;
	return;
}

void LcdAQM1602Y::Clear()
{
	const SENDCMD data[] = {
		{0x01, 760},	// Clear Display
		{0x02, 27},		// Return Home
		{0x80, 27},		// DDRAM 00h
		{0xFF, 0},	
	};
	sendCmds(data);
	m_Location = 0;
	return;
}

/** ユーザー定義文字
*/
void LcdAQM1602Y::SendCG()
{
	//
	const SENDCMD data[] = {
		{0x40, 19},
		{0xFF, 0},	
	};
	sendCmds(data);

	//
	const static uint8_t cg[] = {
		/* 0x00 */	0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,		// FILL BOX
		/* 0x01 */	0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x04, 0x00,		// ↑
		/* 0x02 */	0x04, 0x04, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00,		// ↓
		/* 0x03 */	0x00, 0x04, 0x02, 0x1f, 0x02, 0x04, 0x00, 0x00,		// →
		/* 0x04 */	0x00, 0x04, 0x08, 0x1f, 0x08, 0x04, 0,00, 0x00,		// ←
	};
	for(size_t t = 0; t < sizeof(cg); ++t){
		uint8_t data[] = {0x40, cg[t] };
		i2c_write_blocking(m_pI2c, m_Addr, data, 2, false);
		busy_wait_us(19);
	}
	return;
}

void LcdAQM1602Y::PutChar(const uint8_t ch)
{
	uint8_t dt[] = {0x40, ch};
	i2c_write_blocking(m_pI2c, m_Addr, dt, 2, false);
	busy_wait_us(19);
	m_Location++;
	if (m_Location == 16) {
		const static uint8_t data[] = { 0x00, 0x80|0x40 };	// DDRAM 40h
		i2c_write_blocking(m_pI2c, m_Addr, data, 2, false);
		busy_wait_us(19);
	}
	return;
}

void LcdAQM1602Y::PutStrings(const uint8_t *pStr, const int len)
{
	for (int t = 0; t < len; ++t) {
		PutChar(pStr[t]);
	}
	return;
}

