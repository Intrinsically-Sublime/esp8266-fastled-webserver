/*
   ESP8266 + FastLED + IR Remote: https://github.com/jasoncoon/esp8266-fastled-audio
   Copyright (C) 2015-2016 Jason Coon

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Portions of this file are adapted from the work of Stefan Petrick:
// https://plus.google.com/u/0/115124694226931502095

// Portions of this file are adapted from RGB Shades Audio Demo Code by Garrett Mace:
// https://github.com/macetech/RGBShadesAudio

// Pin definitions
#define MSGEQ7_AUDIO_PIN A0
#define MSGEQ7_STROBE_PIN 5
#define MSGEQ7_RESET_PIN  16
#define AUDIODELAY 0

// Smooth/average settings
#define SPECTRUMSMOOTH 0.08		// lower value = smoother
#define PEAKDECAY 0.01			// lower value = slower peak decay
#define NOISEFLOOR 65

// AGC settings
#define AGCSMOOTH 0.004
//#define GAINUPPERLIMIT 15.0
//#define GAINLOWERLIMIT 0.1

// Global variables
uint16_t spectrumDecay[7] = {0};   	// holds time-averaged values
uint16_t spectrumPeaks[7] = {0};   	// holds peak values
//float audioAvg = 270.0;
//float gainAGC = 0.0;

uint8_t spectrumByteSmoothed[7];	// holds smoothed 8-bit adjusted adc values
//uint8_t spectrumAvg;

unsigned long currentMillis;		// store current loop's millis value
unsigned long audioMillis;		// store time of last audio update

void initializeAudio() {
	pinMode(MSGEQ7_AUDIO_PIN, INPUT);
	pinMode(MSGEQ7_RESET_PIN, OUTPUT);
	pinMode(MSGEQ7_STROBE_PIN, OUTPUT);

	digitalWrite(MSGEQ7_RESET_PIN, LOW);
	digitalWrite(MSGEQ7_STROBE_PIN, HIGH);

	#ifdef SERIAL_OUTPUT
	Serial.println("Audio Initialized");
	#endif
}

void readAudio() {
	static const byte PROGMEM spectrumFactors[7] = {9, 11, 13, 13, 12, 12, 13};

	// reset MSGEQ7 to first frequency bin
	digitalWrite(MSGEQ7_RESET_PIN, HIGH);
	delayMicroseconds(5);
	digitalWrite(MSGEQ7_RESET_PIN, LOW);

	// store raw adc value
	uint16_t spectrumValue = 0;
	// store sum of values for AGC
	uint16_t analogsum = 0;

	// cycle through each MSGEQ7 bin and read the analog values
	for (byte i = 0; i < 7; i++) {

		// set up the MSGEQ7
		digitalWrite(MSGEQ7_STROBE_PIN, LOW);
		delayMicroseconds(50); // to allow the output to settle

		// read the analog value
		spectrumValue = analogRead(MSGEQ7_AUDIO_PIN);
		digitalWrite(MSGEQ7_STROBE_PIN, HIGH);

		#ifdef SERIAL_OUTPUT
		Serial.print("  R");
		Serial.print(i);
		Serial.print("=");
		Serial.print(spectrumValue);
		#endif

		// noise floor filter
		if (spectrumValue < NOISEFLOOR) {
			spectrumValue = 0;
		} else {
			spectrumValue -= NOISEFLOOR;
		}

		// apply correction factor per frequency bin
		spectrumValue = (spectrumValue * pgm_read_byte_near(spectrumFactors + i)) / 10;

		// prepare average for AGC
		analogsum += spectrumValue;

		// apply current gain value
//		spectrumValue *= gainAGC;

		// process time-averaged values
		spectrumDecay[i] = (1.0 - SPECTRUMSMOOTH) * spectrumDecay[i] + SPECTRUMSMOOTH * spectrumValue;

		// process peak values
		if (spectrumPeaks[i] < spectrumDecay[i]) {
			spectrumPeaks[i] = spectrumDecay[i];
		} else {
			spectrumPeaks[i] = spectrumPeaks[i] * (1.0 - PEAKDECAY);
		}

		spectrumByteSmoothed[i] = spectrumDecay[i]/4;

		#ifdef SERIAL_OUTPUT
		Serial.print("..S");
		Serial.print(i);
		Serial.print("=");
		Serial.print(spectrumByteSmoothed[i]);
		#endif
	}

	// Calculate audio levels for automatic gain
//	audioAvg = (1.0 - AGCSMOOTH) * audioAvg + AGCSMOOTH * (analogsum / 7.0);

//	spectrumAvg = (analogsum / 7.0) / 4;

//	#ifdef SERIAL_OUTPUT
//	Serial.println("");
//	Serial.print("Average - ");
//	Serial.println(spectrumAvg);
//	#endif

	// Calculate gain adjustment factor
//	gainAGC = 270.0 / audioAvg;
//	if (gainAGC > GAINUPPERLIMIT) gainAGC = GAINUPPERLIMIT;
//	if (gainAGC < GAINLOWERLIMIT) gainAGC = GAINLOWERLIMIT;
}

void analyzerPeakColumns()
{
	fill_solid(leds, NUM_LEDS, CRGB::Black);


	#if MATRIX_WIDTH < 2
		#define APC_WIDTH 7
		const unsigned int columnSize = NUM_LEDS / 7;
	#elif MATRIX_WIDTH < 7
		#define APC_WIDTH MATRIX_WIDTH
		const unsigned int columnSize = MATRIX_HEIGHT;
	#else
		#define APC_WIDTH MATRIX_WIDTH
		const unsigned int columnSize = MATRIX_HEIGHT;
	#endif

	for (byte i = 0; i < APC_WIDTH; i++) {
		unsigned int columnStart = i * columnSize;
		unsigned int columnEnd = columnStart + columnSize;

		if (columnEnd >= NUM_LEDS) columnEnd = NUM_LEDS - 1;

		unsigned int columnHeight = map(spectrumByteSmoothed[i%7], 0, map(intensity, 0, 255, 800, 385) , 0, columnSize-1);
		unsigned int peakHeight = map(spectrumPeaks[i%7], 0, 1023, 0, columnSize-1);

		for (unsigned int j = columnStart; j < columnStart + columnHeight; j++) {
			if (j < NUM_LEDS && j <= columnEnd) {
				leds[getXyFromLedNum(j)] = CHSV(((i * (255/APC_WIDTH))+gHue)%255, 255, 128);
			}
		}

		unsigned int k = columnStart + peakHeight;

		if (k < NUM_LEDS && k <= columnEnd) {
			leds[getXyFromLedNum(k)] = CHSV(((i * (255/APC_WIDTH))+gHue)%255, 255, 255);
		}
	}
}

void fireAudio()	// fire(cool, spark);
{
	fire(map8(spectrumPeaks[1], 42, 128), map8(spectrumPeaks[3], 10, 42));
}

// Attempt at beat detection
byte beatTriggered = 0;
#define beatLevel 20.0
#define beatDeadzone 30.0
#define beatDelay 50
float lastBeatVal = 0;
byte beatDetect() {
  static float beatAvg = 0;
  static unsigned long lastBeatMillis;
  float specCombo = (spectrumDecay[0] + spectrumDecay[1]) / 2.0;
  beatAvg = (1.0 - AGCSMOOTH) * beatAvg + AGCSMOOTH * specCombo;

  if (lastBeatVal < beatAvg) lastBeatVal = beatAvg;
  if ((specCombo - beatAvg) > beatLevel && beatTriggered == 0 && currentMillis - lastBeatMillis > beatDelay) {
    beatTriggered = 1;
    lastBeatVal = specCombo;
    lastBeatMillis = currentMillis;
    return 1;
  } else if ((lastBeatVal - specCombo) > beatDeadzone) {
    beatTriggered = 0;
    return 0;
  } else {
    return 0;
  }
}

void fade_down(byte value) {
  for (unsigned int i = 0; i < NUM_LEDS; i++)
  {
    leds[i].fadeToBlackBy(value);
  }
}

void spectrumPaletteWaves()
{
//  fade_down(1);

  CRGB color6 = ColorFromPalette(gCurrentPalette, spectrumByteSmoothed[6], spectrumByteSmoothed[6]);
  CRGB color5 = ColorFromPalette(gCurrentPalette, spectrumByteSmoothed[5] / 8, spectrumByteSmoothed[5] / 8);
  CRGB color1 = ColorFromPalette(gCurrentPalette, spectrumByteSmoothed[1] / 2, spectrumByteSmoothed[1] / 2);

  CRGB color = nblend(color6, color5, 256 / 8);
  color = nblend(color, color1, 256 / 2);

  leds[getXyFromLedNum(centerLED)] = color;
  leds[getXyFromLedNum(centerLED)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

  leds[getXyFromLedNum(centerLED - 1)] = color;
  leds[getXyFromLedNum(centerLED - 1)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

  //move to the left
  for (unsigned int i = NUM_LEDS - 1; i > centerLED; i--) {
    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i - 1)];
  }
  // move to the right
  for (unsigned int i = 0; i < centerLED; i++) {
    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i + 1)];
  }
}

void spectrumPaletteWaves2()
{
//  fade_down(1);

  CRGBPalette16 palette = palettes[currentPaletteIndex];

  CRGB color6 = ColorFromPalette(palette, 255 - spectrumByteSmoothed[6], spectrumByteSmoothed[6]);
  CRGB color5 = ColorFromPalette(palette, 255 - spectrumByteSmoothed[5] / 8, spectrumByteSmoothed[5] / 8);
  CRGB color1 = ColorFromPalette(palette, 255 - spectrumByteSmoothed[1] / 2, spectrumByteSmoothed[1] / 2);

  CRGB color = nblend(color6, color5, 256 / 8);
  color = nblend(color, color1, 256 / 2);

  leds[getXyFromLedNum(centerLED)] = color;
  leds[getXyFromLedNum(centerLED)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

  leds[getXyFromLedNum(centerLED - 1)] = color;
  leds[getXyFromLedNum(centerLED - 1)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

  //move to the left
  for (unsigned int i = NUM_LEDS - 1; i > centerLED; i--) {
    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i - 1)];
  }
  // move to the right
  for (unsigned int i = 0; i < centerLED; i++) {
    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i + 1)];
  }
}

void spectrumWaves()
{
  fade_down(2);

  CRGB color = CRGB(spectrumByteSmoothed[6], spectrumByteSmoothed[5] / 8, spectrumByteSmoothed[1] / 2);

  leds[getXyFromLedNum(centerLED)] = color;
  leds[getXyFromLedNum(centerLED)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

  leds[getXyFromLedNum(centerLED - 1)] = color;
  leds[getXyFromLedNum(centerLED - 1)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

  //move to the left
  for (unsigned int i = NUM_LEDS - 1; i > centerLED; i--) {
    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i - 1)];
  }
  // move to the right
  for (unsigned int i = 0; i < centerLED; i++) {
    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i + 1)];
  }
}

void spectrumWaves2()
{
  fade_down(2);

  CRGB color = CRGB(spectrumByteSmoothed[5] / 8, spectrumByteSmoothed[6], spectrumByteSmoothed[1] / 2);

  leds[getXyFromLedNum(centerLED)] = color;
  leds[getXyFromLedNum(centerLED)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

  leds[getXyFromLedNum(centerLED - 1)] = color;
  leds[getXyFromLedNum(centerLED - 1)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

  //move to the left
  for (unsigned int i = NUM_LEDS - 1; i > centerLED; i--) {
    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i - 1)];
  }
  // move to the right
  for (unsigned int i = 0; i < centerLED; i++) {
    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i + 1)];
  }
}

void spectrumWaves3()
{
  fade_down(2);

  CRGB color = CRGB(spectrumByteSmoothed[1] / 2, spectrumByteSmoothed[5] / 8, spectrumByteSmoothed[6]);

  leds[getXyFromLedNum(centerLED)] = color;
  leds[getXyFromLedNum(centerLED)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

  leds[getXyFromLedNum(centerLED - 1)] = color;
  leds[getXyFromLedNum(centerLED - 1)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

  //move to the left
  for (unsigned int i = NUM_LEDS - 1; i > centerLED; i--) {
    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i - 1)];
  }
  // move to the right
  for (unsigned int i = 0; i < centerLED; i++) {
    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i + 1)];
  }
}

//void analyzerColumns()
//{
//  fill_solid(leds, NUM_LEDS, CRGB::Black);

//  const unsigned int columnSize = MATRIX_HEIGHT;

//  for (byte i = 0; i < 7; i++) {
//    unsigned int columnStart = i * columnSize;
//    unsigned int columnEnd = columnStart + columnSize;

//    if (columnEnd >= NUM_LEDS) columnEnd = NUM_LEDS - 1;

//    unsigned int columnHeight = map8(spectrumByteSmoothed[i], 1, columnSize);

//    for (unsigned int j = columnStart; j < columnStart + columnHeight; j++) {
//      if (j >= NUM_LEDS || j >= columnEnd)
//        continue;

//      leds[NUM_LEDS-1-j] = CHSV(i * 40, 255, 255);
//    }
//  }
//}

//void analyzerPeakColumns()
//{
//  fill_solid(leds, NUM_LEDS, CRGB::Black);

//  const unsigned int columnSize = MATRIX_HEIGHT;

//  for (byte i = 0; i < 7; i++) {
//    unsigned int columnStart = i * columnSize;
//    unsigned int columnEnd = columnStart + columnSize;

//    if (columnEnd >= NUM_LEDS) columnEnd = NUM_LEDS - 1;

//    unsigned int columnHeight = map(spectrumByteSmoothed[i], 0, 1023, 0, columnSize);
//    unsigned int peakHeight = map(spectrumPeaks[i], 0, 1023, 0, columnSize);

//    for (unsigned int j = columnStart; j < columnStart + columnHeight; j++) {
//      if (j < NUM_LEDS && j <= columnEnd) {
//        leds[j] = CHSV(i * 40, 255, 128);
//      }
//    }

//    unsigned int k = columnStart + peakHeight;
//    if (k < NUM_LEDS && k <= columnEnd)
//      leds[k] = CHSV(i * 40, 255, 255);
//  }
//}

void beatWaves()
{
  fade_down(2);

  if (beatDetect()) {
    leds[getXyFromLedNum(centerLED)] = CRGB::Red;
  }

  //move to the left
  for (unsigned int i = NUM_LEDS - 1; i > centerLED; i--) {
    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i - 1)];
  }
  // move to the right
  for (unsigned int i = 0; i < centerLED; i++) {
    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i + 1)];
  }
}


#define VUFadeFactor 5
#define VUScaleFactor 2.0
#define VUPaletteFactor 1.5
void drawVU() {
  CRGB pixelColor;

  const float xScale = 255.0 / (NUM_LEDS / 2);
  float specCombo = (spectrumDecay[0] + spectrumDecay[1] + spectrumDecay[2] + spectrumDecay[3]) / 4.0;

  for (uint16_t x = 0; x < NUM_LEDS / 2; x++) {
    int senseValue = specCombo / VUScaleFactor - xScale * x;
    int pixelBrightness = senseValue * VUFadeFactor;
    if (pixelBrightness > 255) pixelBrightness = 255;
    if (pixelBrightness < 0) pixelBrightness = 0;

    int pixelPaletteIndex = senseValue / VUPaletteFactor - 15;
    if (pixelPaletteIndex > 240) pixelPaletteIndex = 240;
    if (pixelPaletteIndex < 0) pixelPaletteIndex = 0;

    pixelColor = ColorFromPalette(palettes[currentPaletteIndex], pixelPaletteIndex, pixelBrightness);

    leds[getXyFromLedNum(x)] = pixelColor;
    leds[getXyFromLedNum(NUM_LEDS - x - 1)] = pixelColor;
  }
}
