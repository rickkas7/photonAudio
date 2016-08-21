#include "Particle.h"


// Tested with Adafruit 1713
// Electret Microphone Amplifier - MAX9814 with Auto Gain Control
// https://www.adafruit.com/products/1713
//
// AR   - No connection
// Out  - Audio out (analog) to Photon A0
// Gain - No connection
// VDD  - 3V3
// GND  - GND

//
// ADCDMA - Class to use Photon ADC in DMA Mode
//
#include "adc_hal.h"
#include "gpio_hal.h"
#include "pinmap_hal.h"
#include "pinmap_impl.h"


void buttonHandler(system_event_t event, int data); // forward declaration

// 2048 is a good size for this buffer. This is the number of samples; the number of bytes is twice
// this, but only half the buffer is sent a time, and then each pair of samples is averaged, so
// this results in 1024 byte writes from TCP, which is optimal.
// This uses 4096 bytes of RAM, which is also reasonable.
const size_t SAMPLE_BUF_SIZE = 2048;

// This is the pin the microphone is connected to.
const int SAMPLE_PIN = A0;

// The audio sample rate. The minimum is probably 8000 for minimally acceptable audio quality, and
// a maximum of 48000 because of timer ISR overhead.
const long SAMPLE_RATE = 32000;

// If you don't hit the setup button to stop recording, this is how long to go before turning it
// off automatically. The limit really is only the disk space available to receive the file.
const unsigned long MAX_RECORDING_LENGTH_MS = 30000;

// This is the IP Address and port that the audioServer.js node server is running on.
IPAddress serverAddr = IPAddress(192,168,2,4);
int serverPort = 7123;


uint16_t samples[SAMPLE_BUF_SIZE];

TCPClient client;
unsigned long recordingStart;

enum State { STATE_WAITING, STATE_CONNECT, STATE_RUNNING, STATE_FINISH };
State state = STATE_WAITING;

//
//
//
class ADCDMA {
public:
	ADCDMA(int pin, uint16_t *buf, size_t bufSize);
	virtual ~ADCDMA();

	void start(size_t freqHZ);
	void stop();

private:
	int pin;
	uint16_t *buf;
	size_t bufSize;
};

// Helpful post:
// https://my.st.com/public/STe2ecommunities/mcu/Lists/cortex_mx_stm32/Flat.aspx?RootFolder=https%3a%2f%2fmy%2est%2ecom%2fpublic%2fSTe2ecommunities%2fmcu%2fLists%2fcortex%5fmx%5fstm32%2fstm32f207%20ADC%2bTIMER%2bDMA%20%20Poor%20Peripheral%20Library%20Examples&FolderCTID=0x01200200770978C69A1141439FE559EB459D7580009C4E14902C3CDE46A77F0FFD06506F5B&currentviews=6249

ADCDMA::ADCDMA(int pin, uint16_t *buf, size_t bufSize) : pin(pin), buf(buf), bufSize(bufSize) {
}

ADCDMA::~ADCDMA() {

}

