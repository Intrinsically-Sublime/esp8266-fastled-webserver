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

// Smooth/average settings
#define SPECTRUMSMOOTH 0.08		// lower value = smoother
#define PEAKDECAY 0.01			// lower value = slower peak decay
#define NOISEFLOOR 65
#define ESP_ANALOG_V 0.95		// Actual peak analog voltage to esp analog input (Max 1.0 volts)

// AGC settings
//#define AUTOGAIN			// Uncomment to enable auto gain
#define AGCSMOOTH 0.004
#define GAINUPPERLIMIT 15.0
#define GAINLOWERLIMIT 0.1

// Global variables
float spectrumDecay[7] = {0};   	// holds time-averaged values
float spectrumPeaks[7] = {0};   	// holds peak values

uint8_t spectrumByte[7];		// holds 8-bit adjusted adc values
uint8_t spectrumByteSmoothed[7];	// holds smoothed 8-bit adjusted adc values
uint8_t interpolatedSpectrum[MATRIX_WIDTH];

void initializeAudio() {
	pinMode(MSGEQ7_AUDIO_PIN, INPUT);
	pinMode(MSGEQ7_RESET_PIN, OUTPUT);
	pinMode(MSGEQ7_STROBE_PIN, OUTPUT);

	digitalWrite(MSGEQ7_RESET_PIN, LOW);
	digitalWrite(MSGEQ7_STROBE_PIN, HIGH);
}

void readAudio() {

	// reset MSGEQ7 to first frequency bin
	digitalWrite(MSGEQ7_RESET_PIN, HIGH);
	delayMicroseconds(5);
	digitalWrite(MSGEQ7_RESET_PIN, LOW);

	// store raw adc value
	uint16_t spectrumValue = 0;
	// store sum of values for AGC
	uint16_t analogsum = 0;

	#ifdef AUTOGAIN
	static float gainAGC = 0.0;
	static float audioAvg = 270.0;
	#endif

	// cycle through each MSGEQ7 bin and read the analog values
	for (uint8_t i = 0; i < 7; i++) {

		// set up the MSGEQ7
		digitalWrite(MSGEQ7_STROBE_PIN, LOW);
		delayMicroseconds(50); // to allow the output to settle

		// read the analog value
		spectrumValue = analogRead(MSGEQ7_AUDIO_PIN);
		digitalWrite(MSGEQ7_STROBE_PIN, HIGH);

		// noise floor filter
		if (spectrumValue < NOISEFLOOR) {
			spectrumValue = 0;
		} else {
			spectrumValue -= NOISEFLOOR;
		}

		// correction for low analog maximum values
		spectrumValue = spectrumValue * (1 / ESP_ANALOG_V);

		// prepare average for AGC
		analogsum += spectrumValue;

		#ifdef AUTOGAIN
		// apply current gain value
		spectrumValue *= gainAGC;
		#endif

		// process time-averaged values
		spectrumDecay[i] = (1.0 - SPECTRUMSMOOTH) * spectrumDecay[i] + SPECTRUMSMOOTH * spectrumValue;

		// process peak values
		if (spectrumPeaks[i] < spectrumDecay[i]) {
			spectrumPeaks[i] = spectrumDecay[i];
		} else {
			spectrumPeaks[i] = spectrumPeaks[i] * (1.0 - PEAKDECAY);
		}

		spectrumByte[i] = spectrumValue/4;
		spectrumByteSmoothed[i] = spectrumDecay[i]/4;
	}

	#ifdef AUTOGAIN

	// Calculate audio levels for automatic gain
	audioAvg = (1.0 - AGCSMOOTH) * audioAvg + AGCSMOOTH * (analogsum / 7.0);

	// Calculate gain adjustment factor
	gainAGC = 270.0 / audioAvg;
	if (gainAGC > GAINUPPERLIMIT) gainAGC = GAINUPPERLIMIT;
	if (gainAGC < GAINLOWERLIMIT) gainAGC = GAINLOWERLIMIT;
	#endif
}

void interpolateSpectrum(uint8_t baseArray = 0)
{
	float step = (1.0 * (6))/(MATRIX_WIDTH-1);
	uint8_t b1;
	uint8_t b2;

	for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
		float f1 = x * step;
		uint8_t xx = f1;
		f1 = f1 - xx;

		if (baseArray == 0) {
			b1 = spectrumByteSmoothed[xx];
			b2 = spectrumByteSmoothed[xx+1];
		} else if (baseArray == 1) {
			b1 = spectrumByte[xx];
			b2 = spectrumByte[xx+1];
		} else if (baseArray == 2) {
			b1 = spectrumPeaks[xx]/4;
			b2 = spectrumPeaks[xx+1]/4;
		}

		if (x < MATRIX_WIDTH-1) {
			interpolatedSpectrum[x] = (1-f1)*b1 + f1*b2;
		} else {
			interpolatedSpectrum[x] = b1; // prevent out of index error
		}
	}
}

