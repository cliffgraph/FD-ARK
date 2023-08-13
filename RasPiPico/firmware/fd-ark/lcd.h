#pragma once

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "def_gpio.h"

class LcdAQM1602Y
{
private:
	struct SENDCMD
	{
		uint8_t	cmd;
		int		wait;	// us
	};

private:
	i2c_inst_t *m_pI2c;
	int	m_Addr, m_PinSDA, m_SinSCL;
	int m_Location;

public:
	LcdAQM1602Y();
	virtual ~LcdAQM1602Y();
private:
	void sendCmds(const SENDCMD *pData);

public:
	void Initialize(i2c_inst_t *pI2c, const uint8_t addr, const int pinSDA, const int pinSCL);
	void Clear();
	void Home();
	void SendCG();
	void PutChar(const uint8_t ch);
	void PutStrings(const uint8_t *pStr, const int len);

};
