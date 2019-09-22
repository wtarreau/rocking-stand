#include <stdint.h>
#include <stdio.h>

/* returns ((x*x) >> 7) + 128 using exclusively 8 bit ops.
 * AVR: -Os => ~0x30 bytes
 *      -O3 => ~0x118 bytes
 */
uint8_t sq8_bit(uint8_t x)
{
	uint8_t count = 8;
	uint8_t out = 0;
	uint8_t cf;
	uint8_t w;

	w = ~x;
	cf = 0;
	do {
		out = (cf << 7) + (out >> 1);
		if (x & 1) {
			out += w;
			cf = (out < w);
		}
		x >>= 1;
	} while (--count);
	return out + 128;
}

/* returns ((x*x) >> 7) + 128 using exclusively a 8*8=>16 bit signed
 * multiply. AVR: ~0x18 bytes + call to mulhi3().
 */
uint8_t sq8_imul(signed char x)
{
	x += 128;
	x = ((int16_t)x * x) >> 7;
	return ~x;
}

#define sq8 sq8_imul

/* returns sin(angle*pi/128)*128+128, which is a sine centered around 128. */
uint8_t sinu8(uint8_t angle)
{
	if (angle & 128)
		return ~sq8(angle << 1);
	else
		return sq8(angle << 1);
}

/* returns sin(angle*pi/128)*128, which is a sine centered around 0. */
int8_t sin8(uint8_t angle)
{
	uint8_t ret = sq8(angle << 1);

	if (angle & 128)
		ret = ~ret;
	return ret + 128;
}

int main()
{
	int x, i;
	uint16_t y;
	uint8_t z;
	uint8_t outh, angle, w;
	uint8_t cf, cfn;

	for (x = 0; x < 256; x ++) {
		z = sin8(x);
		// now z = 0..255, centered around 128
		printf("%3d %3d   ", x, z);

		for (i = 0; i < 1 + z / 4; i++)
			putchar('#');
		putchar('\n');
	}
}
