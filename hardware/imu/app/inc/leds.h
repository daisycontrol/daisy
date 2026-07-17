#ifndef LEDS_H
#define LEDS_H

#include "app_main.h"

extern TIM_HandleTypeDef htim3;
extern DMA_HandleTypeDef hdma_tim3_ch2;

#define MAX_LED 2
namespace led
{

void WS2812_Send (void);
void set_led_int (int LEDnum, int R, int G, int B);
void set_led_float (int LEDnum, float R, float G, float B);
void set_led_hsv(int LEDnum, float H, float S, float V);

}

#endif