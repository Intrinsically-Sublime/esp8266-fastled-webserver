/////////////////// This profile contains all user configurable compile time options /////////////////////
/// To generate irregular Matrix see https://intrinsically-sublime.github.io/FastLED-XY-Map-Generator/ ///

// Default settings are same as ExampleCC4Profile

// Default connections have the first pixel in the top left corner. Use mirror height and mirror width to change locations

#define			CC4P	// ESP-12 based with MSGEQ7 and 5 button D pad and 4 way parallel output
//#define			CC2	// ESP-01 based with 2 button input and 2 outputs

#define DEVICE_NAME 	"Test"
#define MATRIX_WIDTH	24	// Column count
#define MATRIX_HEIGHT	25	// Row count
//#define SERPENTINE		// Uncomment for zigzag/serpentine wiring
//#define HORIZONTAL		// Uncomment for horizontal wiring
//#define MIRROR_WIDTH		// Uncomment to mirror the output horizontally
//#define MIRROR_HEIGHT		// Uncomment to mirror the output vertically
//#define CYLINDRICAL_MATRIX	// Uncomment if your matrix wraps around in a cylinder
//#define REVERSE_ORDER		// Uncomment to have the patterns run in reverse direction (for a limited number of patterns)
//#define DISABLE_BUTTONS	// For use when connected via serial or for generic ESP boards without buttons
//#define IRREGULAR_MATRIX	// Uncomment to use irregular matrix (see example irregular profile)

#ifdef CC4P	// CC4P Specific Settings
	#define PARALLEL_OUTPUT		// Uncommented = Parallel output on WS2813_PORTA. Commented = Sequential output on pins 12,13,14,15
	#define PARALLEL_LED_TYPE	WS2813_PORTA
	#define SEQUENTIAL_LED_TYPE	WS2813
	#define NUM_LEDS_PER_STRIP	150
	#define NUM_STRIPS		4
#endif

#ifdef CC2	// CC2 Specific Settings. NO PARALLEL OUTPUT
	#define DATA_PIN_A		1
	#define LED_TYPE_A		WS2812B
	#define COLOR_ORDER_A		GRB

	// Second output settings.
//	#define DATA_PIN_B		3	// Uncomment to use the second output. NO PARALLEL OUTPUT
	#define LED_TYPE_B		WS2812B	// Only used if DATA_PIN_B is defined
	#define COLOR_ORDER_B		GRB	// Only used if DATA_PIN_B is defined
	#define NUM_LEDS_STRIP_A	50	// Only used if DATA_PIN_B is defined
	#define NUM_LEDS_STRIP_B	50	// Only used if DATA_PIN_B is defined
#endif