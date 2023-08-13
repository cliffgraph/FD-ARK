#include <stdio.h>
#include "pico/stdlib.h"
#include "def_gpio.h"
#include "extio.h"

ExtIO::ExtIO()
{
	m_pinCS = -1;
	m_oldSw = 0;
	m_cntSw = 0;
	m_stsSw = 0;
	m_encA = 0;
	m_leds[0] = false;
	m_leds[1] = false;
	return;
}

ExtIO::~ExtIO()
{
	// do nothing
	return;
}

void ExtIO::initSpiSpeed()
{
	spi_init( SPIDEV, 5 * 1000 * 1000 ); 
	return;
}

void ExtIO::setCS_H()
{
	if (0 <= m_pinCS) 
		gpio_put(m_pinCS, 1/*HIGHT*/);
	return;
}

void ExtIO::setCS_L()
{
	if (0 <= m_pinCS) 
		gpio_put(m_pinCS, 0/*LOW*/);
	return;
}

uint8_t ExtIO::unchiChattering(const uint8_t raw)
{
	const uint8_t sw = raw & (SW_SW3|SW_SW2|SW_SW1|SW_MODE);
	if (m_oldSw != sw){
		m_cntSw = 0;
	}
	if (m_cntSw < 5){
		++m_cntSw;
		if( m_cntSw == 5){
			m_stsSw = sw;
		}
	}
	m_oldSw = sw;
	return m_stsSw;
}

void ExtIO::Initialize(const uint8_t pinCS)
{
	initSpiSpeed();

	m_pinCS = pinCS;

	const static uint8_t initData[] = {
		0x40, REG_IODIR,	0x3f,	// IODIR GP0-4=IN, GP5-7=OUT 
		0x40, REG_GPPU,		0xff,	// Pull-up
		0x40, REG_OLAT,		0x3f,	// 出力値をセットする
		0x40, REG_GPIO,		0xc0,	// セットした出力値をピンに反映させる（1にセットしたbitのみが反映する。ラッチさせる、という）
	};
	setCS_L();
    spi_write_blocking (spi0, initData, sizeof(initData) );
	setCS_H();

	sleep_ms(100);

	for (int t = 0; t < 10; ++t)
		GetInput(nullptr, nullptr);
	return;
}

uint8_t ExtIO::GetPortRaw()
{
	initSpiSpeed();

	const static uint8_t writeData[] = {
		0x41, REG_GPIO, 0x00,
	};
	uint8_t readData[3];
	setCS_L();
    spi_write_read_blocking (spi0, writeData, readData, sizeof(writeData) );
	setCS_H();
	return readData[2];
}

void ExtIO::GetInput(uint8_t *pSw, int *pEnc)
{
	auto rawDt = GetPortRaw();

	// SWの読み込み
	auto sw = unchiChattering(rawDt);
	if (pSw != nullptr)
		*pSw = sw;

	// エンコーダーの読み込み
 	static const int stepVal[] = {
		0, -1, +1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
	const uint8_t e = (rawDt>>3) & 0x03;
	m_encA = ((m_encA<<2) | e) & 0x0f;
	if (pEnc != nullptr) {
		*pEnc = stepVal[m_encA];
	}
	return;
}

/* LEDの消灯／点灯
* @bSts false 消灯
* @bSts	true 点灯
* @return none
*/
void ExtIO::SetLED(const int ledNo, const bool bSts)
{
	initSpiSpeed();

	m_leds[ledNo] = !bSts;
	uint8_t initData[] = {
		0x40, REG_OLAT,		0x3f,	// 出力値をセットする
	};
	initData[2] = 0x3f | (((m_leds[0])?0:1)<<7) | (((m_leds[1])?0:1)<<6);
	setCS_L();
    spi_write_blocking (spi0, initData, sizeof(initData) );
	setCS_H();
	return;
}

void ExtIO::WaitFreeInput(uint8_t sw)
{
	uint8_t temp;
	for(;;) {
		GetInput(&temp, nullptr);
		if ( (temp&sw)==sw ){
			break;
		}

	}
	return;
}


