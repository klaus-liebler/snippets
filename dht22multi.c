/*
 * dht22multi.c
 *
 *  Created on: 06.12.2014
 *      Author: klaus
 */
#include <inttypes.h>
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_dma.h"

#define TIMx TIM6
#define GPIOx GPIOD

#define GPIO_PIN_Filter (GPIO_PIN_0)
#define MAX_COUNT_OF_DHT22		1

//Das ist der Vorgänger-Status, in den beim lezten Interrupt eingetreten wurde
//im Handler muss nun alles getan werden, um in den nächsten Status zu kommen
//am Ende des Handlers ist dieser neue Status dann zu setzen

#define DHTSTATE_INIT			-6
#define DHTSTATE_WAIT			-5
#define DHTSTATE_PULLDOWN		-4
#define DHTSTATE_RELEASE		-3
#define DHTSTATE_CHECKSENSOR		-2
#define DHTSTATE_MEASURE		-1

#define halProbeAll()			(((GPIOx)->IDR) & GPIO_PIN_Filter)

static volatile uint8_t rawData[MAX_COUNT_OF_DHT22][5];
static volatile int16_t temperatures[MAX_COUNT_OF_DHT22];
static volatile uint16_t humidities[MAX_COUNT_OF_DHT22];

static volatile int8_t nextAction[MAX_COUNT_OF_DHT22];
static volatile uint32_t t[MAX_COUNT_OF_DHT22];
static volatile uint16_t previousInp;
static uint16_t timeunit;

uint16_t DHT22_CRCERROR;
uint16_t DHT22_TIMINGERROR;

void
resetStructures ()
{
  uint8_t i;
  for (i = 0; i < MAX_COUNT_OF_DHT22; i++)
    {
      nextAction[i] = -1;
      t[i] = 0;
    }
  previousInp = 0;
  timeunit = 0;
}

void
DHT22MULTI_Init ()
{
  HAL_GPIO_WritePin (GPIOx, GPIO_PIN_Filter, GPIO_PIN_SET); //RELEASE
  resetStructures ();
  uint8_t sensorIndex;
  for (sensorIndex = 0; sensorIndex < MAX_COUNT_OF_DHT22; sensorIndex++)
    {
      temperatures[sensorIndex] = 20 * 10;
      humidities[sensorIndex] = 50 * 10;

    }
  DHT22_CRCERROR=0;
  DHT22_TIMINGERROR=0;
  nextAction[0] = DHTSTATE_INIT;
  TIMx->DIER |= TIM_IT_UPDATE;
  TIMx->CR1 |= TIM_CR1_CEN;
}

static void
doCalculation ()
{
  uint8_t sensorIndex;
  for (sensorIndex = 0; sensorIndex < MAX_COUNT_OF_DHT22; sensorIndex++)
    {
      uint8_t tmp = rawData[sensorIndex][0] + rawData[sensorIndex][1]
	  + rawData[sensorIndex][2] + rawData[sensorIndex][3];
      if (tmp != rawData[sensorIndex][4])
	{
	 GPIOD->ODR |= GPIO_PIN_14;
	 DHT22_CRCERROR |= (1<<sensorIndex);
	}
      else
	{
	  /*
	  float f;
	  f = rawData[sensorIndex][2] & 0x7F;
	  f *= 256;
	  f += rawData[sensorIndex][3];
	  f /= 10;
	  if (rawData[sensorIndex][2] & 0x80)
	    {
	      f *= -1;
	    }
	  temperatures[sensorIndex] = f;
*/
	  int16_t t = ((rawData[sensorIndex][2] & 0x7F) << 8) + rawData[sensorIndex][3]; //TODO: Negative Temperaturen!!!
	  temperatures[sensorIndex] = (int16_t) t;
	  //t = (rawData[sensorIndex][0] << 8) + rawData[sensorIndex][1];
	  //humidities[sensorIndex] = (int16_t) t;
	}
    }
}

static void
doBitbang2 (uint8_t *notReadySensors)
{
  uint16_t inp = (uint16_t) halProbeAll();
  register uint8_t i = 0;
  uint8_t tmp = 1;
  uint16_t flanke;
  flanke = previousInp & ((uint16_t) (~inp)) & GPIO_PIN_Filter; //Bit wird genau das "1", wenn zuvor 1 und jetzt 0, also wenn negative Flanke an konfigurierten Pins
  *notReadySensors = 0;

  //prüfe jedes Inputbits, ob es eine negative Flanke gab
  for (i = 0; i < MAX_COUNT_OF_DHT22; i++)
    {
      if ((flanke & (1 << i)) && nextAction[i] < 40)
	{ //konnte eine fallende Flanke festgestellt werden und hat es bisher nicht zu viele Flanken gegeben
	  (*notReadySensors) = 1;
	  if (nextAction[i] >= 0)
	    {
	      tmp = nextAction[i] / 8; //bei welchem Byte sind wir angelangt
	      rawData[i][tmp] <<= 1; // schiebe die Bits in diesem Byte um eins weiter
	      if ((timeunit - t[i]) > 6)
		{ //wenn seit der letzten fallenden Flanke mehr als 100us vergangen sind
		  rawData[i][tmp] |= 1; //dann soll eine "1" übertragen werden
		}
	    }
	  nextAction[i]++;
	  t[i] = timeunit;
	}
      else
	{
	  //wenn keine Flanke erkannt wurde, aber auch noch eine Flanke kommen könnte (noch kein Timeout!)
	  if ((timeunit - t[i]) <= 200)
	    {
	      (*notReadySensors) = 1;
	    }
	  //Also: erst wenn alle Sensoren in ein Flanken-Timeout gelaufen sind, kann diese Schleife beendet werden
	}
    }
  timeunit++;
  previousInp = inp;
  return;
}

