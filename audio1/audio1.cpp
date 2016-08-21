#include "Particle.h"

// Requires the SparkIntervalTimer library
// https://github.com/pkourany/SparkIntervalTimer
#include "SparkIntervalTimer/SparkIntervalTimer.h"

// Tested with Adafruit 1713
// Electret Microphone Amplifier - MAX9814 with Auto Gain Control
// https://www.adafruit.com/products/1713
//
// AR   - No connection
// Out  - Audio out (analog) to Photon A0
// Gain - No connection
// VDD  - 3V3
// GND  - GND

void buttonHandler(system_event_t event, int data); // forward declaration
void timerISR(); // forward declaration

// 1024 is a good size for this buffer. The practical range is 512 to 2048 bytes; out of that range
// the network writes can get flaky
const size_t SAMPLE_BUF_SIZE = 1024;

// This must be >= 2. If you set it to more, then it will handle hiccups in the networking. Otherwise,
// the samples are just discarded.
const size_t NUM_BUFS = 2;

// This is the pin the microphone is connected to.
const int SAMPLE_PIN = A0;

// The audio sample rate. The minimum is probably 8000 for minimally acceptable audio quality, and
// a maximum of 48000 because of timer ISR overhead.
const long SAMPLE_RATE = 16000;

// If you don't hit the setup button to stop recording, this is how long to go before turning it
// off automatically. The limit really is only the disk space available to receive the file.
const unsigned long MAX_RECORDING_LENGTH_MS = 30000;

// This is the IP Address and port that the audioServer.js node server is running on.
IPAddress serverAddr = IPAddress(192,168,2,4);
int serverPort = 7123;

// Structure to hold the data
typedef struct {
	volatile bool free;
	volatile size_t  index;
	uint8_t data[SAMPLE_BUF_SIZE];
} SampleBuf;

IntervalTimer timer;
SampleBuf sampleBufs[NUM_BUFS];
volatile size_t sampleIndex = 0;
volatile size_t sendIndex = 0;

TCPClient client;
unsigned long recordingStart;

enum State { STATE_WAITING, STATE_CONNECT, STATE_RUNNING, STATE_FINISH };
State state = STATE_WAITING;

void setup() {
	Serial.begin(9600);

	// This determines the number of samples to average to get a single sample
	setADCSampleTime(ADC_SampleTime_3Cycles);

	// Register handler to handle clicking on the SETUP button
	System.on(button_click, buttonHandler);
	pinMode(D7, OUTPUT);

}

void loop() {
	switch(state) {
	case STATE_WAITING:
		// Waiting for the user to press the SETUP button. The setup button handler
		// will bump the state into STATE_CONNECT
		break;

	case STATE_CONNECT:
		// Ready to connect to the server via TCP
		if (client.connect(serverAddr, serverPort)) {
			// Connected

			// Reset the buffers
			for(size_t ii = 0; ii < NUM_BUFS; ii++) {
				sampleBufs[ii].free = true;
				sampleBufs[ii].index = 0;
			}
			sampleIndex = 0;
			sendIndex = 0;

			// We want to sample at 16 KHz
			// 16000 samples/sec = 62.5 microseconds
			// The minimum timer period is about 10 micrseconds
			timer.begin(timerISR, 1000000 / SAMPLE_RATE, uSec);

			recordingStart = millis();
			digitalWrite(D7, HIGH);

			state = STATE_RUNNING;
		}
		else {
			Serial.println("failed to connect to server");
			state = STATE_WAITING;
		}
		break;

	case STATE_RUNNING:
		if (sendIndex < sampleIndex) {
			// There is a sample buffer to send
			SampleBuf *sb = &sampleBufs[sendIndex % NUM_BUFS];

			// Send here
			int count = client.write(sb->data, SAMPLE_BUF_SIZE);
			if (count == SAMPLE_BUF_SIZE) {
				// Success
				// Mark the buffer as sent so it can be reused
				sb->free = true;
				sendIndex++;
			}
			else
			if (count == -16) {
				// Buffer full, retry next time
			}
			else {
				// Error
				Serial.printlnf("error writing %d", count);
				state = STATE_FINISH;
			}
		}
		if (millis() - recordingStart >= MAX_RECORDING_LENGTH_MS) {
			state = STATE_FINISH;
		}
		break;

	case STATE_FINISH:
		digitalWrite(D7, LOW);
		timer.end();
		client.stop();
		state = STATE_WAITING;
		break;
	}
}

void timerISR() {
	// This is an interrupt service routine. Don't put any heavy calculations here
	// or call anything that's not interrupt-safe, such as:
	// Serial, String, any memory allocation (new, malloc, etc.), Particle.publish and other Particle methods, and more.

	SampleBuf *sb = &sampleBufs[sampleIndex % NUM_BUFS];

	if (!sb->free) {
		// The network has fallen behind in sending so discard samples
		return;
	}

	// Convert 12-bit sample to 8-bit
	sb->data[sb->index++] = (uint8_t) (analogRead(SAMPLE_PIN) >> 4);

	if (sb->index >= SAMPLE_BUF_SIZE) {
		// Buffer has been filled. Let it be sent via TCP.
		sb->free = false;
		sb->index = 0;
		sampleIndex++;
	}
}

// button handler for the SETUP button, used to toggle recording on and off
void buttonHandler(system_event_t event, int data) {
	switch(state) {
	case STATE_WAITING:
		state = STATE_CONNECT;
		break;

	case STATE_RUNNING:
		state = STATE_FINISH;
		break;
	}
}
