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
#include <stdio.h>

// initialize audio classes
AudioInputAnalog         adc1(A3);
AudioAnalyzeFFT1024      fft1024;
AudioConnection          patchCord1(adc1, fft1024);

// The display size and color to use
const int matrix_width = 18;
const int matrix_height = 16;

// initialize global variables
float level[matrix_width];			// An array to holds the raw magnitude of the 16 frequency bands
int shown[matrix_width];			// This array holds the on-screen levels.  When the signal drops quickly, these are used to lower the on-screen level 1 bar per update, which looks more pleasing to corresponds to human sound perception.

// These parameters adjust the vertical thresholds
const float maxLevel = 0.2;      // 1.0 = max, lower is more "sensitive"
const float dynamicRange = 50.0; // total range to display, in decibels
const float linearBlend = 0.3;   // useful range is 0 to 0.7

// This array holds the volume level (0 to 1.0) for each vertical pixel to turn on.  Computed in setup() using the 3 parameters above.
float thresholdVertical[matrix_height];

// This array specifies how many of the FFT frequency bin to use for each horizontal pixel.  Because humans hear in octaves and FFT bins are linear, the low frequencies use a small number of bins, higher frequencies use more.
int frequencyBinsHorizontal[matrix_width] = {1, 1, 2, 2, 3, 3, 4, 5, 7, 8, 10, 13, 17, 21, 27, 34, 43, 54};		// only goes up to half the bins, so to 12500Hz, the higher freq bins picked up a lot of noise
float micCorrectionFactor[matrix_width] = {1.58, 1.41, 1.26, 1.12, 1.06, 1.03, 1, 1, 1, 1, 1, 1, 0.891, 0.794, 0.708, 1, 1.15, 1.3};

// Run once from setup, the compute the vertical levels
void computeVerticalLevels()
{
	unsigned int y;
	float n, logLevel, linearLevel;

	for (y=0; y < matrix_height; y++)
	{
		n = (float)y / (float)(matrix_height - 1);
		logLevel = pow10f(n * -1.0 * (dynamicRange / 20.0));
		linearLevel = 1.0 - n;
		linearLevel = linearLevel * linearBlend;
		logLevel = logLevel * (1.0 - linearBlend);
		thresholdVertical[y] = (logLevel + linearLevel) * maxLevel;
	}
}

void setup()
{
	// the audio library needs to be given memory to start working
	AudioMemory(12);		// Audio requires memory to work.

	// compute the vertical thresholds before starting
	computeVerticalLevels();
}

void loop()
{
	int currentFreqBin, prevShown;
	prevShown = 0;

	if (fft1024.available())
	{
		currentFreqBin = 2;				// skip the first two bins since they are always high

		// read fft and calculate number of levels to light for each bar
		for (int i=0; i<matrix_width; i++)
		{
			level[i] = micCorrectionFactor[i] * fft1024.read(currentFreqBin, currentFreqBin + frequencyBinsHorizontal[i] - 1);
			prevShown = shown[i];
			shown[i] = 0;
			for (int j=0; j<matrix_height; j++)
			{
				if (level[i] >= thresholdVertical[j])
				{
					shown[i] = 16-j;
					break;
				}
			}

			if ((shown[i]<prevShown) && (shown[i]>0))
			{
				shown[i] = prevShown - 1;
			}
			currentFreqBin += frequencyBinsHorizontal[i];
		}

		// Clear terminal
		Serial.write(27);       // ESC command
		Serial.print("[2J");    // clear screen command
		Serial.write(27);
		Serial.print("[H");     // cursor to home command

		// print plot to terminal
		for (int row = 0; row<=matrix_height; row++)
		{
			for (int column = 0; column<matrix_width; column++)
			{
				if (row==16)
				{
					char buffer[5];
					sprintf(buffer,"%2d ", shown[column]);
					Serial.print(buffer);
				}
				else
				{
					if (shown[column]>=(16-row))
					{
						Serial.print("-- ");
					}
					else
					{
						Serial.print("   ");
					}
				}
			}
			Serial.println();
		}
		Serial.println("** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **");

		// delay to slow refresh rate
		delay(50);
	}
}