void
DHT22MULTI_StateMachineTimIT ()
{
  //Annahme: Interrupts sind gecleared!!!
  uint32_t time;
  uint16_t rawVal;
  uint8_t notReadySensors;
  GPIOD->ODR ^= GPIO_PIN_12; //BSP_LED_Toggle(LED6);
  switch (nextAction[0])
    {
    case DHTSTATE_INIT:
      TIMx->PSC = 2687; //Prescaler für den nächsten Zyklus vorbereiten; ARR für den aktuellen...
      TIMx->ARR = 1;
      nextAction[0] = DHTSTATE_WAIT;
      break;
    case DHTSTATE_WAIT:
      TIMx->ARR = 62500;
      resetStructures ();
      nextAction[0] = DHTSTATE_PULLDOWN;
      break;
    case DHTSTATE_PULLDOWN:
      HAL_GPIO_WritePin (GPIOx, GPIO_PIN_Filter, GPIO_PIN_RESET); //PullDown
      TIMx->PSC = 671; //Vorbereitung fürs nächste! Zyklus ab dem nächsten ist dann 8us!
      TIMx->ARR = 624; // --> ziehe die DHTs 20ms runter
      nextAction[0] = DHTSTATE_RELEASE;
      break;
    case DHTSTATE_RELEASE:
      HAL_GPIO_WritePin (GPIOx, GPIO_PIN_Filter, GPIO_PIN_SET); //RELEASE
      TIMx->ARR = 5; //48uS nach dem Release muss der DHT dann runter gezogen haben
      nextAction[0] = DHTSTATE_CHECKSENSOR;
      break;
    case DHTSTATE_CHECKSENSOR:
      rawVal = GPIOx->IDR & GPIO_PIN_Filter;
      if (rawVal != 0)
	{
	  GPIOD->ODR |= GPIO_PIN_13;
	  DHT22_TIMINGERROR |=rawVal;
	}
      //TODO: jeder konfigurierte Anschluss muss low sein, ansonsten Fehlerbit setzen. Dieses Fehlerbit wird dann beim nächsten Fehlermanagementzyklus ausgelesen.
      TIMx->ARR = 1; //jetzt beginnt der 16uSek-Synch-Zyklus
      //nextAction[0] = DHTSTATE_SENSOR_PULLDOWN;
      nextAction[0] = DHTSTATE_MEASURE;
      break;
    default:
      doBitbang2 (&notReadySensors);
      if (!notReadySensors)
	{
	  doCalculation ();
	  nextAction[0] = DHTSTATE_INIT;
	}
      break;
    }
}

uint16_t
DHT22MULTI_GetTempInTenthDegrees (uint8_t sensorIndex)
{
  if (sensorIndex < MAX_COUNT_OF_DHT22)
    {
      return temperatures[sensorIndex];
    }
  return 0xFFFF;
}
uint16_t
DHT22MULTI_GetHumiInTenthDegrees (uint8_t sensorIndex)
{
  if (sensorIndex < MAX_COUNT_OF_DHT22)
    {
      return humidities[sensorIndex];
    }
  return 0xFFFF;
}

HAL_StatusTypeDef
DHT22MULTI_Evaluate (uint16_t *rawData, uint16_t rawSize,
		     uint16_t *temperatures, uint16_t *humidities)
{

  uint8_t sensorIndex;
  uint8_t value[5];
  for (sensorIndex = 0; sensorIndex < 16; sensorIndex++)
    {
      uint16_t sensormask = 1 << sensorIndex;
      uint8_t bitindex = 0;
      uint16_t rawIndex;
      int8_t timer = -1; //-1 means: waiting for positive edge;
      for (rawIndex = 0; rawIndex < rawSize; rawIndex++)
	{
	  uint16_t currentVal = rawData[rawIndex] && sensormask;
	  if (currentVal)
	    {
	      if (timer < 0)
		{
		  timer = 0;
		}
	      else
		{
		  timer++;
		}
	    }
	  else
	    {
	      if (timer > 0)
		{
		  //bit setzen/resetten
		  if (timer > 4)
		    {
		      value[bitindex >> 3] |= 1 << (bitindex & 0x07);
		    }
		  else
		    {
		      value[bitindex >> 3] &= ~(1 << (bitindex & 0x07));
		    }

		}
	      timer = -1;
	    }
	}
      //Array ist jetzt beschrieben. Jetzt müssen die Werte noch in ein sinnvolles Format gewandelt werden
      //und in das Ausgabe-Array geschrieben werden
      DHT22MULTI_Init ();      //starte wieder von vorne
      return HAL_OK;

    }
}