void lineSpectrum(uint8_t smooth = 1)
{
	fadeToBlackBy( leds, NUM_LEDS, 24);
	interpolateSpectrum(smooth);
	uint8_t position;

	for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
		if (smooth == 1) {
			position = map(interpolatedSpectrum[x], 0, 420, 0, MATRIX_HEIGHT-1)+((MATRIX_HEIGHT*18)/100);
		} else {
			position = map(interpolatedSpectrum[x], 0, 310, 0, MATRIX_HEIGHT-1)+((MATRIX_HEIGHT*16)/100);
		}
		leds[XY(x,position)] = CHSV(((x * (255/MATRIX_WIDTH))+gHue)%255, 255, 255);
	}
}

void lineSpectrumSmooth()
{
	lineSpectrum(0);
}

void lineSpectrumSharp()
{
	lineSpectrum(1);
}

void analyzerCenterVert(bool outwards, bool interpolate, uint8_t smooth = 0, bool filled = false)
{
	fadeToBlackBy( leds, NUM_LEDS, 24);
	uint8_t columnHeight;

	for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {

		if (interpolate) {
			interpolateSpectrum(smooth);
			columnHeight = map(interpolatedSpectrum[x], 0, 310, 0, MATRIX_HEIGHT-1);
		} else {
			columnHeight = map(spectrumByteSmoothed[x%7], 0, 310, 0, MATRIX_HEIGHT-1);
		}
		
		uint8_t columnStart = MATRIX_HEIGHT - columnHeight;

		if (filled) {
			if (outwards) {
				columnStart = columnStart / 2;

				for (uint8_t y = columnStart; y < columnStart+columnHeight; y++) {
					leds[XY(x,y)] = CHSV(((x * (255/MATRIX_WIDTH))+gHue)%255, 255, 128);
				}
			} else {
				for (uint8_t y = 0; y < columnHeight/2; y++) {
					leds[XY(x,y)] = CHSV(((x * (255/MATRIX_WIDTH))+gHue)%255, 255, 128);
				}

				for (uint8_t y = columnStart+(columnHeight/2); y < MATRIX_HEIGHT; y++) {
					leds[XY(x,y)] = CHSV(((x * (255/MATRIX_WIDTH))+gHue)%255, 255, 128);
				}
			}
		} else {
			if (outwards) {
				columnStart = columnStart / 2;
				leds[XY(x,columnStart)] = CHSV(((x * (255/MATRIX_WIDTH))+gHue)%255, 255, 128);
				leds[XY(x,columnStart+columnHeight)] = CHSV(((x * (255/MATRIX_WIDTH))+gHue)%255, 255, 128);
			} else {
				leds[XY(x,columnHeight/2)] = CHSV(((x * (255/MATRIX_WIDTH))+gHue)%255, 255, 128);
				leds[XY(x,columnStart+(columnHeight/2))] = CHSV(((x * (255/MATRIX_WIDTH))+gHue)%255, 255, 128);
			}
		}
	}
}

void analyzerCenterInVertFilled()
{
	analyzerCenterVert(false, false, 0, true);
}

void analyzerCenterInVertSpreadFilled()
{
	analyzerCenterVert(false, true, 1, true);
}

void analyzerCenterInVertSpreadSmoothFilled()
{
	analyzerCenterVert(false, true, 0, true);
}

void analyzerCenterInVertSpread()
{
	analyzerCenterVert(false, true, 1, false);
}

void analyzerCenterInVertSpreadSmooth()
{
	analyzerCenterVert(false, true, 0, false);
}

void analyzerCenterOutVertFilled()
{
	analyzerCenterVert(true, false, 0, true);
}

void analyzerCenterOutVertSpreadFilled()
{
	analyzerCenterVert(true, true, 1, true);
}

void analyzerCenterOutVertSpreadSmoothFilled()
{
	analyzerCenterVert(true, true, 0, true);
}

void analyzerPeakColumnsFunc(bool interpolate = false)
{
	fill_solid(leds, NUM_LEDS, CRGB::Black);

	for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {

		uint8_t columnHeight;
		uint8_t peakHeight;

		if (interpolate) {
			interpolateSpectrum(0);
			columnHeight = map(interpolatedSpectrum[x], 0, 512, 0, MATRIX_HEIGHT-1);
			interpolateSpectrum(2);
			peakHeight = map(interpolatedSpectrum[x], 0, 255, 0, MATRIX_WIDTH-1);
		} else {
			columnHeight = map(spectrumByteSmoothed[x%7], 0, 512, 0, MATRIX_HEIGHT-1);
			peakHeight = map(spectrumPeaks[x%7], 0, 1023, 0, MATRIX_WIDTH-1);
		}

		for (uint8_t y = 0; y < columnHeight; y++) {
			leds[XY(x,y)] = CHSV(255-((columnHeight-y)*(420/MATRIX_HEIGHT)), 255, 128);
		}

		leds[XY(x,peakHeight)] = CHSV(255,255,255);
	}
}

void analyzerPeakColumns()
{
	analyzerPeakColumnsFunc(false);
}

void analyzerPeakColumnsSpread()
{
	analyzerPeakColumnsFunc(true);
}

void fireAudio()	// fire(cool, spark);
{
	fire(map8(spectrumPeaks[1], 42, 128), map8(spectrumPeaks[3], 10, 42));
}

