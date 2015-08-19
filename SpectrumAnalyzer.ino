/*
  Required Connections
  --------------------
    pin 2:  LED Strip #1    OctoWS2811 drives 8 LED Strips.
    pin 14: LED strip #2    All 8 are the same length.
    pin 7:  LED strip #3
    pin 8:  LED strip #4    A 100 ohm resistor should used
    pin 6:  LED strip #5    between each Teensy pin and the
    pin 20: LED strip #6    wire to the LED strip, to minimize
    pin 21: LED strip #7    high frequency ringining & noise.
    pin 5:  LED strip #8
    pin 15 & 16 - Connect together, but do not use
    pin 4 - Do not use
    pin 3 - Do not use as PWM.  Normal use is ok.
    pin 1 - Output indicating CPU usage, monitor with an oscilloscope,
            logic analyzer or even an LED (brighter = CPU busier)
    pin 17: A3, use a mic input
*/

#include <Audio.h>
#include <OctoWS2811.h>

#define WHITE  0xFFFFFF
#define BLACK  0x000000

const int ledsPerStrip = 48;
DMAMEM int displayMemory[ledsPerStrip*6];
int drawingMemory[ledsPerStrip*6];
const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config);
AudioInputAnalog         adc1(A3);
AudioAnalyzeFFT1024      fft1024;
AudioConnection          patchCord1(adc1, fft1024);

float scale = 50.0;		// The scale sets how much sound is needed in each frequency range to show all 16 bars.  Higher numbers are more sensitive.
float level[18];		// An array to hold the 16 frequency bands
int   shown[18];		// This array holds the on-screen levels.  When the signal drops quickly, these are used to lower the on-screen level 1 bar per update, which looks more pleasing to corresponds to human sound perception.

void setup()
{
  AudioMemory(12);		// Audio requires memory to work.
  leds.begin();
  leds.show();
}

void loop()
{
  if (fft1024.available())
  {
    // read the 512 FFT frequencies into 16 level music is heard in octaves, but the FFT data is linear, so for the higher octaves, read many FFT bins together.
    level[0] =  fft1024.read(2);
    level[1] =  fft1024.read(3);
    level[2] =  fft1024.read(4, 5);
    level[3] =  fft1024.read(6, 7);
    level[4] =  fft1024.read(8, 10);
    level[5] =  fft1024.read(11, 13);
    level[6] =  fft1024.read(14, 17);
    level[7] =  fft1024.read(18, 22);
    level[8] =  fft1024.read(23, 29);
    level[9] =  fft1024.read(30, 37);
    level[10] = fft1024.read(38, 47);
    level[11] = fft1024.read(48, 60);
    level[12] = fft1024.read(61, 77);
    level[13] = fft1024.read(78, 98);
    level[14] = fft1024.read(99, 125);
    level[15] = fft1024.read(126, 159);
    level[16] = fft1024.read(160, 202);
    level[17] = fft1024.read(203, 255);

    for (int i=0; i<18; i++)			// calculate number of levels to light
    {

      // TODO: conversion from FFT data to display bars should be exponentially scaled.  But how keep it a simple example?
      int val = level[i] * scale;
      if (val > 16) val = 16;

      if (val >= shown[i])
      {
        shown[i] = val;
      }
      else
      {
        if (shown[i] > 0)
        {
        	shown[i] = shown[i] - 1;
        }
      }
    }


    for (int bar=0; bar<18; bar++)			// set pixels
    {
    		for (int led=0; led<16; led++)
    		{
    			if((bar-1)%3 == 0)				// reverse upside down bar
    			{
					if(shown[bar]>=(16-led))
					{
						leds.setPixel((bar*16)+led, WHITE);
					}
					else
					{
						leds.setPixel((bar*16)+led, BLACK);
					}
    			}
    			else
    			{
					if(shown[bar]>=(led+1))
					{
						leds.setPixel((bar*16)+led, WHITE);
					}
					else
					{
						leds.setPixel((bar*16)+led, BLACK);
					}
    			}
    		}
    }
    leds.show();
    delay(50);
  }
}
