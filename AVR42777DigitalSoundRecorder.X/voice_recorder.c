/*
 * voice_recorder.c
 *
 * Created: 27-Jun-16 3:28:58 PM
 *  Author: elizabeth.roy
 */

/*
 * SD_card_test.c
 *
 * Created: 02-Jun-16 9:18:01 AM
 * Author : elizabeth.roy
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "sdcard_raw.h"

/* LED0 = PB7 */
#define LEDPORT PORTB
#define LED0PIN (1 << 7)
#define LED_OFF() (LEDPORT.OUTSET = LED0PIN)
#define LED_ON() (LEDPORT.OUTCLR = LED0PIN)
#define LED_IS_ON() (!(LEDPORT.OUT & LED0PIN))

/* OLED1 Xplained Pro Buttons */
#define RECORD_PORT PORTB
#define RECORD_PIN (1 << 3) /* PB3 */
#define ERASE_PORT PORTB
#define ERASE_PIN (1 << 6) /* PB6 */
#define PLAYBACK_PORT PORTA
#define PLAYBACK_PIN (1 << 3) /* PA3 */

/* ADC input PA7 (channel 7) - input from microphone */
#define MICPORT PORTA
#define MICPIN (1 << 7)
#define ADC_INPUT ADC_MUXPOS_AIN7_gc

/* Sampling timer */
#define SAMPLE_TIMER TCB0
#define SAMPLE_FREQ 8000 /* Hz */
#define SAMPLE_TIMER_START() SAMPLE_TIMER.CTRLA |= TCB_ENABLE_bm
#define SAMPLE_TIMER_STOP() SAMPLE_TIMER.CTRLA &= ~(TCB_ENABLE_bm)

/* DAC output pin PA6 - output to speaker */
#define SPEAKPORT PORTA
#define SPEAKPIN (1 << 6)

#define BUFFER_SIZE 128
#define START_SECTOR 10

uint8_t           buffer0[BUFFER_SIZE], buffer1[BUFFER_SIZE];
uint8_t *         buffers[]     = {buffer0, buffer1};
volatile uint8_t  active_buffer = 0, alt_buffer = 1;
volatile uint8_t  buffer_index = 0, buffer_end = 0;
volatile uint32_t last_write_addr = 0;

void record(void);
void erase(void);
void playback(void);
void IO_init(void);
void sample_timer_init(void);
void mic_hw_init(void);
void speaker_hw_init(void);
void old_code(void);

int main(void)
{
	DSTATUS status;

	/* Set CPU clock to 10MHz */
	_PROTECTED_WRITE(CLKCTRL_MCLKCTRLB, (CLKCTRL_PEN_bm | CLKCTRL_PDIV_2X_gc));

	/* Initialize SD card */
	uint8_t i = 10;
	do {
		status = disk_initialize();
	} while ((status != 0) && --i);

	/* Initialize peripherals */
	IO_init();
	sample_timer_init();
	mic_hw_init();
	speaker_hw_init();

	/* Enable global interrupts */
	sei();

	while (1) {
		while (LED_IS_ON()) {
			status = disk_initialize();
			if (status == 0)
				LED_OFF();
		}
		if (!(RECORD_PORT.IN & RECORD_PIN)) {
			record();
		}
		if (!(ERASE_PORT.IN & ERASE_PIN)) {
			erase();
		}
		if (!(PLAYBACK_PORT.IN & PLAYBACK_PIN)) {
			playback();
		}
	}
}

/* Part of RECORD routine - Grab result from ADC recording */
ISR(ADC0_RESRDY_vect)
{
	/* Interrupt flag cleared on reading of ADC result register */
	*(buffers[active_buffer] + buffer_index++) = ADC0.RES;

	if (buffer_index == BUFFER_SIZE) {
		/* Reset buffer index */
		buffer_index = 0;
		/* Change active buffer */
		active_buffer ^= 1;
		alt_buffer ^= 1;
		/* Buffer full flag */
		buffer_end = 0x01;
	}
}

void record(void)
{
	DRESULT  result;
	uint16_t bytes_written;
	uint32_t offset = 0;

	// if(last_write_addr != 0) erase();
	last_write_addr = 0;

	result = disk_write_start(START_SECTOR);
	if (result != RES_OK)
		LED_ON();

	buffer_index = 0;

	/* Enable event forwarding to ADC */
	EVSYS.ASYNCUSER1 = EVSYS_ASYNCUSER1_SYNCCH0_gc;
	SAMPLE_TIMER_START();

	do {
		bytes_written = BUFFER_SIZE;

		if (buffer_end == 0x01) {
			/* Write bytes */
			result = disk_write_continue(buffers[alt_buffer], BUFFER_SIZE);
			if (result != RES_OK)
				LED_ON();
			offset += BUFFER_SIZE;
			buffer_end = 0;
		}

		/* While button is pushed and end of file is not reached */
	} while ((!(RECORD_PORT.IN & RECORD_PIN)) && (bytes_written == BUFFER_SIZE) && (!(LED_IS_ON())));

	SAMPLE_TIMER_STOP();
	/* Stop event forwarding to ADC */
	EVSYS.ASYNCUSER1 = 0;

	/* Write buffer finished on */
	if (bytes_written == BUFFER_SIZE) {
		disk_write_continue(buffers[active_buffer], BUFFER_SIZE);
		offset += BUFFER_SIZE;
	}

	/* Save last written address */
	last_write_addr = offset;

	/* Finalize write */
	result = disk_write_stop();
	if (result != RES_OK)
		LED_ON();
}

