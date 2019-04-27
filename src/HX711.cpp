#include <stdint.h>

#include "HX711.h"
#include "mgos_gpio.h"
#include "mgos_system.h"

HX711::HX711(uint8_t dout, uint8_t pd_sck, uint8_t gain) {
	begin(dout, pd_sck, gain);
}


void HX711::begin(uint8_t dout, uint8_t pd_sck, uint8_t gain) {
	PD_SCK = pd_sck;
	DOUT = dout;

	mgos_gpio_set_mode(PD_SCK, MGOS_GPIO_MODE_OUTPUT);
	mgos_gpio_set_mode(DOUT, MGOS_GPIO_MODE_INPUT);
	set_gain(gain);
}

bool HX711::is_ready() {
	return mgos_gpio_read(DOUT) == false;
}

void HX711::set_gain(uint8_t gain) {
	switch (gain) {
		case 128:		// channel A, gain factor 128
			GAIN = 1;
			break;
		case 64:		// channel A, gain factor 64
			GAIN = 3;
			break;
		case 32:		// channel B, gain factor 32
			GAIN = 2;
			break;
	}

	mgos_gpio_write(PD_SCK, false);
	read();
}

int HX711::shiftIn(int dataPin, int clockPin, int bitOrder) {

 uint8_t value = 0;
 uint8_t i;

 for (i = 0; i < 8; ++i) {
	mgos_gpio_write(clockPin, true);
	mgos_usleep(1); //needed to meet chip spec
	 if (bitOrder == 0)
		value |= mgos_gpio_read(dataPin) << i;
	 else
		value |= mgos_gpio_read(dataPin) << (7 - i);

	 mgos_gpio_write(clockPin, false);
	 mgos_usleep(1);//needed to meet chip spec
 }
 return value;
}

int32_t HX711::read() {
	// wait for the chip to become ready
	while (!is_ready()) {
		// Will do nothing on Arduino but prevent resets of ESP8266 (Watchdog Issue)
		mgos_usleep(0);
		// yield();
	}

	uint32_t value = 0;
	uint8_t data[3] = { 0 };
	uint8_t filler = 0x00;
	
	mgos_ints_disable();
	
//rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
	// pulse the clock pin 24 times to read the data
	data[2] = shiftIn(DOUT, PD_SCK, 1);
	data[1] = shiftIn(DOUT, PD_SCK, 1);
	data[0] = shiftIn(DOUT, PD_SCK, 1);
//	rtc_clk_cpu_freq_set(RTC_CPU_FREQ_240M);

	// set the channel and the gain factor for the next reading using the clock pin
	for (unsigned int i = 0; i < GAIN; i++) {
		mgos_gpio_write(PD_SCK, true);
		mgos_gpio_write(PD_SCK, false);
	}
	
	mgos_ints_enable();

	// Replicate the most significant bit to pad out a 32-bit signed integer
	if (data[2] & 0x80) {
		filler = 0xFF;
	} else {
		filler = 0x00;
	}

	// Construct a 32-bit signed integer
	value = ( static_cast<unsigned int32_t>(filler) << 24
			| static_cast<unsigned int32_t>(data[2]) << 16
			| static_cast<unsigned int32_t>(data[1]) << 8
			| static_cast<unsigned int32_t>(data[0]) );

	return static_cast<int32_t>(value);
}

int32_t HX711::read_average(uint8_t times) {
	int32_t sum = 0;
	for (uint8_t i = 0; i < times; i++) {
		sum += read();
		mgos_usleep(0); 
		//yield();
	}
	return sum / times;
}

double HX711::get_value(uint8_t times) {
	return read_average(times) - OFFSET;
}

float HX711::get_units(uint8_t times) {
	return get_value(times) / SCALE;
}

void HX711::tare(uint8_t times) {
	double sum = read_average(times);
	set_offset(sum);
}

void HX711::set_scale(float scale) {
	SCALE = scale;
}

float HX711::get_scale() {
	return SCALE;
}

void HX711::set_offset(int32_t offset) {
	OFFSET = offset;
}

int32_t HX711::get_offset() {
	return OFFSET;
}

void HX711::power_down() {
	mgos_gpio_write(PD_SCK, false);
	mgos_gpio_write(PD_SCK, true);
}

void HX711::power_up() {
	mgos_gpio_write(PD_SCK, false);
}
