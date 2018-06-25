// Example minimal profile for use with CC4P 

#define			CC4P	// ESP-12 based with MSGEQ7 and 5 button D pad and 4 way parallel output

#define DEVICE_NAME 	"ExampleCC4Profile"

//#define PARALLEL_OUTPUT		// Uncommented = Parallel output on WS2813_PORTA. Commented = Sequential output on pins 12,13,14,15
#define PARALLEL_LED_TYPE	WS2813_PORTA
#define SEQUENTIAL_LED_TYPE	WS2813
#define NUM_LEDS_PER_STRIP	150
#define NUM_STRIPS		4

#define MATRIX_WIDTH	24	// Column count
#define MATRIX_HEIGHT	25	// Row count