void erase(void)
{
	DRESULT  result;
	uint32_t offset = last_write_addr;

	if (offset) {

		result = disk_write_start(START_SECTOR);
		if (result != RES_OK)
			LED_ON();

		/* Set a buffer to all zeros */
		for (buffer_index = 0; buffer_index < BUFFER_SIZE; buffer_index++) {
			buffer0[buffer_index] = 0;
		}

		do {
			result = disk_write_continue(buffer0, BUFFER_SIZE);
			if (result != RES_OK)
				LED_ON();
			offset -= BUFFER_SIZE;
		} while (offset);

		/* Finalize write */
		disk_write_stop();
	}
}

/* Part of PLAYBACK routine - Update DAC output */
ISR(TCB0_INT_vect)
{
	/* Load next data into DAC */
	DAC0.DATA = *(buffers[active_buffer] + buffer_index++);

	if (buffer_index == BUFFER_SIZE) {
		buffer_index = 0;
		/* Switch to other buffer */
		active_buffer ^= 1;
		alt_buffer ^= 1;
		/* Flag for new data to be loaded into finished buffer */
		buffer_end = 0x01;
	}

	/* Clear interrupt flag */
	SAMPLE_TIMER.INTFLAGS = 0xFF;
}

void playback(void)
{
	DRESULT  result;
	uint32_t offset = last_write_addr;
	buffer_index    = 0;

	result = disk_read_start(START_SECTOR);
	if (result != RES_OK)
		LED_ON();

	/* Read first data */
	disk_read_continue(buffers[active_buffer], BUFFER_SIZE);
	offset -= BUFFER_SIZE;
	disk_read_continue(buffers[alt_buffer], BUFFER_SIZE);
	offset -= BUFFER_SIZE;

	buffer_index = 0;

	/* Enable DAC */
	DAC0.CTRLA |= DAC_ENABLE_bm;

	/* Enable sample timer overflow ISR and start timer */
	SAMPLE_TIMER.INTCTRL = TCB_CAPT_bm;
	SAMPLE_TIMER_START();

	do {
		if (buffer_end == 0x01) {
			result = disk_read_continue(buffers[alt_buffer], BUFFER_SIZE);
			if (result != RES_OK)
				LED_ON();
			offset -= BUFFER_SIZE;
			buffer_end = 0;
		}
	} while ((!(PLAYBACK_PORT.IN & PLAYBACK_PIN)) && (offset) && (!(LED_IS_ON())));

	/* Stop timer and disable sample timer overflow ISR */
	SAMPLE_TIMER_STOP();
	SAMPLE_TIMER.INTCTRL = 0;

	/* Disable DAC */
	DAC0.CTRLA &= ~(DAC_ENABLE_bm);

	/* Finalize read */
	result = disk_read_stop();
	if (result != RES_OK)
		LED_ON();
}

/* Setup Functions */
void IO_init(void)
{
	/* LED */
	LEDPORT.DIRSET = LED0PIN;
	LED_OFF();

	/* Buttons */
	RECORD_PORT.DIRCLR     = RECORD_PIN;
	RECORD_PORT.PIN3CTRL   = PORT_PULLUPEN_bm;
	ERASE_PORT.DIRCLR      = ERASE_PIN;
	ERASE_PORT.PIN6CTRL    = PORT_PULLUPEN_bm;
	PLAYBACK_PORT.DIRCLR   = PLAYBACK_PIN;
	PLAYBACK_PORT.PIN3CTRL = PORT_PULLUPEN_bm;
}

void sample_timer_init(void)
{
	/** Timer configured to overflow at 15686Hz **/

	/* Event input to SYNCCH0 from TCB0 */
	EVSYS.SYNCCH0 = EVSYS_SYNCCH0_TCB0_gc;

	/* Count val = F_CPU / (SAMPLE_FREQ * 2 (ovf pr period) * PRESC_DIV) - 1 */
	uint16_t period   = (F_CPU / 2) / (SAMPLE_FREQ * 2 * 1) - 1;
	SAMPLE_TIMER.CCMP = period;
	/* Clock prescaler */
	SAMPLE_TIMER.CTRLA = TCB_CLKSEL_CLKDIV1_gc;

	/* Start to record, enable interrupt and start to playback */
}

void mic_hw_init(void)
{
	/* ADC input */
	MICPORT.DIRCLR = MICPIN;
	/* 8 bit resolution */
	ADC0.CTRLA = ADC_RESSEL_bm;
	/* Reference 2.5V */
	VREF.CTRLA |= VREF_ADC0REFSEL_2V5_gc;
	/* Prescaler DIV2 */
	ADC0.CTRLC = ADC_PRESC_DIV2_gc;
	/* ADC input 7 (PA7) */
	ADC0.MUXPOS = ADC_INPUT;
	/* Enable */
	ADC0.CTRLA |= ADC_ENABLE_bm;
	/* Start Event Input enable (start conversion on event input) */
	ADC0.EVCTRL = ADC_STARTEI_bm;
	/* Clear interrupt flags */
	ADC0.INTFLAGS = 0xFF;
	/* Enable ADC result ready interrupt */
	ADC0.INTCTRL = ADC_RESRDY_bm;

	/* Ready for enabling event forwarding to ADC and start timer to
	 * start conversions
	 */
}

void speaker_hw_init(void)
{
	/* DAC output pin */
	SPEAKPORT.DIRSET = SPEAKPIN;
	/* DAC reference 2.5V */
	VREF.CTRLA |= VREF_DAC0REFSEL_2V5_gc;
	/* Enable DAC output to pin */
	DAC0.CTRLA = DAC_OUTEN_bm;

	/* Ready to load first data and enable */
}
