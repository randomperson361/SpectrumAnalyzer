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
    pin 17: A#, use for pot
    pin 18: A4, use a mic input
    pin 19: use for button
*/

#include <ADC.h>
#include <Audio.h>
#include <OctoWS2811.h>

#define MIC_PIN		18
#define POT_PIN		17
#define BUTTON_PIN 	19

#define BLACK 0x000000

#define TRUE   1
#define FALSE  0

// Display size
const int matrix_width = 18;
const int matrix_height = 16;

// Set variables needed for LED strip
const int ledsPerStrip = 48;
DMAMEM int displayMemory[ledsPerStrip*6];
int drawingMemory[ledsPerStrip*6];
const int config = WS2811_GRB | WS2811_800kHz;

// Initialize all needed objects
ADC *adc = new ADC();
OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config);
AudioInputAnalog         adc1(MIC_PIN);
AudioAnalyzeFFT1024      fft1024;
AudioConnection          patchCord1(adc1, fft1024);

// Arrays needed to calculate what to show on bars
float level[matrix_width];			// An array to holds the raw magnitude of the 16 frequency bands
// The following two arrays have a value range of 0 to 16, indicating the number of bars shown
int shown[matrix_width];			// This array holds the on-screen levels
int prevShown[matrix_width];		// This array holds the previous on-screen levels
int peakIndLevel[matrix_width];		// This array determines how high the peak indicator bar is
bool isFalling[matrix_width];		// This array tells whether or not a bar is falling

// These parameters adjust the vertical thresholds
const float maxLevel = 0.1;      // 1.0 = max, lower is more "sensitive"
const float dynamicRange = 50.0; // total range to display, in decibels
const float linearBlend = 0.4;   // useful range is 0 to 0.7

// This array holds the volume level (0 to 1.0) for each vertical pixel to turn on.  Computed in setup() using the 3 parameters above.
float thresholdVertical[matrix_height];

// These variables are used to set the LED color mode and brightness
int colorMode = 0;			// sets which mode to use to determine coloration of the bars
int potVal;				// stores the reading from the pot

// These parameters are used to calculate how fast the bars fall after they have risen and peak indicator settings
const bool usePeakIndicator = TRUE;	// determines wether or not a peak indicator bar will be used
const int dimNum = 0;				// nummber of bars to dim while bar is falling
const int topBarFallDelay = 10;		// number of milliseconds it takes for the top bar to fall back down a level
const int topBarRiseDelay = 0;		// number of milliseconds it takes for the top bar to rise up a level
const int peakBarFallDelay = 100;	// number of milliseconds it takes for the peak bar to fall back down a level
const int displayUpdateDelay = 0;	// number of milliseconds it takes for the display to refresh
elapsedMillis topBarRiseTimer[matrix_width];		// auto incrementing variable to determine time since the top bar last rose for each bar
elapsedMillis topBarFallTimer[matrix_width];		// auto incrementing variable to determine time since the top bar last fell for each bar
elapsedMillis peakBarTimer[matrix_width];			// auto incrementing variable to determine time since the peak bar last fell for each bar
elapsedMillis displayUpdateTimer;					// auto incrementing variable to determine time last display update

// This array specifies how many of the FFT frequency bin to use for each horizontal pixel.  Because humans hear in octaves and FFT bins are linear, the low frequencies use a small number of bins, higher frequencies use more.
int frequencyBinsHorizontal[matrix_width] = {1, 1, 1, 2, 2, 3, 3, 4, 5, 6, 7, 9, 11, 13, 16, 20, 24, 29};		// only goes up to ~160 the bins, so to ~7000Hz, the higher freq bins picked up a lot of noise and arent used often in music
float micCorrectionFactor[matrix_width] = {1, 1.03, 1.09, 1.09, 1.06, 1.03, 1, 1, 1, 1, 1, 1, 1, 0.94, 0.89, 0.84, 0.89, 0.94};	//correction factors added due to imperfections in the mic and amplifier

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
int rainbowColor(int index, int brightness)
{
	int color;
	switch (index)
	{
		case 1: color=makeColor(272, 100, brightness); break;
		case 2: color=makeColor(248, 100, brightness); break;
		case 3: color=makeColor(225, 100, brightness); break;
		case 4: color=makeColor(202, 100, brightness); break;
		case 5: color=makeColor(178, 100, brightness); break;
		case 6: color=makeColor(154, 100, brightness); break;
		case 7: color=makeColor(131, 100, brightness); break;
		case 8: color=makeColor(107, 100, brightness); break;
		case 9: color=makeColor(84, 100, brightness); break;
		case 10: color=makeColor(60, 100, brightness); break;
		case 11: color=makeColor(45, 100, brightness); break;
		case 12: color=makeColor(30, 100, brightness); break;
		case 13: color=makeColor(15, 100, brightness); break;
		case 14: color=makeColor(0, 100, brightness); break;
		case 15: color=makeColor(0, 100, brightness); break;
		case 16: color=makeColor(0, 0, brightness); break;
		default: color=makeColor(0, 0, brightness); break;
	}
	return color;
}

