#include <stm32f0xx_ll_gpio.h>
#include "gpioF0.hh"

static constexpr uint32_t CRL =0;
static constexpr uint32_t CRH =1;
static constexpr uint32_t IDR =2;
static constexpr uint32_t ODR =3;
static constexpr uint32_t BSSR =4;
static constexpr uint32_t BRR = 5;

inline static uint8_t pin2port(Pin pin)
{
	return ((uint8_t)pin) >> 4;
}

inline static uint8_t pin2localPin(Pin pin)
{
	return ((uint8_t)pin) & 0x0F;
}

inline static uint32_t* pin2portBase(Pin pin)
{
	return (uint32_t*)(GPIOA_BASE + (GPIOB_BASE-GPIOA_BASE)*(pin2port(pin)));
}

inline static uint8_t pin2CRx(Pin pin)
{
	return (((uint8_t)pin) & 0x08) >> 3;
}

inline static uint8_t pin2CRxOffset(Pin pin)
{
	return 4*(((uint8_t)pin) & 0x07);
}

void Gpio::ConfigurePin (Pin pin, PinMode pinMode) {

	if(pin==Pin::NO_PIN) return;
	uint8_t portIndex=pin2port(pin);
	uint32_t clockEnableBit = RCC_AHBENR_GPIOAEN << portIndex;
	SET_BIT(RCC->AHBENR, clockEnableBit);
	uint32_t* offsetZero = pin2portBase(pin);
	__IO uint32_t* CRx = offsetZero +  pin2CRx(pin);
	uint8_t shift = pin2CRxOffset(pin);
	uint32_t val= *CRx;
	val &=~((uint32_t)(0x0F) << shift);
	val|=((uint32_t)(pinMode) << shift);
	*CRx = val;
}

void Gpio::ConfigurePinPull (Pin pin, PullDirection dir){
	if(pin==Pin::NO_PIN) return;
	uint32_t* gpiox_ODR32 = pin2portBase(pin)+ODR;
	uint16_t* gpiox_ODR = (uint16_t*)gpiox_ODR32;
	if(dir==PullDirection::DOWN)
	{
		*gpiox_ODR &= ~(1<<pin2localPin(pin)); 
	}
	else{
		*gpiox_ODR |= (1<<pin2localPin(pin)); 
	}
}

void Gpio::Set (Pin pin, bool value) {
	if(pin==Pin::NO_PIN) return;
	uint32_t* gpiox_BSSR = pin2portBase(pin)+BSSR;
	uint8_t localPin = pin2localPin(pin);
	uint32_t bit2set= 1 << (localPin + 16 * !value);
	*gpiox_BSSR = bit2set;
}

bool Gpio::Get(Pin pin) {
	if(pin==Pin::NO_PIN) return false;
	uint32_t* gpiox_IDR32 = pin2portBase(pin)+IDR;
	uint16_t* gpiox_IDR = (uint16_t*)gpiox_IDR32;
	return * gpiox_IDR & (1 << pin2localPin(pin));
}
