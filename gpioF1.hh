#pragma once
#include <stm32f1xx.h>

enum class Pin:uint8_t {
	PA00,
	PA01,
	PA02,
	PA03,
	PA04,
	PA05,
	PA06,
	PA07,
	PA08,
	PA09,
	PA10,
	PA11,
	PA12,
	PA13,
	PA14,
	PA15,
	PB00,
	PB01,
	PB02,
	PB03,
	PB04,
	PB05,
	PB06,
	PB07,
	PB08,
	PB09,
	PB10,
	PB11,
	PB12,
	PB13,
	PB14,
	PB15,
	PC00,
	PC01,
	PC02,
	PC03,
	PC04,
	PC05,
	PC06,
	PC07,
	PC08,
	PC09,
	PC10,
	PC11,
	PC12,
	PC13,
	PC14,
	PC15,
	NO_PIN=UINT8_MAX
};

enum class PinMode:uint8_t
{
	OUTPUTPP_10MHz=0b0001,
	OUTPUTPP__2MHz=0b0010,
	OUTPUTPP_50MHz=0b0011,
	OUTPUTOD_10MHz=0b0101,
	OUTPUTOD__2MHz=0b0110,
	OUTPUTOD_50MHz=0b0111,

	OUTPUTAFPP_10MHz=0b1001,
	OUTPUTAFPP__2MHz=0b1010,
	OUTPUTAFPP_50MHz=0b1011,
	OUTPUTAFOD_10MHz=0b1101,
	OUTPUTAFOD__2MHz=0b1110,
	OUTPUTAFOD_50MHz=0b1111,	
	
	INPUT_ANALOG=0b0000,
	INPUT_FLOATING=0b0100,
	INPUT_PULL=0b1000,
};

enum class PullDirection:uint8_t
{
	DOWN=0,
	UP=1,
};


class Gpio {
	public:
		static void ConfigurePin (Pin pin, PinMode mode=PinMode::OUTPUTPP_10MHz);
		static void ConfigurePinPull (Pin pin, PullDirection dir=PullDirection::UP);
		static void ConfigureAFOut (Pin pin);
		static void Set(Pin pin, bool value);
		static bool Get(Pin pin);
};