#include "leds.h"
#include "math.h"

uint8_t LED_Data[MAX_LED][4];
uint8_t LED_Mod[MAX_LED][4];  // for brightness
int datasentflag = 0;


using namespace led;

void led::set_led_int (int LEDnum, int R, int G, int B)
{
	LED_Data[LEDnum][0] = LEDnum;
	LED_Data[LEDnum][1] = G;
	LED_Data[LEDnum][2] = R;
	LED_Data[LEDnum][3] = B;
}


void led::set_led_float (int LEDnum, float R, float G, float B)
{
	LED_Data[LEDnum][0] = LEDnum;
	LED_Data[LEDnum][1] = G * 255.0f;
	LED_Data[LEDnum][2] = R * 255.0f;
	LED_Data[LEDnum][3] = B * 255.0f;
}

void led::set_led_hsv(int LEDnum, float H, float S, float V){
    // Log dimming curve
    if(V < 0){
        V = 0;
    }
    V = V*V;

    float C = V * S;
    float H_prime = fmod(H, 360) / 60.0f;
    // volatile float mod_test = fabs((float)fmod(H_prime, 2) - 1.0f;
    float X = C * (1.0f - fabs( fmod(H_prime, 2.0f) - 1.0f));

    float r = 0;
    float g = 0;
    float b = 0;

    if(0 <= H_prime && H_prime < 1){
        r = C;
        g = X;
    }else if(1 <= H_prime && H_prime < 2){
        r = X;
        g = C;
    }else if(2 <= H_prime && H_prime < 3){
        g = C;
        b = X;
    }else if(3 <= H_prime && H_prime < 4){
        g = X;
        b = C;
    }else if(4 <= H_prime && H_prime < 5){
        r = X;
        b = C;
    }else if(5 <= H_prime && H_prime < 6){
        r = C;
        b = X;
    }

    float m = V - C;

    set_led_float(LEDnum, r+m, g+m, b+m);
}

uint16_t pwmData[(32*MAX_LED)+50];

void led::WS2812_Send (void)
{
	uint32_t indx=0;
	uint32_t color;


	for (int i= 0; i<MAX_LED; i++)
	{
		color = ((LED_Data[i][1]<<16) | (LED_Data[i][2]<<8) | (LED_Data[i][3]));

		for (int i=23; i>=0; i--)
		{
			if (color&(1<<i))
			{
				pwmData[indx] = 120;  // 2/3 of 90
			}

			else pwmData[indx] = 60;  // 1/3 of 90

			indx++;
		}

	}

	for (int i=0; i<50; i++)
	{
		pwmData[indx] = 0;
		indx++;
	}

	HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_4, (uint32_t *)pwmData, indx);
	while (!datasentflag){};
	datasentflag = 0;
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
	HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_4);
	datasentflag=1;
}
