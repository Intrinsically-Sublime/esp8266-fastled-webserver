/*
 * Novel Mutations Costume Controller Firmware https://github.com/Intrinsically-Sublime/esp8266-fastled-webserver 2018
 *
 * Forked from ESP8266 + FastLED + IR Remote: https://github.com/jasoncoon/esp8266-fastled-webserver 2015-2016 Jason Coon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_INTERRUPT_RETRY_COUNT 0

#define FASTLED_INTERNAL
#include <FastLED.h>
FASTLED_USING_NAMESPACE

//#define SERIAL_OUTPUT		// Uncomment to enable serial output. Useful for debugging

extern "C" {
#include "user_interface.h"
}

#include <ESP8266WiFi.h>
//#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WebSocketsServer.h>
#include <FS.h>
#include <EEPROM.h>
#include "GradientPalettes.h"
#include "Field.h"

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define PROGMEM   ICACHE_RODATA_ATTR	// ESP PROGMEM Macro

// True = Access point mode (direct connection to ESP8266)
// False = Station mode (connects to existing network)
// IPAddress = 192.168.4.1
const bool apMode = true;

ESP8266WebServer webServer(80);
WebSocketsServer webSocketsServer = WebSocketsServer(81);
ESP8266HTTPUpdateServer httpUpdateServer;

// AP mode password
const char WiFiAPPSK[] = "";

// Wi-Fi network to connect to (if not in AP mode)
const char* ssid = "";
const char* password = "";
#define DISABLE_WIFI_ON_BOOT true	// Default is to disable WiFi on boot to preserve battery (must have button 1 to enable if disabled)

#include "FSBrowser.h"

////// External profiles. Allows separate profiles for each device. See Profile.h for all options. ///////

//#include "profiles/Profile.h"			// Full profile with all possible options.

#include "profiles/ExampleCC4Profile.h"	// Example minimal CC4P profile.
//#include "profiles/ExampleCC2Profile.h"	// Example minimal CC2 profile.
//#include "profiles/IrregularCC4Profile.h"	// Example irregular CC4P profile.

//////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////// All animation functions mapped through XY(x,y) function. ///////////////////////////

#ifndef NUM_LEDS
#define NUM_LEDS	MATRIX_WIDTH*MATRIX_HEIGHT	// Should be equal to visible LEDs
#endif

CRGB leds[NUM_LEDS+1];	// One extra pixel for hiding out of bounds data

int wrapX(int x) {	// Used by XY function and tempMatrix
	#ifdef CYLINDRICAL_MATRIX
		if (x >= MATRIX_WIDTH) {
			return x - MATRIX_WIDTH;
		} else if (x < 0) {
			return x + MATRIX_WIDTH;
		} else {
			return x;
		}
	#else
		if (x < 0 ) return 0;
		if (x >= MATRIX_WIDTH) return MATRIX_WIDTH-1;
		return x;
	#endif
}

int XY(int x, int y, bool wrap = false) {	// x = Width, y = Height

	#ifdef CYLINDRICAL_MATRIX
		// Wrap X around for use on cylinders
		if (wrap) { x = wrapX(x); }
	#endif

	// map anything outside of the matrix to the extra hidden pixel
	if (y >= MATRIX_HEIGHT || x >= MATRIX_WIDTH || x < 0 || y < 0) { return NUM_LEDS; }

	#ifdef MIRROR_WIDTH
		x = (MATRIX_WIDTH - 1) - x;
	#endif

	#ifndef MIRROR_HEIGHT	// Default is to mirror the height to be compatible with irregular matrix array that is read from the top left corner
		y = (MATRIX_HEIGHT - 1) - y;
	#endif

	#ifdef HORIZONTAL
		uint8_t xx = x;
		x = y;
		y = xx;
		#define XorY MATRIX_WIDTH
	#else
		#define XorY MATRIX_HEIGHT
	#endif

	#ifdef SERPENTINE
		if(x%2 == 0) {
			return (x * XorY) + y;
		} else {
			return (x * XorY) + ((XorY - 1) - y);
		}
	#elif defined IRREGULAR_MATRIX
		#if NUM_LEDS < 256
			return pgm_read_byte(XYTable + ((y * MATRIX_WIDTH) + x));
		#else
			return pgm_read_word(XYTable + ((y * MATRIX_WIDTH) + x));
		#endif
	#else
		return (x * XorY) + y;
	#endif
}

int getXyFromLedNum(int ledNum) {
	return XY(ledNum/MATRIX_HEIGHT,ledNum%MATRIX_HEIGHT);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

const uint16_t matrixTotal = MATRIX_WIDTH * MATRIX_HEIGHT;	// Total size of the LED matrix including any hidden pixels

#if MATRIX_HEIGHT/6 > 6
	const uint8_t fireBase	= 6;
#else
	const uint8_t fireBase = (MATRIX_HEIGHT/6)+1;
#endif

// For best battery life MILLI_AMPS = NUM_LEDS * 3 (gives poor white) Better white MILLI_AMPS = NUM_LEDS * 9 (poor battery life)
#define MILLI_AMPS         NUM_LEDS * 3	// IMPORTANT: set the max MILLI_AMPS no greater than your power supply (1A = 1000mA)
#define VOLTAGE		   4.2		//Set voltage used 4.2v for Lipo or 5v for 5V power supply or USB battery bank
#define WIFI_MAX_POWER     1		//Set wifi output power between 0 and 20.5db (default around 19db)

const uint16_t centerLED = NUM_LEDS/2;

#if MATRIX_HEIGHT >= 4 && MATRIX_WIDTH >= 4
	#define MATRIX_2D
#endif

#if MATRIX_HEIGHT >= 5 && MATRIX_WIDTH >= 3
	#define TEXT_MATRIX
#endif

// The 32bit version of our coordinates
static uint16_t noiseX;
static uint16_t noiseY;
static uint16_t noiseZ;

// Flag used to prevent over writing to the EEPROM
bool eepromChanged = false;

bool WiFiEnabled = !DISABLE_WIFI_ON_BOOT;

// Array of temp cells (used by fire, theMatrix, coloredRain, stormyRain)
uint_fast8_t tempMatrix[MATRIX_WIDTH+1][MATRIX_HEIGHT+1];
// Temporary CRGB array for storing RGB data for one column to be duplicated.
CRGB tempHeightStrip[MATRIX_HEIGHT];

const uint8_t PROGMEM brightnessMap[] = { 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 80, 96, 128, 160, 192 };
const uint8_t brightnessCount = 17;
uint8_t brightnessIndex = 14;	// 14 = 128

// ten seconds per color palette makes a good demo
// 20-120 is better for deployment
const uint8_t secondsPerPalette = 10;

// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
uint8_t cooling = 72;

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
uint8_t sparking = 84;

// SMOOTHING; How much blending should be done between frames
// Lower = more blending and smoother flames. Higher = less blending and flickery flames
const uint8_t fireSmoothing = 220;

uint8_t speed = 42;

uint8_t intensity = 42;

///////////////////////////////////////////////////////////////////////

// Forward declarations of an array of cpt-city gradient palettes, and
// a count of how many there are.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];

uint8_t gCurrentPaletteNumber = 0;

CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );

extern const CRGBPalette16 twinklePalettes[];

const CRGBPalette16 WoodFireColors_p = CRGBPalette16(CRGB::Black, CRGB::OrangeRed, CRGB::Orange, CRGB::Gold);		//* Orange
const CRGBPalette16 SodiumFireColors_p = CRGBPalette16(CRGB::Black, CRGB::Orange, CRGB::Gold, CRGB::Goldenrod);		//* Yellow
const CRGBPalette16 CopperFireColors_p = CRGBPalette16(CRGB::Black, CRGB::Green, CRGB::GreenYellow, CRGB::LimeGreen);	//* Green
const CRGBPalette16 AlcoholFireColors_p = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::DeepSkyBlue, CRGB::LightSkyBlue);//* Blue
const CRGBPalette16 RubidiumFireColors_p = CRGBPalette16(CRGB::Black, CRGB::Indigo, CRGB::Indigo, CRGB::DarkBlue);	//* Indigo
const CRGBPalette16 PotassiumFireColors_p = CRGBPalette16(CRGB::Black, CRGB::Indigo, CRGB::MediumPurple, CRGB::DeepPink);//* Violet
const CRGBPalette16 LithiumFireColors_p = CRGBPalette16(CRGB::Black, CRGB::FireBrick, CRGB::Pink, CRGB::DeepPink);	//* Red

uint8_t currentPatternIndex = 0; // Index number of which pattern is current
uint8_t autoplay = 0;

uint8_t autoplayDuration = 60;
unsigned long autoPlayTimeout = 0;

uint8_t currentPaletteIndex = 0;
uint8_t currentFirePaletteIndex = 0;
uint8_t rotatingFirePaletteIndex = 0;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

CRGB solidColor = CRGB::Blue;
CRGB solidRainColor = CRGB(60,80,90);

typedef struct {
  CRGBPalette16 palette;
   String name;
 } PaletteAndName;
typedef PaletteAndName PaletteAndNameList[];

const CRGBPalette16 palettes[] = {
	RainbowColors_p,
	RainbowStripeColors_p,
	CloudColors_p,
	LavaColors_p,
	OceanColors_p,
	ForestColors_p,
	PartyColors_p,
	HeatColors_p
};

const uint8_t paletteCount = ARRAY_SIZE(palettes);

const String paletteNames[paletteCount] = {
	"Rainbow",
	"Rainbow Stripe",
	"Cloud",
	"Lava",
	"Ocean",
	"Forest",
	"Party",
	"Heat",
};

CRGBPalette16 RotatingFire_p(WoodFireColors_p);

const CRGBPalette16 firePalettes[] = {
	WoodFireColors_p,
	SodiumFireColors_p,
	CopperFireColors_p,
	AlcoholFireColors_p,
	RubidiumFireColors_p,
	PotassiumFireColors_p,
	LithiumFireColors_p,
	RotatingFire_p
};

CRGBPalette16 TargetFire_p(SodiumFireColors_p);

const uint8_t firePaletteCount = ARRAY_SIZE(firePalettes);

const String firePaletteNames[firePaletteCount] = {
	"Wood -- Orange",
	"Sodium -- Yellow",
	"Copper -- Green",
	"Alcohol -- Blue",
	"Rubidium -- Indigo",
	"Potassium -- Violet",
	"Lithium -- Red",
	"Rotating -- ↻"
};

#ifndef CC2
	#include "Audio.h"
	#include "FunkyNoise.h"
#endif

typedef void (*Pattern)();
typedef Pattern PatternList[];
typedef struct {
  Pattern pattern;
  String name;
} PatternAndName;
typedef PatternAndName PatternAndNameList[];

#include "TwinkleFOX.h"
#ifdef MATRIX_2D
	//#include "FireWorks.h"  	// Fireworks or Fireworks2
	#include "FireWorks2.h" 	// Fireworks or Fireworks2
#endif

#ifdef MATRIX_2D
	#define FIRE_POSITION 0		// Used to keep track of where fire is in the pattern list
	#define RAIN_POSITION 2		// Used to keep track of where rain is in the pattern list
	#define STORM_POSITION 3	// Used to keep track of where storm is in the pattern list
	#define TWINKLE_POSITION 14	// Used to keep track of where twinkle is in the pattern list
	#define SOLID_POSITION 15	// Used to keep track of where solid is in the pattern list
#else
	#define FIRE_POSITION 0		// Used to keep track of where fire is in the pattern list
	#define RAIN_POSITION 99	// Unused
	#define STORM_POSITION 99	// Unused
	#define TWINKLE_POSITION 10	// Used to keep track of where twinkle is in the pattern list
	#define SOLID_POSITION 11	// Used to keep track of where solid is in the pattern list
#endif

// List of patterns to cycle through.  Each is defined as a separate function below.
PatternAndNameList patterns = {
	#ifdef CC4P
	{ fireAudio,			"Audio Activated Fire -- Uses Fire Palettes, Speed" },
	#else
	{ fireBasic,			"Fire -- Uses Fire Palettes, Speed, Cooling, Sparking" },
	#endif

	#ifdef MATRIX_2D
	{ fireworks,			"Fireworks -- Uses Speed" },
	{ coloredRain,			"Rain -- Uses Speed, Intensity, Color Picker" },
	{ stormyRain,			"Storm -- Uses Speed, Intensity, Color Picker" },
	{ theMatrix,			"The Matrix -- Uses Speed, Intensity" },
	#endif
	{ pride,			"Pride -- Uses Speed" },
	{ rainbow,			"Rainbow" },
	{ rainbowWithGlitter,		"Rainbow w/ Glitter" },
	{ rainbowSolid,			"Solid Rainbow -- Uses General Palettes" },
	{ colorWaves,			"Color Waves" },
	{ confetti,			"Confetti -- Uses General Palettes" },
	{ sinelon,			"Sinelon -- Uses General Palettes, Speed" },
	{ bpm,				"Beat -- Uses General Palettes, Speed" },
	{ juggle,			"Juggle -- Uses Speed" },

	// TwinkleFOX with palettes
	{ drawTwinkles,			"TwinkleFOX -- Uses Twinkle Palettes, Speed, Intensity" },
	{ showSolidColor,		"Solid Color -- Uses Color Picker" }

	#ifdef CC4P
	,{ spectrumWaves,		"Spectrum Waves" },		// Good (center radiating toward ends)
	{ spectrumPaletteWaves,		"Spectrum Palette Waves" },	// Good (center flows towards ends)
	{ spectrumPaletteWaves2,	"Spectrum Palette Waves 2" },	// OK (center radiating outwards)
	{ spectrumWaves2,		"Spectrum Waves 2" },		// Good (center radiating toward ends)
	{ spectrumWaves3,		"Spectrum Waves 3" },		// Good (center radiating toward ends)
	{ drawVU,			"VU" },				// OK (ends radiating toward center)
	{ analyzerPeakColumns,		"Analyzer Peak Columns" },	// Good (bottom up)
	{ beatWaves,			"BeatWaves" },			// Good (center radiating toward ends)
	{ FunkyNoise1,			"FunkyNoise1" },		// Good (Clear visualization)
	{ FunkyNoise2,			"FunkyNoise2" },		// Good (Clear visualization)
	{ FunkyNoise3,			"FunkyNoise3" },		// Ok (Slowly Changing)
	{ FunkyNoise4,			"FunkyNoise4" },		// OK (Changes very quickly and a little flashy)
	{ FunkyNoise5,			"FunkyNoise5" }			// Good (Changes quickly but smoothly)
	#endif
};

const uint8_t patternCount = ARRAY_SIZE(patterns);

#include "Fields.h"
#ifndef DISABLE_BUTTONS
#include "Buttons.h"
#endif

void setup() {
	delay(2000);
	#ifdef SERIAL_OUTPUT
	Serial.begin(115200);
	delay(100);
	Serial.setDebugOutput(true);
	#endif

	#ifndef CC2
	initializeAudio();
	#endif

	#ifdef CC4P
	#ifdef PARALLEL_OUTPUT
	FastLED.addLeds<PARALLEL_LED_TYPE,NUM_STRIPS>(leds, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
	#else
	FastLED.addLeds<SEQUENTIAL_LED_TYPE,12,GRB>(leds, 0, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
	FastLED.addLeds<SEQUENTIAL_LED_TYPE,13,GRB>(leds, NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
	FastLED.addLeds<SEQUENTIAL_LED_TYPE,14,GRB>(leds, 2*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
	FastLED.addLeds<SEQUENTIAL_LED_TYPE,15,GRB>(leds, 3*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
	#endif
	#endif

	#ifdef CC2
	#ifdef DATA_PIN_B
	FastLED.addLeds<LED_TYPE_A, DATA_PIN_A, COLOR_ORDER_A>(leds, 0, NUM_LEDS_STRIP_A).setCorrection(TypicalLEDStrip);
	FastLED.addLeds<LED_TYPE_B, DATA_PIN_B, COLOR_ORDER_B>(leds, NUM_LEDS_STRIP_A, NUM_LEDS_STRIP_B).setCorrection(TypicalLEDStrip);
//	FastLED.addLeds<LED_TYPE_A, DATA_PIN_A, COLOR_ORDER_A>(leds, 0, NUM_LEDS_STRIP_A).setCorrection(CRGB(215,172,70));
//	FastLED.addLeds<LED_TYPE_B, DATA_PIN_B, COLOR_ORDER_B>(leds, NUM_LEDS_STRIP_A, NUM_LEDS_STRIP_B).setCorrection(CRGB(255,255,160));
	#else		
	FastLED.addLeds<LED_TYPE_A, DATA_PIN_A, COLOR_ORDER_A>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
	#endif
	#endif

	FastLED.setDither(true);
	FastLED.setBrightness(brightness);
	FastLED.setMaxPowerInVoltsAndMilliamps(VOLTAGE, MILLI_AMPS);
	fill_solid(leds, NUM_LEDS, CRGB::Black);
	FastLED.show();

	EEPROM.begin(512);
	loadSettings();

	#ifdef SERIAL_OUTPUT
	Serial.println();
	Serial.print( F("Heap: ") ); Serial.println(system_get_free_heap_size());
	Serial.print( F("Boot Vers: ") ); Serial.println(system_get_boot_version());
	Serial.print( F("CPU: ") ); Serial.println(system_get_cpu_freq());
	Serial.print( F("SDK: ") ); Serial.println(system_get_sdk_version());
	Serial.print( F("Chip ID: ") ); Serial.println(system_get_chip_id());
	Serial.print( F("Flash ID: ") ); Serial.println(spi_flash_get_id());
	Serial.print( F("Flash Size: ") ); Serial.println(ESP.getFlashChipRealSize());
	Serial.print( F("Vcc: ") ); Serial.println(ESP.getVcc());
	Serial.println();
	#endif

	SPIFFS.begin();
	{
		Dir dir = SPIFFS.openDir("/");
		while (dir.next()) {
			String fileName = dir.fileName();
			size_t fileSize = dir.fileSize();
		#ifdef SERIAL_OUTPUT
			Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
		}
		Serial.printf("\n");
		#else
		}
		#endif
	}

	setPower(1);	// Be sure it is turned on when being powered on.

#ifndef DISABLE_BUTTONS
	setupButtons();
#endif

	setupWebserver();

	if (DISABLE_WIFI_ON_BOOT) {	// Disable WiFi at startup to preserve battery
		disableWiFi();
	} else {
		setupWiFi();
	}

	autoPlayTimeout = millis() + (autoplayDuration * 1000);

	// Initialize noise coordinates to some random values
	noiseX = random16();
	noiseY = random16();
	noiseZ = random16();
}

void indicatorLEDs(CRGB color)
{
	#if NUM_LEDS > 10
	#define INDICATOR_LEDS 10
	#else
	#define INDICATOR_LEDS NUM_LEDS
	#endif

	fill_solid(leds, INDICATOR_LEDS, CRGB::Black);
	for (uint8_t i = INDICATOR_LEDS; i > 0; i--) {
		leds[i] = color;
		FastLED.show();
		FastLED.delay(500/INDICATOR_LEDS);
	}
	for (uint8_t i = INDICATOR_LEDS; i > 0; i--) {
		leds[i] = CRGB::Black;
		FastLED.show();
		FastLED.delay(500/INDICATOR_LEDS);
	}
}

void disableWiFi()
{
	WiFi.disconnect(); 
	WiFi.mode(WIFI_OFF);
	WiFi.forceSleepBegin();
	delay(1);
	indicatorLEDs(CRGB::Red);
	WiFiEnabled = false;
}

void setupWiFi() 
{
	//Set wifi output power between 0 and 20.5db (default around 19db)
	WiFi.setOutputPower(WIFI_MAX_POWER);
	WiFi.setSleepMode(WIFI_NONE_SLEEP);

	if (apMode) {
		WiFi.mode(WIFI_AP);

		// Do a little work to get a unique-ish name. Append the
		// last two bytes of the MAC (HEX'd) to "Thing-":
		uint8_t mac[WL_MAC_ADDR_LENGTH];
		WiFi.softAPmacAddress(mac);
		String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
				String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
		macID.toUpperCase();
		#ifdef DEVICE_NAME
		String AP_NameString = DEVICE_NAME" " + macID;
		#else
		String AP_NameString = "ESP8266 Thing " + macID;
		#endif

		char AP_NameChar[AP_NameString.length() + 1];
		memset(AP_NameChar, 0, AP_NameString.length() + 1);

		for (int i = 0; i < AP_NameString.length(); i++)
			AP_NameChar[i] = AP_NameString.charAt(i);

		WiFi.softAP(AP_NameChar, WiFiAPPSK);

		#ifdef SERIAL_OUTPUT
		Serial.printf("Connect to Wi-Fi access point: %s\n", AP_NameChar);
		Serial.println("and open http://192.168.4.1 in your browser");
		#endif
  	} else {
		WiFi.mode(WIFI_STA);
		#ifdef SERIAL_OUTPUT
		Serial.printf("Connecting to %s\n", ssid);
		#endif
		if (String(WiFi.SSID()) != String(ssid)) {
			WiFi.begin(ssid, password);
		}

		while (WiFi.status() != WL_CONNECTED) {
			delay(500);
		#ifdef SERIAL_OUTPUT
			Serial.print(".");
		}

		Serial.print("Connected! Open http://");
		Serial.print(WiFi.localIP());
		Serial.println(" in your browser");
		#else
		}
		#endif
	}
	indicatorLEDs(CRGB::Blue);
	WiFiEnabled = true;
}

void setupWebserver()
{
	httpUpdateServer.setup(&webServer);

	webServer.on("/all", HTTP_GET, []() {
		String json = getFieldsJson(fields, fieldCount);
		webServer.send(200, "text/json", json);
	});

	webServer.on("/fieldValue", HTTP_GET, []() {
		String name = webServer.arg("name");
		String value = getFieldValue(name, fields, fieldCount);
		webServer.send(200, "text/json", value);
	});

	webServer.on("/fieldValue", HTTP_POST, []() {
		String name = webServer.arg("name");
		String value = webServer.arg("value");
		String newValue = setFieldValue(name, value, fields, fieldCount);
		webServer.send(200, "text/json", newValue);
	});

	webServer.on("/power", HTTP_POST, []() {
		String value = webServer.arg("value");
		setPower(value.toInt());
		sendInt(power);
	});

	webServer.on("/cooling", HTTP_POST, []() {
		String value = webServer.arg("value");
		cooling = value.toInt();
		broadcastInt("cooling", cooling);
		sendInt(cooling);
	});

	webServer.on("/sparking", HTTP_POST, []() {
		String value = webServer.arg("value");
		sparking = value.toInt();
		broadcastInt("sparking", sparking);
		sendInt(sparking);
	});

	webServer.on("/speed", HTTP_POST, []() {
		String value = webServer.arg("value");
		speed = value.toInt();
		broadcastInt("speed", speed);
		sendInt(speed);
	});

	webServer.on("/intensity", HTTP_POST, []() {
		String value = webServer.arg("value");
		intensity = value.toInt();
		broadcastInt("intensity", intensity);
		sendInt(intensity);
	});

	webServer.on("/solidColor", HTTP_POST, []() {
		String r = webServer.arg("r");
		String g = webServer.arg("g");
		String b = webServer.arg("b");
		setSolidColor(r.toInt(), g.toInt(), b.toInt());
		sendString(String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
	});

	webServer.on("/pattern", HTTP_POST, []() {
		String value = webServer.arg("value");
		setPattern(value.toInt());
		sendInt(currentPatternIndex);
	});

	webServer.on("/patternName", HTTP_POST, []() {
		String value = webServer.arg("value");
		setPatternName(value);
		sendInt(currentPatternIndex);
	});

	webServer.on("/palette", HTTP_POST, []() {
		String value = webServer.arg("value");
		setPalette(value.toInt());
		sendInt(currentPaletteIndex);
	});

	webServer.on("/paletteName", HTTP_POST, []() {
		String value = webServer.arg("value");
		setPaletteName(value);
		sendInt(currentPaletteIndex);
	});

	webServer.on("/firePalette", HTTP_POST, []() {
		String value = webServer.arg("value");
		setFirePalette(value.toInt());
		sendInt(currentFirePaletteIndex);
	});

	webServer.on("/firePaletteName", HTTP_POST, []() {
		String value = webServer.arg("value");
		setFirePaletteName(value);
		sendInt(currentFirePaletteIndex);
	});

	webServer.on("/brightness", HTTP_POST, []() {
		String value = webServer.arg("value");
		setBrightness(value.toInt());
		sendInt(brightness);
	});

	webServer.on("/autoplay", HTTP_POST, []() {
		String value = webServer.arg("value");
		setAutoplay(value.toInt());
		sendInt(autoplay);
	});

	webServer.on("/autoplayDuration", HTTP_POST, []() {
		String value = webServer.arg("value");
		setAutoplayDuration(value.toInt());
		sendInt(autoplayDuration);
	});

	webServer.on("/twinklePalette", HTTP_POST, []() {
		String value = webServer.arg("value");
		setTwinklePalette(value.toInt());
		sendInt(currentTwinklePaletteIndex);
	});

	webServer.on("/twinklePaletteName", HTTP_POST, []() {
		String value = webServer.arg("value");
		setTwinklePaletteName(value);
		sendInt(currentTwinklePaletteIndex);
	});

//	webServer.on("/twinkleSpeed", HTTP_POST, []() {
//		String value = webServer.arg("value");
//		twinkleSpeed = value.toInt();
//		if(twinkleSpeed < 0) twinkleSpeed = 0;
//		else if (twinkleSpeed > 8) twinkleSpeed = 8;
//		broadcastInt("twinkleSpeed", twinkleSpeed);
//		sendInt(twinkleSpeed);
//	});

//	webServer.on("/twinkleDensity", HTTP_POST, []() {
//		String value = webServer.arg("value");
//		twinkleDensity = value.toInt();
//		if(twinkleDensity < 0) twinkleDensity = 0;
//		else if (twinkleDensity > 8) twinkleDensity = 8;
//		broadcastInt("twinkleDensity", twinkleDensity);
//		sendInt(twinkleDensity);
//	});

	//list directory
	webServer.on("/list", HTTP_GET, handleFileList);
	//load editor
	webServer.on("/edit", HTTP_GET, []() {
	if (!handleFileRead("/edit.htm")) webServer.send(404, "text/plain", "FileNotFound");
	});
	//create file
	webServer.on("/edit", HTTP_PUT, handleFileCreate);
	//delete file
	webServer.on("/edit", HTTP_DELETE, handleFileDelete);
	//first callback is called after the request has ended with all parsed arguments
	//second callback handles file uploads at that location
	webServer.on("/edit", HTTP_POST, []() {
	webServer.send(200, "text/plain", "");
	}, handleFileUpload);

	webServer.serveStatic("/", SPIFFS, "/", "max-age=86400");

	webServer.begin();
	webSocketsServer.begin();
	webSocketsServer.onEvent(webSocketEvent);
	#ifdef SERIAL_OUTPUT
	Serial.println("HTTP web server started");
	Serial.println("Web socket server started");
	#endif
}

void sendInt(uint8_t value)
{
	sendString(String(value));
}

void sendString(String value)
{
	webServer.send(200, "text/plain", value);
}

void broadcastInt(String name, uint8_t value)
{
	String json = "{\"name\":\"" + name + "\",\"value\":" + String(value) + "}";
	webSocketsServer.broadcastTXT(json);
}

void broadcastString(String name, String value)
{
	String json = "{\"name\":\"" + name + "\",\"value\":\"" + String(value) + "\"}";
	webSocketsServer.broadcastTXT(json);
}

void loop() {
	// Keeps framerate below 250fps giving time for the server to serve pages when a low number of LEDs are used along with audio analyzer.
	#if NUM_LEDS_PER_STRIP < 130 && defined CC4P
	FastLED.delay(6-(NUM_LEDS_PER_STRIP*0.03));
	#endif

	#ifndef CC2
	currentMillis = millis(); // save the current timer value

	// analyze the audio input
	if (currentMillis - audioMillis > AUDIODELAY) {
	audioMillis = currentMillis;
	readAudio();
	}

	// Add entropy to random number generator; we use a lot of it.
	random16_add_entropy(analogRead(MSGEQ7_AUDIO_PIN));
	#else
	random16_add_entropy(analogRead(random8()));
	#endif

//	dnsServer.processNextRequest();
	webSocketsServer.loop();
	webServer.handleClient();

	#ifndef DISABLE_BUTTONS
	// Read Buttons
	EVERY_N_MILLISECONDS(BUTTON_CHECK_INTERVAL) {
		readButtons();
	}
	#endif

	// Only write to EEPROM every N minutes and only when data has been changed to prevent wear on the EEPROM
	EVERY_N_MINUTES(1) {
		if (eepromChanged) {
			EEPROM.commit();
			eepromChanged = false;
		}
	}

	if (power == 0) {
		fill_solid(leds, NUM_LEDS, CRGB::Black);
		FastLED.show();
		// FastLED.delay(15);
		return;
	}

//	 EVERY_N_SECONDS(10) {
//	   Serial.print( F("Heap: ") ); Serial.println(system_get_free_heap_size());
//	 }

	// change to a new cpt-city gradient palette
	EVERY_N_SECONDS( secondsPerPalette ) {
		gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, 1, gGradientPaletteCount);
		gTargetPalette = gGradientPalettes[ gCurrentPaletteNumber ];
	}

	EVERY_N_MILLISECONDS(40) {
		// slowly blend the current palette to the next
		nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 8);
		nblendPaletteTowardPalette(RotatingFire_p, TargetFire_p, 30);
		gHue++;  // slowly cycle the "base color" through the rainbow
	}

	EVERY_N_SECONDS(25) {
		RotatingFire_p = firePalettes[ rotatingFirePaletteIndex ];
		rotatingFirePaletteIndex = addmod8( rotatingFirePaletteIndex, 1, firePaletteCount-1);
		TargetFire_p = firePalettes[ rotatingFirePaletteIndex ];
	}

	if (autoplay && (millis() > autoPlayTimeout)) {
		#ifdef CUSTOM_PLAYLIST
		adjustPlaylistPattern();
		#else
		adjustPattern(true);
		if (currentPatternIndex == SOLID_POSITION) { // Skip the solid color when in autoplay
			adjustPattern(true);
		}
		#endif
		autoRotatePalettes();
		autoPlayTimeout = millis() + (autoplayDuration * 1000);
	}

	// Call the current pattern function once, updating the 'leds' array
	patterns[currentPatternIndex].pattern();

	FastLED.show();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

	#ifdef SERIAL_OUTPUT
	switch (type) {
		case WStype_DISCONNECTED:
			Serial.printf("[%u] Disconnected!\n", num);
			break;

		case WStype_CONNECTED:
			{
			IPAddress ip = webSocketsServer.remoteIP(num);
			Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

			// send message to client
			// webSocketsServer.sendTXT(num, "Connected");
			}
			break;

		case WStype_TEXT:
			Serial.printf("[%u] get Text: %s\n", num, payload);

			// send message to client
			// webSocketsServer.sendTXT(num, "message here");

			// send data to all connected clients
			// webSocketsServer.broadcastTXT("message here");
			break;

		case WStype_BIN:
			Serial.printf("[%u] get binary length: %u\n", num, length);
			hexdump(payload, length);

			// send message to client
			// webSocketsServer.sendBIN(num, payload, lenght);
			break;
	}
#endif
}

void loadSettings()
{
	brightness = EEPROM.read(0);

	currentPatternIndex = EEPROM.read(1);
	if (currentPatternIndex < 0) {
		currentPatternIndex = 0;
	} else if (currentPatternIndex >= patternCount) {
		currentPatternIndex = patternCount - 1;
	}

	byte r = EEPROM.read(2);
	byte g = EEPROM.read(3);
	byte b = EEPROM.read(4);

	if (r == 0 && g == 0 && b == 0) {
	} else {
		solidColor = CRGB(r, g, b);
	}

	power = EEPROM.read(5);

	autoplay = EEPROM.read(6);
	autoplayDuration = EEPROM.read(7);

	currentPaletteIndex = EEPROM.read(8);
	if (currentPaletteIndex < 0) {
		currentPaletteIndex = 0;
	}
	else if (currentPaletteIndex >= paletteCount) {
		currentPaletteIndex = paletteCount - 1;
	}
	currentFirePaletteIndex = EEPROM.read(9);
	currentTwinklePaletteIndex = EEPROM.read(10);
}

void setPower(uint8_t value)
{
	power = value == 0 ? 0 : 1;

	EEPROM.write(5, power);
	EEPROM.commit();

	broadcastInt("power", power);
}

void setAutoplay(uint8_t value)
{
	autoplay = value == 0 ? 0 : 1;

	EEPROM.write(6, autoplay);
	eepromChanged = true;

	broadcastInt("autoplay", autoplay);

	if (autoplay) {
		indicatorLEDs(CRGB::Green);
	} else {
		indicatorLEDs(CRGB::Purple);
	}
}

void setAutoplayDuration(uint8_t value)
{
	autoplayDuration = value;

	EEPROM.write(7, autoplayDuration);
	eepromChanged = true;

	autoPlayTimeout = millis() + (autoplayDuration * 1000);

	broadcastInt("autoplayDuration", autoplayDuration);
}

void setSolidColor(CRGB color)
{
	setSolidColor(color.r, color.g, color.b);
}

void setSolidColor(uint8_t r, uint8_t g, uint8_t b)
{
	if(currentPatternIndex == RAIN_POSITION || currentPatternIndex == STORM_POSITION) {
		solidRainColor = CRGB(r, g, b);
	} else {
		solidColor = CRGB(r, g, b);
		setPattern(SOLID_POSITION);
	}

	EEPROM.write(2, r);
	EEPROM.write(3, g);
	EEPROM.write(4, b);
	eepromChanged = true;

	broadcastString("color", String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
}

// increase or decrease the current pattern number, and wrap around at the ends
void adjustPattern(bool up)
{
	if (up) {
		currentPatternIndex = (currentPatternIndex+1)%patternCount;
	} else {
		currentPatternIndex = (currentPatternIndex+(patternCount-1))%patternCount;
	}

	if (autoplay == 0) {
		EEPROM.write(1, currentPatternIndex);
		eepromChanged = true;
	}

	broadcastInt("pattern", currentPatternIndex);
}

void adjustPlaylistPattern()
{
	currentPlaylistIndex = (currentPlaylistIndex+1)%playlistCount;
	currentPatternIndex = playlist[currentPlaylistIndex];
	broadcastInt("pattern", currentPatternIndex);
}

// change color palettes once each time through patterns when in autoplay mode
void autoRotatePalettes()
{
	if (currentPatternIndex == 0) {
		setFirePalette((currentFirePaletteIndex+1)%firePaletteCount);
		setPalette((currentPaletteIndex+1)%paletteCount);
		setTwinklePalette((currentTwinklePaletteIndex+1)%twinklePaletteCount);
	}
}

void rotateColor(bool up)
{
	if (currentPatternIndex == FIRE_POSITION) {
		if(up) {
			setFirePalette((currentFirePaletteIndex+1)%firePaletteCount);
		} else {
			setFirePalette((currentFirePaletteIndex+(firePaletteCount-1))%firePaletteCount);
		}
	} else if (currentPatternIndex == TWINKLE_POSITION) {
		if(up) {
			setTwinklePalette((currentTwinklePaletteIndex+1)%twinklePaletteCount);
		} else {
			setTwinklePalette((currentTwinklePaletteIndex+(twinklePaletteCount-1))%twinklePaletteCount);
		}
		broadcastInt("twinklePalette", currentTwinklePaletteIndex);
	#ifdef MATRIX_2D
	} else if (currentPatternIndex == RAIN_POSITION || currentPatternIndex == STORM_POSITION) {
		if(up) {
			hsv2rgb_rainbow(CHSV((rgb2hsv_approximate(solidRainColor).hue+17)%255, 80, 172),solidRainColor);
		} else {
			hsv2rgb_rainbow(CHSV((rgb2hsv_approximate(solidRainColor).hue+238)%255, 80, 172),solidRainColor);
		}
	#endif
	} else if (currentPatternIndex == SOLID_POSITION) {
		if(up) {
			hsv2rgb_rainbow(CHSV((rgb2hsv_approximate(solidColor).hue+17)%255, 255, 255),solidColor);
		} else {
			hsv2rgb_rainbow(CHSV((rgb2hsv_approximate(solidColor).hue+238)%255, 255, 255),solidColor);
		}
	} else {
		if(up) {
			setPalette((currentPaletteIndex+1)%paletteCount);
		} else {
			setPalette((currentPaletteIndex+(paletteCount-1))%paletteCount);
		}
		broadcastInt("palette", currentPaletteIndex);
	}
}

void setPattern(uint8_t value)
{
	if (value >= patternCount) {
		value = patternCount - 1;
	}

	currentPatternIndex = value;

	if (autoplay == 0) {
		EEPROM.write(1, currentPatternIndex);
		eepromChanged = true;
	}

	broadcastInt("pattern", currentPatternIndex);
}

void setPatternName(String name)
{
	for(uint8_t i = 0; i < patternCount; i++) {
		if(patterns[i].name == name) {
			setPattern(i);
			break;
		}
	}
}

void setPalette(uint8_t value)
{
	if (value >= paletteCount) {
		value = paletteCount - 1;
	}

	currentPaletteIndex = value;

	EEPROM.write(8, currentPaletteIndex);
	eepromChanged = true;

	broadcastInt("palette", currentPaletteIndex);
}

void setPaletteName(String name)
{
	for(uint8_t i = 0; i < paletteCount; i++) {
		if(paletteNames[i] == name) {
			setPalette(i);
			break;
		}
	}
}

void setFirePalette(uint8_t value)
{
	if (value >= firePaletteCount) {
		value = firePaletteCount - 1;
	}

	currentFirePaletteIndex = value;

	EEPROM.write(9, currentFirePaletteIndex);
	eepromChanged = true;

	broadcastInt("firePalette", currentFirePaletteIndex);
}

void setFirePaletteName(String name)
{
	for(uint8_t i = 0; i < firePaletteCount; i++) {
		if(firePaletteNames[i] == name) {
			setFirePalette(i);
			break;
		}
	}
}

void adjustBrightness(bool up)
{
	if (up) {
		brightnessIndex = (brightnessIndex+1)%brightnessCount;
	} else {
		brightnessIndex = (brightnessIndex+(brightnessCount-1))%brightnessCount;
	}

	brightness = pgm_read_byte(brightnessMap + brightnessIndex);

	FastLED.setBrightness(brightness);

	EEPROM.write(0, brightness);
	eepromChanged = true;

	broadcastInt("brightness", brightness);
}

void setBrightness(uint8_t value)
{
	if (value > 255) {
		value = 255;
	} else if (value < 0) {
		value = 0;
	}

	brightness = value;

	FastLED.setBrightness(brightness);

	EEPROM.write(0, brightness);
	eepromChanged = true;

	broadcastInt("brightness", brightness);
}

////////////////////////////////////// Animation Functions ////////////////////////////////////////

void showSolidColor()
{
	fill_solid(leds, NUM_LEDS, solidColor);
}

void rainbowSolid()
{
	fill_solid(leds, NUM_LEDS, CRGB(ColorFromPalette(palettes[currentPaletteIndex], gHue, 255)));
}

void confetti()
{
	// random colored speckles that blink in and fade smoothly
	fadeToBlackBy( leds, NUM_LEDS, 10);
	uint16_t pos = random16(NUM_LEDS);
	leds[pos] += ColorFromPalette(palettes[currentPaletteIndex], gHue + random8(64));
}

void fireBasic()
{
	fire(cooling, sparking);
}

// based on FastLED example Fire2012WithPalette: https://github.com/FastLED/FastLED/blob/master/examples/Fire2012WithPalette/Fire2012WithPalette.ino
void fire(uint8_t cool, uint8_t spark)
{
	FastLED.delay(1000/map8(speed,30,110));
	// Add entropy to random number generator; we use a lot of it.
	random16_add_entropy(random(256));

	CRGBPalette16 fire_p( CRGB::Black);

	if (currentFirePaletteIndex < firePaletteCount-1) {
		fire_p = firePalettes[currentFirePaletteIndex];
	} else {
		fire_p = RotatingFire_p;
	}

	// Loop for each column individually
	for (int x = 0; x < MATRIX_WIDTH; x++) {
		// Step 1.  Cool down every cell a little
		for (int i = 0; i < MATRIX_HEIGHT; i++) {
			tempMatrix[x][i] = qsub8(tempMatrix[x][i], random(0, ((cool * 10) / MATRIX_HEIGHT) + 2));
		}

		// Step 2.  Heat from each cell drifts 'up' and diffuses a little
		for (int k = MATRIX_HEIGHT; k > 0; k--) {
			tempMatrix[x][k] = (tempMatrix[x][k - 1] + tempMatrix[x][k - 2] + tempMatrix[x][k - 2]) / 3;
		}

		// Step 3.  Randomly ignite new 'sparks' of heat near the bottom
		if (random(255) < spark) {
			int j = random(fireBase);
			tempMatrix[x][j] = qadd8(tempMatrix[x][j], random(160, 255));
		}

		// Step 4.  Map from heat cells to LED colors
		for (int y = 0; y < MATRIX_HEIGHT; y++) {
			// Blend new data with previous frame. Average data between neighbouring pixels
			nblend(leds[XY(x,y)], ColorFromPalette(fire_p, ((tempMatrix[x][y]*0.7) + (tempMatrix[wrapX(x+1)][y]*0.3))), fireSmoothing);
		}
	}
}

void theMatrix()
{
	// ( Depth of dots, maximum brightness, frequency of new dots, length of tails, color, splashes, clouds )
	rain(60, 200, map8(intensity,5,100), 195, CRGB::Green, false, false, false);
}

void coloredRain()
{
	// ( Depth of dots, maximum brightness, frequency of new dots, length of tails, color, splashes, clouds )
	rain(60, 180, map8(intensity,2,60), 10, solidRainColor, true, false, false);
}

void stormyRain()
{
	// ( Depth of dots, maximum brightness, frequency of new dots, length of tails, color, splashes, clouds )
	rain(0, 90, map8(intensity,0,150)+60, 10, solidRainColor, true, true, true);
}

// based on FastLED example Fire2012WithPalette:
void rain(byte backgroundDepth, byte maxBrightness, byte spawnFreq, byte tailLength, CRGB rainColor, bool splashes, bool clouds, bool storm)
{
	FastLED.delay(1000/map8(speed,16,32));
	// Add entropy to random number generator; we use a lot of it.

	CRGB lightningColor = CRGB(72,72,80);
	CRGBPalette16 rain_p( CRGB::Black, rainColor );
	CRGBPalette16 rainClouds_p( CRGB::Black, CRGB(15,24,24), CRGB(9,15,15), CRGB::Black );

	fadeToBlackBy( leds, NUM_LEDS, 255-tailLength);

	// Loop for each column individually
	for (int x = 0; x < MATRIX_WIDTH; x++) {
		// Step 1.  Move each dot down one cell
		for (int i = 0; i < MATRIX_HEIGHT; i++) {
			if (tempMatrix[x][i] >= backgroundDepth) {	// Don't move empty cells
				if (i > 0) {
					tempMatrix[x][i-1] = tempMatrix[x][i];
				}
				tempMatrix[x][i] = 0;
			}
		}

		// Step 2.  Randomly spawn new dots at top
		if (random(255) < spawnFreq) {
			tempMatrix[x][MATRIX_HEIGHT-1] = random(backgroundDepth, maxBrightness);
		}

		// Step 3. Map from tempMatrix cells to LED colors
		for (int y = 0; y < MATRIX_HEIGHT; y++) {
			if (tempMatrix[x][y] >= backgroundDepth) {	// Don't write out empty cells
				leds[XY(x,y)] = ColorFromPalette(rain_p, tempMatrix[x][y]);
			}
		}

		// Step 4. Add splash if called for
		if (splashes) {
			static uint_fast8_t splashArray[MATRIX_WIDTH];

			byte j = splashArray[x];
			byte v = tempMatrix[x][0];

			if (j >= backgroundDepth) {
				leds[XY(x-2,0,true)] = ColorFromPalette(rain_p, j/3);
				leds[XY(x+2,0,true)] = ColorFromPalette(rain_p, j/3);
				splashArray[x] = 0; 	// Reset splash
			}

			if (v >= backgroundDepth) {
				leds[XY(x-1,1,true)] = ColorFromPalette(rain_p, v/2);
				leds[XY(x+1,1,true)] = ColorFromPalette(rain_p, v/2);
				splashArray[x] = v;	// Prep splash for next frame
			}
		}

		// Step 5. Add lightning if called for
		if (storm) {
			uint8_t lightning[MATRIX_WIDTH][MATRIX_HEIGHT];

			if (random16() < 72) {		// Odds of a lightning bolt
				lightning[scale8(random8(), MATRIX_WIDTH)][MATRIX_HEIGHT-1] = 255;	// Random starting location
				for(int ly = MATRIX_HEIGHT-1; ly > 1; ly--) {
					for (int lx = 0; lx < MATRIX_WIDTH; lx++) {
						if (lightning[lx][ly] == 255) {
							lightning[lx][ly] = 0;
							uint8_t dir = random8(4);
							switch (dir) {
								case 0:
									leds[XY(lx+1,ly-1,true)] = lightningColor;
									lightning[wrapX(lx+1)][ly-1] = 255;	// move down and right
								break;
								case 1:
									leds[XY(lx,ly-1,true)] = CRGB(128,128,128);
									lightning[lx][ly-1] = 255;		// move down
								break;
								case 2:
									leds[XY(lx-1,ly-1,true)] = CRGB(128,128,128);
									lightning[wrapX(lx-1)][ly-1] = 255;	// move down and left
								break;
								case 3:
									leds[XY(lx-1,ly-1,true)] = CRGB(128,128,128);
									lightning[wrapX(lx-1)][ly-1] = 255;	// fork down and left
									leds[XY(lx-1,ly-1,true)] = CRGB(128,128,128);
									lightning[wrapX(lx+1)][ly-1] = 255;	// fork down and right
								break;
							}
						}
					}
				}
			}
		}

		// Step 6. Add clouds if called for
		if (clouds) {
			uint16_t noiseScale = 250;	// A value of 1 will be so zoomed in, you'll mostly see solid colors. A value of 4011 will be very zoomed out and shimmery
			const uint8_t cloudHeight = (MATRIX_HEIGHT*0.16)+1;
			// This is the array that we keep our computed noise values in
			static uint8_t noise[MATRIX_WIDTH][cloudHeight];
			int xoffset = noiseScale * x + gHue;

			for(int z = 0; z < cloudHeight; z++) {
				int yoffset = noiseScale * z - gHue;
				uint8_t dataSmoothing = 192;
				uint8_t noiseData = qsub8(inoise8(noiseX + xoffset,noiseY + yoffset,noiseZ),16);
				noiseData = qadd8(noiseData,scale8(noiseData,39));
				noise[x][z] = scale8( noise[x][z], dataSmoothing) + scale8( noiseData, 256 - dataSmoothing);
				nblend(leds[XY(x,MATRIX_HEIGHT-z-1)], ColorFromPalette(rainClouds_p, noise[x][z]), (cloudHeight-z)*(250/cloudHeight));
			}
			noiseZ ++;
		}
	}
}

void addGlitter( uint8_t chanceOfGlitter)
{
	if ( random8() < chanceOfGlitter) {
		leds[ random16(NUM_LEDS) ] += CRGB::White;
	}
}

void bpm()
{
	// colored stripes pulsing at a defined Beats-Per-Minute (BPM)
	uint8_t beat = beatsin8(map8(speed,30,150), 64, 255);
	CRGBPalette16 palette = palettes[currentPaletteIndex];
	for ( int r = 0; r < MATRIX_HEIGHT; r++) {
		for (uint8_t i = 0; i < MATRIX_WIDTH; i++) {
			#ifdef REVERSE_ORDER
			leds[XY(i,MATRIX_HEIGHT-1-r)] = ColorFromPalette(palette, gHue + (r * 2), beat - gHue + (r * 10));
			#else
			leds[XY(i,r)] = ColorFromPalette(palette, gHue + (r * 2), beat - gHue + (r * 10));
			#endif
		}
	}
}

void juggle()
{
	static uint8_t    numdots =   4; // Number of dots in use.
	static uint8_t   faderate =   2; // How long should the trails be. Very low value = longer trails.
	static uint8_t     hueinc =  255 / numdots - 1; // Incremental change in hue between each dot.
	static uint8_t    thishue =   0; // Starting hue.
	static uint8_t     curhue =   0; // The current hue
	static uint8_t    thissat = 255; // Saturation of the colour.
	static uint8_t thisbright = 255; // How bright should the LED/display be.
	static uint8_t   basebeat =   5; // Higher = faster movement.

	basebeat = map8(speed,5,30);

	static uint8_t lastSecond =  99;  // Static variable, means it's only defined once. This is our 'debounce' variable.
	uint8_t secondHand = (millis() / 1000) % 30; // IMPORTANT!!! Change '30' to a different value to change duration of the loop.

	if (lastSecond != secondHand) { // Debounce to make sure we're not repeating an assignment.
		lastSecond = secondHand;
		switch (secondHand) {
			case  0: numdots = 1; basebeat = 20; hueinc = 16; faderate = 2; thishue = 0; break; // You can change values here, one at a time , or altogether.
			case 10: numdots = 4; basebeat = 10; hueinc = 16; faderate = 8; thishue = 128; break;
			case 20: numdots = 8; basebeat =  3; hueinc =  0; faderate = 8; thishue = random8(); break; // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
			case 30: break;
		}
	}

	// Several colored dots, weaving in and out of sync with each other
	curhue = thishue; // Reset the hue values.
	fadeToBlackBy(leds, NUM_LEDS, faderate);
	for ( int i = 0; i < numdots; i++) {
		uint16_t pos_n = beatsin16(basebeat + i + numdots, 0, MATRIX_HEIGHT-1);
		for (uint8_t c = 0; c < MATRIX_WIDTH; c++) {
			leds[XY(c,pos_n)] += CHSV(gHue + curhue, thissat, thisbright);
		}
		curhue += hueinc;
	}
}

void colorWaves()
{
	colorwaves( leds, MATRIX_HEIGHT, gCurrentPalette);
}

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves( CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette)
{
	static uint16_t sPseudotime = 0;
	static uint16_t sLastMillis = 0;
	static uint16_t sHue16 = 0;

	uint8_t brightdepth = beatsin88( 341, 96, 224);
	uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
	uint8_t msmultiplier = beatsin88(147, 23, 60);

	uint16_t hue16 = sHue16;//gHue * 256;
	uint16_t hueinc16 = beatsin88(113, 300, 1500);

	uint16_t ms = millis();
	uint16_t deltams = ms - sLastMillis ;
	sLastMillis  = ms;
	sPseudotime += deltams * msmultiplier;
	sHue16 += deltams * beatsin88( 400, 5, 9);
	uint16_t brightnesstheta16 = sPseudotime;

	for ( uint16_t i = 0 ; i < numleds; i++) {
		hue16 += hueinc16;
		uint8_t hue8 = hue16 / 256;
		uint16_t h16_128 = hue16 >> 7;
		if ( h16_128 & 0x100) {
			hue8 = 255 - (h16_128 >> 1);
		} else {
			hue8 = h16_128 >> 1;
		}

		brightnesstheta16  += brightnessthetainc16;
		uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

		uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
		uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
		bri8 += (255 - brightdepth);

		uint8_t index = hue8;
		//index = triwave8( index);
		index = scale8( index, 240);

		CRGB newcolor = ColorFromPalette( palette, index, bri8);

		uint16_t pixelnumber = i;
		pixelnumber = (numleds - 1) - pixelnumber;

		for (uint8_t c = 0; c < MATRIX_WIDTH; c++) {
			#ifdef REVERSE_ORDER
			nblend( ledarray[XY(c,numleds-1-pixelnumber)], newcolor, 128);
			#else
			nblend( ledarray[XY(c,pixelnumber)], newcolor, 128);
			#endif
		}
	}
}

// Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
// This function draws rainbows with an ever-changing, widely-varying set of parameters.
void pride()
{
	static uint16_t sPseudotime = 0;
	static uint16_t sLastMillis = 0;
	static uint16_t sHue16 = 0;

	uint8_t sat8 = beatsin88( 87, 220, 250);
	uint8_t brightdepth = beatsin88( 341, 200, 250);
	uint16_t brightnessthetainc16;
	uint8_t msmultiplier = beatsin88(147, 23, 60);

	brightnessthetainc16 = beatsin88( map(speed,1,255,150,475), (20 * 256), (40 * 256));

	uint16_t hue16 = sHue16;//gHue * 256;
	uint16_t hueinc16 = beatsin88(113, 1, 3000);

	uint16_t ms = millis();
	uint16_t deltams = ms - sLastMillis ;
	sLastMillis  = ms;
	sPseudotime += deltams * msmultiplier;
	sHue16 += deltams * beatsin88( 400, 5, 9);
	uint16_t brightnesstheta16 = sPseudotime;

	for ( uint16_t i = 0 ; i < matrixTotal; i++) {
		hue16 += hueinc16;
		uint8_t hue8 = hue16 / 256;

		brightnesstheta16  += brightnessthetainc16;
		uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

		uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
		uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
		bri8 += (255 - brightdepth);

		CRGB newcolor = CHSV( hue8, sat8, bri8);

		#ifdef REVERSE_ORDER
		uint16_t pixelnumber = (matrixTotal - 1) - i;
		#else
		uint16_t pixelnumber = i;
		#endif

		nblend( leds[XY(pixelnumber/MATRIX_HEIGHT,pixelnumber%MATRIX_HEIGHT)], newcolor, 64);
	}
}

void rainbow()
{
	fill_rainbow(tempHeightStrip, MATRIX_HEIGHT, gHue, 10);

	for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
		for (int y = 0; y < MATRIX_HEIGHT; y++) {
			#ifdef REVERSE_ORDER
			leds[XY(x,y)] = tempHeightStrip[y];
			#else
			leds[XY(x,y)] = tempHeightStrip[MATRIX_HEIGHT-1-y];
			#endif
		}
	}
}

void rainbowWithGlitter()
{
	// built-in FastLED rainbow, plus some random sparkly glitter
	rainbow();
	addGlitter(80);
}

void sinelon()
{
	// a colored dot sweeping back and forth, with fading trails
	fadeToBlackBy( tempHeightStrip, MATRIX_HEIGHT, 20);
	int pos = beatsin16(map8(speed,30,150), 0, MATRIX_HEIGHT - 1);
	static int prevpos = 0;
	CRGB color = ColorFromPalette(palettes[currentPaletteIndex], gHue, 255);

	if( pos < prevpos ) {
		fill_solid( tempHeightStrip+pos, (prevpos-pos)+1, color);
	} else {
		fill_solid( tempHeightStrip+prevpos, (pos-prevpos)+1, color);
	}

	for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
		for (int y = 0; y < MATRIX_HEIGHT; y++) {
			leds[XY(x,y)] = tempHeightStrip[y];
		}
	}
	prevpos = pos;
}

//////////////////////////////// End Animation Functions /////////////////////////////////////////////////
