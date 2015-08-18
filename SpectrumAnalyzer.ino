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

AudioInputAnalog         adc1(A3);
AudioAnalyzeFFT1024      fft1024;
AudioConnection          patchCord1(adc1, fft1024);

float scale = 60.0;		// The scale sets how much sound is needed in each frequency range to show all 16 bars.  Higher numbers are more sensitive.
float level[18];		// An array to hold the 16 frequency bands
int   shown[18];		// This array holds the on-screen levels.  When the signal drops quickly, these are used to lower the on-screen level 1 bar per update, which looks more pleasing to corresponds to human sound perception.

void setup()
{
  AudioMemory(12);		// Audio requires memory to work.
  Serial.begin(38400);
}

void loop()
{
  if (fft1024.available())
  {
    // read the 512 FFT frequencies into 16 level music is heard in octaves, but the FFT data is linear, so for the higher octaves, read many FFT bins together.
    level[0] =  fft1024.read(0);
    level[1] =  fft1024.read(1);
    level[2] =  fft1024.read(2, 3);
    level[3] =  fft1024.read(4, 5);
    level[4] =  fft1024.read(6, 9);
    level[5] =  fft1024.read(10, 13);
    level[6] =  fft1024.read(14, 18);
    level[7] =  fft1024.read(19, 26);
    level[8] =  fft1024.read(27, 35);
    level[9] =  fft1024.read(36, 49);
    level[10] = fft1024.read(50, 66);
    level[11] = fft1024.read(67, 89);
    level[12] = fft1024.read(90, 120);
    level[13] = fft1024.read(121, 160);
    level[14] = fft1024.read(161, 215);
    level[15] = fft1024.read(216, 287);
    level[16] = fft1024.read(288, 383);
    level[17] = fft1024.read(384, 511);

    for (int i=0; i<18; i++)
    {
      Serial.print(level[i]*scale);
      Serial.print(" ");

      /*
      // TODO: conversion from FFT data to display bars should be
      // exponentially scaled.  But how keep it a simple example?
      int val = level[i] * scale;
      if (val > 8) val = 8;

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
        val = shown[i];
      }
	  */
    }

    Serial.print(" CPU:");
    Serial.println(AudioProcessorUsageMax());
    delay(250);
  }
}