void spectrumPaletteWaves(uint8_t version = 1)
{
	CRGB color6;
	CRGB color5;
	CRGB color1;

	if (version == 1) {
		color6 = ColorFromPalette(gCurrentPalette, spectrumByteSmoothed[6], spectrumByteSmoothed[6]);
		color5 = ColorFromPalette(gCurrentPalette, spectrumByteSmoothed[5] / 8, spectrumByteSmoothed[5] / 8);
		color1 = ColorFromPalette(gCurrentPalette, spectrumByteSmoothed[1] / 2, spectrumByteSmoothed[1] / 2);
	} else if (version == 2) {
		color6 = ColorFromPalette(palettes[currentPaletteIndex], 255 - spectrumByteSmoothed[6], spectrumByteSmoothed[6]);
		color5 = ColorFromPalette(palettes[currentPaletteIndex], 255 - spectrumByteSmoothed[5] / 8, spectrumByteSmoothed[5] / 8);
		color1 = ColorFromPalette(palettes[currentPaletteIndex], 255 - spectrumByteSmoothed[1] / 2, spectrumByteSmoothed[1] / 2);
	}

	CRGB color = nblend(color6, color5, 256 / 8);
	color = nblend(color, color1, 256 / 2);

	leds[getXyFromLedNum(centerLED)] = color;
	leds[getXyFromLedNum(centerLED)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

	leds[getXyFromLedNum(centerLED - 1)] = color;
	leds[getXyFromLedNum(centerLED - 1)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

	//move to the left
	for (uint16_t i = NUM_LEDS - 1; i > centerLED; i--) {
		leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i - 1)];
	}
	// move to the right
	for (uint16_t i = 0; i < centerLED; i++) {
		leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i + 1)];
	}
}

void spectrumPaletteWaves1()
{
	spectrumPaletteWaves(1);
}

void spectrumPaletteWaves2()
{
	spectrumPaletteWaves(2);
}

void spectrumWaves(uint8_t version = 1)
{
	fadeToBlackBy( leds, NUM_LEDS, 2);
	CRGB color;

	if (version == 1) {
		color = CRGB(spectrumByteSmoothed[6], spectrumByteSmoothed[5] / 8, spectrumByteSmoothed[1] / 2);
	} else if (version == 2) {
		color = CRGB(spectrumByteSmoothed[5] / 8, spectrumByteSmoothed[6], spectrumByteSmoothed[1] / 2);
	} else if (version == 3) {
		color = CRGB(spectrumByteSmoothed[1] / 2, spectrumByteSmoothed[5] / 8, spectrumByteSmoothed[6]);
	}

	leds[getXyFromLedNum(centerLED)] = color;
	leds[getXyFromLedNum(centerLED)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

	leds[getXyFromLedNum(centerLED - 1)] = color;
	leds[getXyFromLedNum(centerLED - 1)].fadeToBlackBy(spectrumByteSmoothed[3] / 12);

	//move to the left
	for (uint16_t i = NUM_LEDS - 1; i > centerLED; i--) {
		leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i - 1)];
	}
	// move to the right
	for (uint16_t i = 0; i < centerLED; i++) {
		leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i + 1)];
	}
}

void spectrumWaves1()
{
	spectrumWaves(1);
}

void spectrumWaves2()
{
	spectrumWaves(2);
}

void spectrumWaves3()
{
	spectrumWaves(3);
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

///////////// OLD UNUSED

//// Attempt at beat detection
//uint8_t beatTriggered = 0;
//#define beatLevel 20.0
//#define beatDeadzone 30.0
//#define beatDelay 50
//float lastBeatVal = 0;
//uint8_t beatDetect() {
//  static float beatAvg = 0;
//  static unsigned long lastBeatMillis;
//  float specCombo = (spectrumDecay[0] + spectrumDecay[1]) / 2.0;
//  beatAvg = (1.0 - AGCSMOOTH) * beatAvg + AGCSMOOTH * specCombo;

//  if (lastBeatVal < beatAvg) lastBeatVal = beatAvg;
//  if ((specCombo - beatAvg) > beatLevel && beatTriggered == 0 && millis() - lastBeatMillis > beatDelay) {
//    beatTriggered = 1;
//    lastBeatVal = specCombo;
//    lastBeatMillis = millis();
//    return 1;
//  } else if ((lastBeatVal - specCombo) > beatDeadzone) {
//    beatTriggered = 0;
//    return 0;
//  } else {
//    return 0;
//  }
//}

//void beatWaves()
//{
//  fadeToBlackBy( leds, NUM_LEDS, 2);

//  if (beatDetect()) {
//    leds[getXyFromLedNum(centerLED)] = CRGB::Red;
//  }

//  //move to the left
//  for (unsigned int i = NUM_LEDS - 1; i > centerLED; i--) {
//    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i - 1)];
//  }
//  // move to the right
//  for (unsigned int i = 0; i < centerLED; i++) {
//    leds[getXyFromLedNum(i)] = leds[getXyFromLedNum(i + 1)];
//  }
//}
