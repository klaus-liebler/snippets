#pragma once
#include <inttypes.h>
void DHT22MULTI_Init();
void DHT22MULTI_StateMachineTimIT();
uint16_t DHT22MULTI_GetTempInTenthDegrees (uint8_t sensorIndex);
uint16_t DHT22MULTI_GetHumiInTenthDegrees (uint8_t sensorIndex);
