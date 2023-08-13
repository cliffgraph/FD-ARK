#pragma once

#include <stdio.h>	// for uint8_t

class ExtIO
{
public:
	// bit0: SW3	0:push
	// bit1: SW2	0:push
	// bit2: SW1	0:push
	// bit3: ENC1-B	
	// bit4: ENC1-A	
	// bit5: MODE-SW	
	const static uint8_t SW_SW3		= 0x01;
	const static uint8_t SW_SW2		= 0x02;
	const static uint8_t SW_SW1		= 0x04;
	const static uint8_t SW_MODE	= 0x20;		// MODE H:EMU, L:WR
public:
	const static int	LED_FD		= 0;
	const static int	LED_SD		= 1;
	const static bool	LEDON		= true;
	const static bool	LEDOFF		= false;

private:
	// MCP23S08
	const static uint8_t REG_IODIR	= 0x00;
	const static uint8_t REG_IPOL	= 0x01;
	const static uint8_t REG_GPINTEN= 0x02;
	const static uint8_t REG_DEFVAL	= 0x03;
	const static uint8_t REG_INTCON	= 0x04;
	const static uint8_t REG_IOCON	= 0x05;
	const static uint8_t REG_GPPU	= 0x06;
	const static uint8_t REG_INTF	= 0x07;
	const static uint8_t REG_INTCAP	= 0x08;
	const static uint8_t REG_GPIO	= 0x09;
	const static uint8_t REG_OLAT	= 0x0A;


private:
	int	m_pinCS;
	uint8_t m_oldSw;
	uint8_t m_cntSw;
	uint8_t m_stsSw;
	uint8_t m_encA;
	bool m_leds[2];		// LED_FD, LED_SD

public:
	ExtIO();
	virtual ~ExtIO();
private:
	void initSpiSpeed();
	void setCS_H();
	void setCS_L();
	uint8_t unchiChattering(const uint8_t raw);

public:
	void Initialize(const uint8_t pinCS);
	uint8_t GetPortRaw();
	void GetInput(uint8_t *pSw, int *pEnc);
	void SetLED(const int ledNo, const bool bSts);
	void WaitFreeInput(uint8_t sw);

};
