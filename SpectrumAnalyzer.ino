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

#define TRUE   1
#define FALSE  0
#define WHITE  0xFFFFFF
#define BLACK  0x000000

// Display size
const int matrix_width = 18;
const int matrix_height = 16;

// Set variables needed for LED strip
const int ledsPerStrip = 48;
DMAMEM int displayMemory[ledsPerStrip*6];
int drawingMemory[ledsPerStrip*6];
const int config = WS2811_GRB | WS2811_800kHz;

// Initialize all needed objects
OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config);
AudioInputAnalog         adc1(A3);
AudioAnalyzeFFT1024      fft1024;
AudioConnection          patchCord1(adc1, fft1024);

// Arrays needed to calculate what to show on bars
float level[matrix_width];			// An array to holds the raw magnitude of the 16 frequency bands
// The following two arrays have a range of 0 to 16, indicating the number of bars shown
int shown[matrix_width];			// This array holds the on-screen levels
int peakIndLevel[matrix_width];		// This array determines how high the peak indicator bar is

// These parameters adjust the vertical thresholds
const float maxLevel = 0.2;      // 1.0 = max, lower is more "sensitive"
const float dynamicRange = 45.0; // total range to display, in decibels
const float linearBlend = 0.3;   // useful range is 0 to 0.7

// This array holds the volume level (0 to 1.0) for each vertical pixel to turn on.  Computed in setup() using the 3 parameters above.
float thresholdVertical[matrix_height];

// These parameters are used to calculate how fast the bars fall after they have risen and peak indicator settings
const int colorMode = 0;			// sets which mode to use to determine coloration of the bars
const bool usePeakIndicator = TRUE;	// determines wether or not a peak indicator bar will be used
const int topBarFallDelay = 0;		// number of milliseconds it takes for the top bar to fall back down a level
const int topBarRiseDelay = 0;		// number of milliseconds it takes for the top bar to rise up a level
const int peakBarFallDelay = 100;	// number of milliseconds it takes for the peak bar to fall back down a level
const int displayUpdateDelay = 0;	// number of milliseconds it takes for the display to refresh
elapsedMillis topBarRiseTimer[matrix_width];		// auto incrementing variable to determine time since the top bar last rose for each bar
elapsedMillis topBarFallTimer[matrix_width];		// auto incrementing variable to determine time since the top bar last fell for each bar
elapsedMillis peakBarTimer[matrix_width];			// auto incrementing variable to determine time since the peak bar last fell for each bar
elapsedMillis displayUpdateTimer;					// auto incrementing variable to determine time last display update

// This array specifies how many of the FFT frequency bin to use for each horizontal pixel.  Because humans hear in octaves and FFT bins are linear, the low frequencies use a small number of bins, higher frequencies use more.
int frequencyBinsHorizontal[matrix_width] = {1, 1, 2, 2, 3, 3, 4, 5, 7, 8, 10, 13, 17, 21, 27, 34, 43, 54};		// only goes up to half the bins, so to ~12500Hz, the higher freq bins picked up a lot of noise
float micCorrectionFactor[matrix_width] = {1.58, 1.41, 1.26, 1.12, 1.06, 1.03, 1, 1, 1, 1, 1, 1, 0.891, 0.794, 0.708, 1, 1.15, 1.3};	//correction factors added due to imperfections in the mic and amplifier

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

// Helper Function for makeColor
unsigned int h2rgb(unsigned int v1, unsigned int v2, unsigned int hue)
{
	if (hue < 60) return v1 * 60 + (v2 - v1) * hue;
	if (hue < 180) return v2 * 60;
	if (hue < 240) return v1 * 60 + (v2 - v1) * (240 - hue);
	return v1 * 60;
}

// Convert HSL (Hue, Saturation, Lightness) to RGB (Red, Green, Blue)
//
//   hue:        0 to 359 - position on the color wheel, 0=red, 60=orange,
//                            120=yellow, 180=green, 240=blue, 300=violet
//
//   saturation: 0 to 100 - how bright or dull the color, 100=full, 0=gray
//
//   lightness:  0 to 100 - how light the color is, 100=white, 50=color, 0=black
//
int makeColor(unsigned int hue, unsigned int saturation, unsigned int lightness)
{
	unsigned int red, green, blue;
	unsigned int var1, var2;

	if (hue > 359) hue = hue % 360;
	if (saturation > 100) saturation = 100;
	if (lightness > 100) lightness = 100;

	// algorithm from: http://www.easyrgb.com/index.php?X=MATH&H=19#text19
	if (saturation == 0) {
		red = green = blue = lightness * 255 / 100;
	} else {
		if (lightness < 50) {
			var2 = lightness * (100 + saturation);
		} else {
			var2 = ((lightness + saturation) * 100) - (saturation * lightness);
		}
		var1 = lightness * 200 - var2;
		red = h2rgb(var1, var2, (hue < 240) ? hue + 120 : hue - 240) * 255 / 600000;
		green = h2rgb(var1, var2, hue) * 255 / 600000;
		blue = h2rgb(var1, var2, (hue >= 120) ? hue - 120 : hue + 240) * 255 / 600000;
	}
	return (red << 16) | (green << 8) | blue;
}

