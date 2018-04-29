/* This program is free software: you can redistribute it and/or modify
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
 * 
 * Designed for Novel Mutations Costume Controller button boards.
 * All NM button boards include debounce circuits so there is no software
 * debouncing except some offered by the EVERY_N_MILLISECONDS. Can be used
 * for any number of buttons between 2 and 5. Offering between 6 and 15
 * function calls.
 * The first two buttons control the majority of functions to work with CC2
 * All five buttons are used by CC4P
 */


#ifdef CC2
#define BUTTON_COUNT 2
const uint8_t buttonPins[BUTTON_COUNT] { 0,2 };
#endif

#ifdef CC4P
#define BUTTON_COUNT 5
const uint8_t buttonPins[BUTTON_COUNT] { 4,0,3,1,2 };	// Up, Right, Down, Left, Center
#endif

#define BUTTON_HOLD_TIME 1000			// Duration of a long click (ms)
#define BUTTON_XL_TIME 4000			// Duration of a extra long click (ms)
#define BUTTON_CHECK_INTERVAL 50		// Interval at which buttons are checked (ms)
unsigned long buttonTime[BUTTON_COUNT];		// Button down time
bool buttonState[BUTTON_COUNT];			// Button state HIGH/LOW

void setupButtons() {
	for (uint8_t b = 0; b < BUTTON_COUNT; b++) {
		pinMode(buttonPins[b], INPUT);
	}
}

// 0 = Short press, 1 = Long press, 2 = Extra long press
//
//                 UP
//                 0 = Brightness++
//                 1 = Speed++
//                 2 = WiFi on/off
//
// LEFT            CENTER            RIGHT
// 0 = Pattern--   0 = Text on/off   0 = Pattern++
// 1 = Color/Pal-- 1 = Audio on/off  1 = Color/Pal++
// 2 = Autoplay    2 = Unassigned    2 = LEDs on/off
//
//                 DOWN
//                 0 = Brightness--
//                 1 = Speed--
//                 2 = Unassigned

// CC4P has 5 buttons, CC2 has 2 buttons
void processButtonPress(uint8_t b, uint8_t pressDuration)
{
	switch(b) {
		case 0:	// CC4P = Up, CC2 = Top
			if(pressDuration == 0) {	// Brightness++
				#ifdef CC2
				brightness = (brightness+15)%255;
				#else
				adjustBrightness(true);
				#endif
			} else if(pressDuration == 1) {	// Speed++
				speed = (speed+10)%255;
			} else if(pressDuration == 2) {	// WiFi on/off
//				enableWiFi = !enableWiFi;
			}
		break;
		case 1:	// CC4P = Right, CC2 = Bottom
			if(pressDuration == 0) {	// Pattern++
				adjustPattern(true);
			} else if(pressDuration == 1) {	// Palette++
				rotateColor(true);
			} else if(pressDuration == 2) {	// LEDs On/Off
				setPower(!power);
			}
		break;
		case 2:	// CC4P = Down
			if(pressDuration == 0) {	// Brightness--
				adjustBrightness(false);
			} else if(pressDuration == 1) {	// Speed--
				speed = (speed+245)%255;
			} else if(pressDuration == 2) { // Unassigned
				// Unassigned
			}
		break;
		case 3:	// CC4P = Left
			if(pressDuration == 0) {	// Pattern--
				adjustPattern(false);
			} else if(pressDuration == 1) {	// Palette--
				rotateColor(false);
			} else if(pressDuration == 2) { // Autoplay
				setAutoplay(!autoplay);
			}
		break;
		case 4:	// CC4P = Center
			if(pressDuration == 0) {	// Text on/off
//				enableText = !enableText;
			} if(pressDuration == 1) {	// Audio on/off
//				enableAudio = !enableAudio;
			} else if(pressDuration == 2) {	// Unassigned
				// Unassigned
			}
		break;
	}
}

// Novel Mutations Costume Controllers have hardware debouncing circuits so no software deboucing needed
void readButtons()
{
	for (uint8_t b = 0; b < BUTTON_COUNT; b++) {
		uint8_t state = digitalRead(buttonPins[b]);
		if (buttonState[b] != state && state == LOW) {
			buttonState[b] = state;
			buttonTime[b] = millis();
			break;
		}

		if (buttonState[b] != state && state == HIGH) {
			buttonState[b] = state;

			unsigned long duration = millis() - buttonTime[b];

			if (duration > BUTTON_CHECK_INTERVAL) {
				if (duration > BUTTON_XL_TIME) {
					processButtonPress(b,2);
				} else if (duration > BUTTON_HOLD_TIME) {
					processButtonPress(b,1);
				} else {
					processButtonPress(b,0);
				}
			}
		}
	}
}
