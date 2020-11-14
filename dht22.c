#include "dht22.h"
#include "timing.h"
#include "stm32f10x.h"

#define MAX_COUNT_OF_DHT22	8
#define ACTION_RESET 		-5
#define ACTION_WAIT550 		-4
#define ACTION_WAIT70 		-3
#define ACTION_WAIT80 		-2

#define T550 				550
#define T70					70
#define T80					80
#define T75					75
#define T_CONSTFAKTOR		72

static volatile u8 rawData[MAX_COUNT_OF_DHT22][5];
static volatile s8 nextAction[MAX_COUNT_OF_DHT22];
static volatile u32 t[MAX_COUNT_OF_DHT22];


#define halOutLow(bus)		(GPIO_ResetBits(bus->PORT, bus->GPIO_Pin))
#define halOutRelease(bus)	(GPIO_SetBits(bus->PORT, bus->GPIO_Pin))
#define halProbeAll(bus) 	((uint16_t)bus->PORT->IDR)
#define halProbe(bus)		(GPIO_ReadInputDataBit(bus->PORT, bus->GPIO_Pin))


static void doBitbang2(DHT_BusHandle *bus, u32 now, u8 *notReadySensors){

	static u16 previousInp=0xFFFF;
	u16 inp=halProbeAll(bus);
	register u8 i=0;
	u8 tmp = 1;
	u16 flanke;
	flanke=previousInp & (u16)(~inp) & bus->GPIO_Pin; //Bit wird genau das "1", wenn zuvor 1 und jetzt 0, also wenn negative Flanke an konfigurierten Pins
	*notReadySensors=0;

	//prüfe jedes Inputbits, ob es eine negative Flanke gab
	for(i=0;i<MAX_COUNT_OF_DHT22;i++){
		if((flanke & (1<<i)) && nextAction[i]<40){ //konnte eine fallende Flanke festgestellt werden und hat es bisher nicht zu viele Flanken gegeben
			(*notReadySensors)=1;
			if(nextAction[i]>=0){
				tmp = nextAction[i] / 8; //bei welchem Byte sind wir angelangt
				rawData[i][tmp] <<= 1; // schiebe die Bits in diesem Byte um eins weiter
				if ((now - t[i]) >= ((50+50)*72)){ //wenn seit der letzten fallenden Flanke mehr als 100us vergangen sind
					rawData[i][tmp] |= 1; //dann soll eine "1" übertragen werden
				}
			}
			nextAction[i]++;
			t[i]= now;
		}else{
			//wenn keine Flanke erkannt wurde, aber auch noch eine Flanke kommen könnte (noch kein Timeout!)
			if((now - t[i]) <= ((500)*72)){
				(*notReadySensors)=1;
			}
			//Also: erst wenn alle Sensoren in ein Flanken-Timeout gelaufen sind, kann diese Schleife beendet werden
		}
	}
	previousInp=inp;
	return;
}




static DHT_ErrorLevel doProtocol(DHT_BusHandle *bus, u8 *notReadySensors){
	u32 now=TIMING_GetNativeTick();
	switch(nextAction[0]){
	case ACTION_RESET:
		halOutLow(bus);
		nextAction[0]++;
		t[0]=now;
		break;
	case ACTION_WAIT550:
		if(now - t[0]>((T550)*T_CONSTFAKTOR)){
			halOutRelease(bus);
			nextAction[0]++;
		}
		break;
	case ACTION_WAIT70:
		if(now - t[0]>=((T550+T70)*T_CONSTFAKTOR)){
			u16 inp=halProbeAll(bus);
			if(inp & bus->GPIO_Pin){
				//wenn an einem einzigen aktiven Pin eine 1 steht --> Abbruch
				return DHT_ERROR_NO_DEVICES_ON_NET;
			}
			nextAction[0]++;
		}
		break;
	case ACTION_WAIT80:
		if(now - t[0]>=((T550+T70+T80)*T_CONSTFAKTOR)){
			u16 inp=halProbeAll(bus);
			//ist überall HIGH-Level, wo ein DHT angeschlossen ist?
			if(~inp & bus->GPIO_Pin){
				//wenn an einem einzigen aktiven Pin eine 0 steht --> Abbruch
				return DHT_ERROR_NO_DEVICES_ON_NET;
			}
			nextAction[0]++;
			t[0] = now;
		}
		break;
	default:
		doBitbang2(bus, now, notReadySensors);
		break;

	}
	return DHT_ERROR_NO_ERROR_SET;
}



DHT_ErrorLevel DHT_Init_Sync(DHT_BusHandle *bus) {

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_StructInit(&GPIO_InitStructure);

	//CLOCK
	RCC_APB2PeriphClockCmd(bus->RCC_APB2Periph, ENABLE);

	//GPIO (OpenDrain)

	halOutRelease(bus);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Pin = bus->GPIO_Pin;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(bus->PORT, &GPIO_InitStructure);

	/*
	// Enable AFIO clock
	NVIC_InitTypeDef NVIC_InitStructure;

	EXTI_InitTypeDef EXTI_InitStructure;
	EXTI_StructInit(&EXTI_InitStructure);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

	GPIO_EXTILineConfig(bus->GPIO_PortSource, bus->GPIO_PinSource);

	EXTI_InitStructure.EXTI_Line = bus->EXTI_Line;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	//EXTI_Init(&EXTI_InitStructure);

	//NVIC
	NVIC_InitStructure.NVIC_IRQChannel = bus->EXTIx_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0F;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0F;
	//NVIC_Init(&NVIC_InitStructure);

	EXTI_ClearITPendingBit(bus->EXTI_Line);
	stdbus=bus;
	*/
	return DHT_ERROR_NO_ERROR_SET;

}

float DHT_GetTemp(u8 index){
	float f;
	f = rawData[index][2] & 0x7F;
	f *= 256;
	f += rawData[index][3];
	f /= 10;
	if (rawData[index][2] & 0x80){
		f *= -1;
	}
	return f;
}

float DHT_GetHumidity(u8 index) {
	float f;
	f = rawData[index][0];
	f *= 256;
	f += rawData[index][1];
	f /= 10;
	return f;

}

DHT_ErrorLevel DHT_Start_Sync(DHT_BusHandle *bus){


	u8 i=0;
	u8 tmp=1;
	//Initialisiere Datenstrukturen
	for(i=0;i<8;i++){
		if(bus->GPIO_Pin & (1<<i)){
			nextAction[i] = rawData[i][0] = rawData[i][1] = rawData[i][2] = rawData[i][3] = rawData[i][4] = 0;

		}else{
			nextAction[i] = rawData[i][0] = rawData[i][1] = rawData[i][2] = rawData[i][3] = rawData[i][4] = 100;
		}
	}
	nextAction[0]=ACTION_RESET;

	while(tmp){
		doProtocol(bus, &tmp);
		TIMING_DelayUs(1);

	}
	for(i=0;i<8;i++){
		if(bus->GPIO_Pin & (1<<i)){
			tmp=rawData[i][0]+rawData[i][1]+rawData[i][2]+rawData[i][3];
			if(tmp!=rawData[i][4]){
				return DHT_ERROR_CRC;
			}
		}
	}

	return DHT_ERROR_NO_ERROR_SET;
}