// selects a color from the predefined rainbow with yellow centered
int rainbowColor(int index)
{
	int color;
	switch (index)
	{
		case 1: color=0x8800FF; break;
		case 2: color=0x2400FF; break;
		case 3: color=0x003FFF; break;
		case 4: color=0x00A3FF; break;
		case 5: color=0x00FFF6; break;
		case 6: color=0x00FF91; break;
		case 7: color=0x00FF2D; break;
		case 8: color=0x36FF00; break;
		case 9: color=0x9AFF00; break;
		case 10: color=0xFFFF00; break;
		case 11: color=0xFFBF00; break;
		case 12: color=0xFF7F00; break;
		case 13: color=0xFF3F00; break;
		case 14: color=0xFF0000; break;
		case 15: color=0xFF0000; break;
		case 16: color=WHITE; break;
		default: color=WHITE; break;
	}
	return color;
}

void setup()
{
  AudioMemory(12);		// Audio requires memory to work.
  computeVerticalLevels();	// compute the vertical thresholds before starting
  leds.begin();
  leds.show();
  for (int i=0; i<matrix_width; i++)
  {
	  topBarRiseTimer[i] = 0;
	  topBarFallTimer[i] = 0;
	  peakBarTimer[i] = 0;
	  peakIndLevel[i] = 0;
  }
  displayUpdateTimer = 0;
}

void loop()
{
  int currentFreqBin, prevShown, ledHeight;
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

			// delay top bar falling to make animation look more natural
			if (shown[i] >= prevShown)
			{
				topBarFallTimer[i] = 0;
			}
			else if ((shown[i]>0) && (topBarFallTimer[i] >= topBarFallDelay))
			{
				shown[i] = prevShown - 1;
				topBarFallTimer[i] = 0;
			}

			// delay top barr rising to make animation look more natural
			if (shown[i] <= prevShown)
			{
				topBarRiseTimer[i] = 0;
			}
			else if ((shown[i]>0) && (topBarRiseTimer[i] >= topBarRiseDelay))
			{
				shown[i] = prevShown + 1;
				topBarRiseTimer[i] = 0;
			}


			// calculate where the peak indicator should be if it is being used
			if (usePeakIndicator)
			{
				if (shown[i] >= peakIndLevel[i])
				{
					peakIndLevel[i] = shown[i]+1;
					peakBarTimer[i] = 0;
				}
				else if ((peakIndLevel[i] > (shown[i]+1)) && (peakIndLevel[i]>1) && (peakBarTimer[i] >= peakBarFallDelay))
				{
					peakIndLevel[i]--;
					peakBarTimer[i] = 0;
				}
			}
			currentFreqBin += frequencyBinsHorizontal[i];
		}


    for (int bar=0; bar<18; bar++)			// set pixels
    {
    		for (int led=0; led<16; led++)
    		{
    			// height of led on panel from 1 to 16, 0 is for not shown
    			if((bar-1)%3 == 0)		// correct for upside down wired bars
    			{
    				ledHeight = 16-led;
    			}
    			else
    			{
    				ledHeight = led+1;
    			}

				if (shown[bar]>=ledHeight)
				{
					switch (colorMode)
					{
					case 0: leds.setPixel((bar*16)+led, rainbowColor(shown[bar])); break;
					case 1: leds.setPixel((bar*16)+led, rainbowColor(ledHeight)); break;
					}
				}
				else if (peakIndLevel[bar] == ledHeight)
				{
					leds.setPixel((bar*16)+led, WHITE);
				}
				else
				{
					leds.setPixel((bar*16)+led, BLACK);
				}

    		}
    }
    if (displayUpdateTimer >= displayUpdateDelay)
    {
    	leds.show();
    	displayUpdateTimer = 0;
    }
  }
}
