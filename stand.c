/* stand.c - 2019/07/28 - Willy Tarreau - public domain
 *
 * This is a C AVR re-implementation of my previous rocking stand for PIC
 * that was written in 12F675 assembly. It implements a slow sine wave whose
 * amplitude, offset and frequency are controlled by 3 external potentiometers,
 * and converts it into a pulse length to drive a servo-motor. A sine wave was
 * found to be critical to avoid splashes when changing direction. The offset
 * is used to fix the center and to allow to repurpose old servos with broken
 * gears which can only operate on limited range.
 *
 * The code is written in Arduino style using setup() and loop() so that it is
 * easily ported to other environments. But it has no external dependency. It's
 * about 492 bytes on ATtiny13 and 506 on ATtiny85.
 */

/* Internal frequency, 9.6 MHz for Tiny13A, 8.0 MHz for 25/45/85 */
#ifndef F_CPU
#error "Must set F_CPU to the CPU's frequency in Hz"
#endif

#include <avr/io.h>
#include <stdint.h>

/* current angle in the highest 8 bits */
uint16_t angle = 0;

/* returns ((x*x) >> 7) + 128 using exclusively a 8*8=>16 bit signed
 * multiply. AVR: ~0x18 bytes + call to mulhi3().
 */
uint8_t sq8(int8_t x)
{
	x += 128;
	x = ((int16_t)x * x) >> 7;
	return ~x;
}

/* returns sin(angle*pi/128)*128, which is a sine centered around 0.
 * It works by approximating two paraboles. sin(x) follows x^2 and
 * 1-(x^2) very closely (6% error max) so these are sufficient.
 */
int8_t sin8(uint8_t angle)
{
	uint8_t ret = sq8(angle << 1);

	if (angle & 128)
		ret = ~ret;
	return ret + 128;
}

/* Convert microseconds to loops around an empty asm statement. Each loop was
 * measured to take 5 cycles on gcc 5.4 (vs 4 on 4.3)! This is limited to 64k
 * loops, or ~20ms at 8 MHz, 17ms at 9.6 MHz.
 */
static inline uint16_t us2loops(uint16_t us)
{
	return (uint32_t)us * (F_CPU / 100000) / 50 + 1;
}

/* Sleeps this number of microseconds. Limited ~32ms at 8 MHz, 27ms
 * at 9.6 MHz. May overflow on 8 MHz CPUs above 8 milliseconds at once.
 */
static inline void delay_us(uint16_t us)
{
	us = us2loops(us);
	while (--us)
		asm volatile("");
}

/* send a positive pulse of <width> microseconds on port B<port> */
static inline void send_pulse(uint8_t port, uint16_t width)
{
	width = us2loops(width);
	PORTB |= _BV(port);
	while (--width)
		asm volatile("");
	PORTB &= ~_BV(port);
}

/* configure the ADC channel between 0 to 3 */
static inline void set_adc_channel(const uint8_t ch)
{
	switch (ch) {
	case 0:	ADMUX = (ADMUX & ~(_BV(MUX1) | _BV(MUX0))) | (0         | 0);
		break;
	case 1:	ADMUX = (ADMUX & ~(_BV(MUX1) | _BV(MUX0))) | (0         | _BV(MUX0));
		break;
	case 2:	ADMUX = (ADMUX & ~(_BV(MUX1) | _BV(MUX0))) | (_BV(MUX1) | 0);
		break;
	case 3:	ADMUX = (ADMUX & ~(_BV(MUX1) | _BV(MUX0))) | (_BV(MUX1) | _BV(MUX0));
		break;
	}
}

/* starts the ADC conversion */
static inline void start_adc(void)
{
	ADCSRA |= _BV(ADSC);
}

/* reads the highest 8 bits of the ADC once ready */
static inline uint8_t read_adch(void)
{
	while (ADCSRA & _BV(ADSC))
		;
	/* result left aligned (8-bit in ADCH) => ADLAR = 1 */
	ADMUX |= _BV(ADLAR);
	return ADCH;
}

/* reads the 10 bits of the ADC once ready */
static inline uint16_t read_adc(void)
{
	while (ADCSRA & _BV(ADSC))
		;
	/* result right aligned (10-bit in ADCH:ADCL) => ADLAR = 0 */
	ADMUX &= ~_BV(ADLAR);
	return ADCL + ((uint16_t)ADCH << 8);
}

/* The loop is pretty simple. It runs for roughly 20 ms, measures the 3
 * potentiometers, computes the pulse length and position and sends it to
 * port PB0. Since the ADC conversion takes some time, we include it
 * in the pause delay. Thus we start a conversion on a port, wait 6 ms,
 * wait for the conversion to finish, start another one etc. This totals
 * 18 ms, to which we add roughly 1.5ms.
 */
void loop(void)
{
	uint16_t p1; // Potentiometer 1 : offset
	uint16_t p2; // Potentiometer 2 : speed
	uint8_t  p3; // Potentiometer 3 : amplitude
	uint16_t pw; // pulse_width

	p1 = 0; p2 = 0; p3 = 0;

	/* start conversion for ADC1, wait 6 ms and read it */
	set_adc_channel(1);
	start_adc();
	delay_us(6000);
	p1 = read_adc();

	/* start conversion for ADC2, wait 6 ms and read it */
	set_adc_channel(2);
	start_adc();
	delay_us(6000);
	p2 = read_adc();

	/* start conversion for ADC3, wait 6 ms and read it */
	set_adc_channel(3);
	start_adc();
	delay_us(6000);
	p3 = read_adch();

	/* note: ~18 ms elapsed here */

	/* the amplitude is 0..255. We multiply it by 4/256 before applying it
	 * to the signed sine (-128..127) so that we have a -510..508 range
	 * around 1500us. We then add the offset between -1024..+1022
	 */
	pw =	1500 + // center
		((int16_t)(p1 - 512) << 1) + // offset: -1024..+1022
		(((int16_t)sin8(angle >> 8) * p3) >> 6); // current angle

	/* we roughly perform this increment 50 times a second. This means that
	 * a complete cycle will be done ~3 times per second this way at the
	 * fastest speed.
	 */
	angle += (uint16_t)p2 << 2;
	send_pulse(PB0, pw);

	/* now we're around 18..22 ms */
}

void setup(void)
{
	/* PB#  pin  dir  role
	 * PB0   5   out  servo
	 * PB1   6    -   unused
	 * PB2   7    in  ADC1 - offset
	 * PB3   2    in  ADC3 - amplitude
	 * PB4   3    in  ADC2 - speed
	 * PB5   1    -   RESET
	 */

	/* set output pins */
	DDRB = _BV(DDB0);

	/* ADC sampling rate : clk/8 => ADPS{2,1,0} = 011 */
	/* ADEN=1 to enable ADC */
	ADCSRA = _BV(ADEN) | _BV(ADPS1) | _BV(ADPS0);

	/* ADC reference : Vcc => REFS0 = 0 */
	ADMUX = 0;
}

int main(void)
{
	setup();
#if TEST
	/* must produce exactly 1 KHz signal on PB0 */
	while (1) {
		delay_us(500);
		PORTB ^= _BV(PB0);
	}
#else
	while (1)
		loop();
#endif
}