void ADCDMA::start(size_t freqHZ) {

    // Using Dual ADC Regular Simultaneous DMA Mode 1

	// Using Timer3. To change timers, make sure you edit all of:
	// RCC_APB1Periph_TIM3, TIM3, ADC_ExternalTrigConv_T3_TRGO

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	// Set the pin as analog input
	// GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	// GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    HAL_Pin_Mode(pin, AN_INPUT);

	// Enable the DMA Stream IRQ Channel
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	// 60000000UL = 60 MHz Timer Clock = HCLK / 2
	// Even low audio rates like 8000 Hz will fit in a 16-bit counter with no prescaler (period = 7500)
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	TIM_TimeBaseStructure.TIM_Period = (60000000UL / freqHZ) - 1;
	TIM_TimeBaseStructure.TIM_Prescaler = 0;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
	TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update); // ADC_ExternalTrigConv_T3_TRGO
	TIM_Cmd(TIM3, ENABLE);

	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	DMA_InitTypeDef DMA_InitStructure;

	// DMA2 Stream0 channel0 configuration
	DMA_InitStructure.DMA_Channel = DMA_Channel_0;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)buf;
	DMA_InitStructure.DMA_PeripheralBaseAddr =  0x40012308; // CDR_ADDRESS; Packed ADC1, ADC2;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMA_InitStructure.DMA_BufferSize = bufSize;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Enable;
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(DMA2_Stream0, &DMA_InitStructure);

	// Don't enable DMA Stream Half / Transfer Complete interrupt
	// Since we went out of loop anyway, there's no real advantage to using the interrupt, and as
	// far as I can tell, you can't set the interrupt handler for DMA2_Stream0 without modifying
	// system firmware because there's no built-in handler for it.
	// DMA_ITConfig(DMA2_Stream0, DMA_IT_TC | DMA_IT_HT, ENABLE);

	DMA_Cmd(DMA2_Stream0, ENABLE);

	// ADC Common Init
	ADC_CommonInitStructure.ADC_Mode = ADC_DualMode_RegSimult;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div2;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_1;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&ADC_CommonInitStructure);

	// ADC1 configuration
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_Rising;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T3_TRGO;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Left;
	ADC_InitStructure.ADC_NbrOfConversion = 1;
	ADC_Init(ADC1, &ADC_InitStructure);

	// ADC2 configuration - same
	ADC_Init(ADC2, &ADC_InitStructure);

	//
	ADC_RegularChannelConfig(ADC1, PIN_MAP[pin].adc_channel, 1, ADC_SampleTime_15Cycles);
    ADC_RegularChannelConfig(ADC2, PIN_MAP[pin].adc_channel, 1, ADC_SampleTime_15Cycles);

	// Enable DMA request after last transfer (Multi-ADC mode)
	ADC_MultiModeDMARequestAfterLastTransferCmd(ENABLE);

	// Enable ADCs
	ADC_Cmd(ADC1, ENABLE);
	ADC_Cmd(ADC2, ENABLE);

	ADC_SoftwareStartConv(ADC1);
}

void ADCDMA::stop() {
	// Stop the ADC
	ADC_Cmd(ADC1, DISABLE);
	ADC_Cmd(ADC2, DISABLE);

	DMA_Cmd(DMA2_Stream0, DISABLE);

	// Stop the timer
	TIM_Cmd(TIM3, DISABLE);
}

ADCDMA adcDMA(A0, samples, SAMPLE_BUF_SIZE);

// End ADCDMA


void setup() {
	Serial.begin(9600);

	// This determines the number of samples to average to get a single sample
	setADCSampleTime(ADC_SampleTime_3Cycles);

	// Register handler to handle clicking on the SETUP button
	System.on(button_click, buttonHandler);
	pinMode(D7, OUTPUT);

}

void loop() {
	uint16_t *sendBuf = NULL;

	switch(state) {
	case STATE_WAITING:
		// Waiting for the user to press the SETUP button. The setup button handler
		// will bump the state into STATE_CONNECT
		break;

	case STATE_CONNECT:
		// Ready to connect to the server via TCP
		if (client.connect(serverAddr, serverPort)) {
			// Connected
			adcDMA.start(SAMPLE_RATE);

			// We want to sample at 16 KHz
			// 16000 samples/sec = 62.5 microseconds
			// The minimum timer period is about 10 micrseconds
			// timer.begin(timerISR, 1000000 / SAMPLE_RATE, uSec);
			Serial.println("starting");

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
		if (DMA_GetFlagStatus(DMA2_Stream0, DMA_FLAG_HTIF0)) {
		    DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_HTIF0);
		    sendBuf = samples;
		}
		if (DMA_GetFlagStatus(DMA2_Stream0, DMA_FLAG_TCIF0)) {
		    DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_TCIF0);
		    sendBuf = &samples[SAMPLE_BUF_SIZE / 2];
		}

		if (sendBuf != NULL) {
			// There is a sample buffer to send

			// Average the pairs of samples
			for(size_t ii = 0, jj = 0; ii < SAMPLE_BUF_SIZE / 2; ii += 2, jj++) {
				uint32_t sum = (uint32_t)sendBuf[ii] + (uint32_t)sendBuf[ii + 1];
				sendBuf[jj] = (uint16_t)(sum / 2);
			}

			// Send here. We're actually sending 1/4 of the samples here, 1/2 of the
			// samples buffer, and we've averaged each pair of samples, but the samples
			// are 16 bits and client.write() takes bytes, so only / 2 here
			int count = client.write((uint8_t *)sendBuf, SAMPLE_BUF_SIZE / 2);
			if (count == SAMPLE_BUF_SIZE / 2) {
				// Success
			}
			else
			if (count == -16) {
				// TCP Buffer full
				Serial.printlnf("buffer full, discarding");
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
		adcDMA.stop();
		client.stop();
		Serial.println("stopping");
		state = STATE_WAITING;
		break;
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