void setup()
{
  pinMode(POT_PIN, INPUT);			// configure pot pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);	// configure button pin
  adc->setAveraging(4, ADC_1);
  adc->setResolution(8, ADC_1);
  adc->setConversionSpeed(ADC_VERY_LOW_SPEED,ADC_1);
  adc->setSamplingSpeed(ADC_VERY_LOW_SPEED,ADC_1);
  adc->setReference(ADC_REF_3V3, ADC_1);
  Serial.begin(38400);

  AudioMemory(12);		// Audio requires memory to work.
  computeVerticalLevels();	// compute the vertical thresholds before starting
  leds.begin();
  leds.show();
  for (int i=0; i<matrix_width; i++)
  {
	  prevShown[i] = 0;
	  topBarRiseTimer[i] = 0;
	  topBarFallTimer[i] = 0;
	  peakBarTimer[i] = 0;
	  peakIndLevel[i] = 0;
  }
  displayUpdateTimer = 0;
}

void loop()
{
  if(!digitalRead(BUTTON_PIN))
  {
  	colorMode++;
  	colorMode %= 13;
  	delay(200);
  }
  potVal = adc->analogRead(POT_PIN, ADC_1);
  potVal = (potVal*75)/255;
  int currentFreqBin, ledHeight, maxShown, brightness;
  float dimFactor;
  if (fft1024.available())
  {
		currentFreqBin = 2;				// skip the first two bins since they are always high
		maxShown = 0;					// this variable holds the highest bar level

		// read fft and calculate number of levels to light for each bar
		for (int i=0; i<matrix_width; i++)
		{
			level[i] = micCorrectionFactor[i] * fft1024.read(currentFreqBin, currentFreqBin + frequencyBinsHorizontal[i] - 1);
			prevShown[i] = shown[i];
			shown[i] = 0;
			for (int j=0; j<matrix_height; j++)
			{
				if (level[i] >= thresholdVertical[j])
				{
					shown[i] = 16-j;
					break;
				}
			}
			if (shown[i] > maxShown)
			{
				maxShown = shown[i];
			}
			currentFreqBin += frequencyBinsHorizontal[i];
		}

		for (int bar=0; bar<18; bar++)			// set pixels
		{
			// drop all bars to bottom if sound below minimum threshold
			/*
			if (maxShown < 8)
			{
				shown[bar] = 0;
			}
			*/

			// delay top bar falling to make animation look more natural
			if (shown[bar] >= prevShown[bar])
			{
				isFalling[bar] = FALSE;
				topBarFallTimer[bar] = 0;
			}
			else if ((shown[bar]>0) && (topBarFallTimer[bar] >= topBarFallDelay))
			{
				isFalling[bar] = TRUE;
				shown[bar] = prevShown[bar] - 1;
				topBarFallTimer[bar] = 0;
			}
			else
			{
				isFalling[bar] = TRUE;
				shown[bar] = prevShown[bar];
			}

			// delay top barr rising to make animation look more natural
			if (shown[bar] <= prevShown[bar])
			{
				topBarRiseTimer[bar] = 0;
			}
			else if ((shown[bar]>0) && (topBarRiseTimer[bar] >= topBarRiseDelay))
			{
				shown[bar] = prevShown[bar] + 1;
				topBarRiseTimer[bar] = 0;
			}
			else
			{
				shown[bar] = prevShown[bar];
			}

			// calculate where the peak indicator should be if it is being used
			if (usePeakIndicator)
			{
				if (shown[bar] >= peakIndLevel[bar])
				{
					peakIndLevel[bar] = shown[bar]+1;
					peakBarTimer[bar] = 0;
				}
				else if ((peakIndLevel[bar] > (shown[bar]+1)) && (peakIndLevel[bar]>1) && (peakBarTimer[bar] >= peakBarFallDelay))
				{
					peakIndLevel[bar]--;
					peakBarTimer[bar] = 0;
				}
			}

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
					if (isFalling[bar] && (ledHeight > ((shown[bar]+1)- dimNum)))
					{
						dimFactor = ((shown[bar]+1.-ledHeight)) / (dimNum+1.);
					}
					else
					{
						dimFactor = 1.0;
					}

					brightness = int(potVal*dimFactor);

					switch (colorMode)
					{
					case 0: leds.setPixel((bar*16)+led, rainbowColor(shown[bar],brightness)); break;
					case 1: leds.setPixel((bar*16)+led, rainbowColor(ledHeight,brightness)); break;
					case 2: leds.setPixel((bar*16)+led, makeColor(bar*20, 100, brightness)); break;
					case 3: leds.setPixel((bar*16)+led, makeColor(((bar*20)+(millis()/2))%360, 100, brightness)); break;
					case 4: leds.setPixel((bar*16)+led, makeColor(((bar*20)+(millis()/10))%360, 100, brightness)); break;
					case 5: leds.setPixel((bar*16)+led, makeColor(((bar*20)+(millis()/50))%360, 100, brightness)); break;
					case 6: leds.setPixel((bar*16)+led, makeColor(0, 0, brightness)); break;
					case 7: leds.setPixel((bar*16)+led, makeColor(0, 100, brightness)); break;
					case 8: leds.setPixel((bar*16)+led, makeColor(60, 100, brightness)); break;
					case 9: leds.setPixel((bar*16)+led, makeColor(120, 100, brightness)); break;
					case 10:leds.setPixel((bar*16)+led, makeColor(180, 100, brightness)); break;
					case 11:leds.setPixel((bar*16)+led, makeColor(240, 100, brightness)); break;
					case 12:leds.setPixel((bar*16)+led, makeColor(300, 100, brightness)); break;
					}
				}
				else if (peakIndLevel[bar] == ledHeight)
				{
					leds.setPixel((bar*16)+led, makeColor(0,0,potVal));
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
