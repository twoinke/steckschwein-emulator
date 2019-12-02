// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD

#include <stdio.h>
#include <stdbool.h>
#include "spi.h"
#include "sdcard.h"
#include "via.h"

// VIA#2
// PB0 SPICLK
// PB1 SS1 SDCARD
// PB2 SS2 KEYBOARD
// PB3 SS3 RTC
// PB4 unassigned
// PB5 SD write protect
// PB6 SD detect
// PB7 MOSI
// CB1 SPICLK (=PB0)
// CB2 MISO

static bool initialized;

void spi_init() {
	initialized = false;
}

#define SPI_DEV_SDCARD	0b1100
#define SPI_DEV_KEYBRD	0b1010
#define SPI_DEV_RTC		0b0110
#define SPI_DESELECT	(SPI_DEV_SDCARD | SPI_DEV_KEYBRD | SPI_DEV_RTC)

void dispatch_device(uint8_t port) {
	bool clk = port & 1;

	static bool last_clk = false;
	// only care about rising clock
	if (clk == last_clk) {
		return;
	}
	last_clk = clk;
	if (clk == 0) {
		return;
	}

	bool is_sdcard = !((port >> 1) & 1);
	bool is_keyboard = !((port >> 2) & 1);
	bool is_rtc = !((port >> 3) & 1);

	bool mosi = port >> 7;

	static bool last_sdcard;
	static bool last_key;
	static bool last_rtc;
	static uint8_t bit_counter = 0;

	if (!last_sdcard && is_sdcard) {
		bit_counter = 0;
		sdcard_select();
	} else if (is_keyboard) {
//		bit_counter = 0;
//		printf("keyboard\n");
	} else if (!last_rtc && is_rtc) {
//		bit_counter = 0;
//		printf("rtc\n");
	}
	last_sdcard = is_sdcard;
//	last_rtc = is_rtc;

// For initialization, the client has to pull&release CLK 74 times.
// The SD card should be deselected, because it's not actual
// data transmission (we ignore this).
	if (!initialized) {
		if (clk == 1) {
			static int init_counter = 0;
			init_counter++;
			if (init_counter >= 70) {
				sdcard_select();
				initialized = true;
			}
		}
		return;
	}

// for everything else, a device has to be selected
	if (!is_sdcard && !is_keyboard && !is_rtc) {
		return;
	}

// receive byte
	static uint8_t inbyte, outbyte;
	bool bit = mosi;
	inbyte <<= 1;
	inbyte |= bit;
	bit_counter++;
	if (bit_counter != 8) {
		return;
	}

	bit_counter = 0;

	if (initialized && is_sdcard) {// TODO FIXME move to sdcard
		outbyte = sdcard_handle(inbyte);
	} else if (is_keyboard) {
		static int p = 0;
//		char shellcmd[] = {'p','a','c','m','a','n',0xd,0};
//		char shellcmd[] = {'d','i','n','o','s','a','u','r',0xd,0};
//		char shellcmd[] = {'l','l',' ','p','r','o','g','s',0xd,0};
//		char shellcmd[] = {'g','f','x','7','l','i','n','e',0xd,0};
		char shellcmd[] = {'g','f','x','7','s','o','r','t',0xd,0};
//		char shellcmd[] = {'p','o','n','g',0xd,0};
//		char shellcmd[] = {'u','n','r','c','l','o','c','k',0xd,0};

		if(shellcmd[p] != 0){
			outbyte = shellcmd[p++];
		}else
			outbyte = 0xff;
//		printf("key out\n");
	} else if (is_rtc) {
		outbyte = 0x11;
//		printf("rtc out\n");
	}

// send byte
	via2_sr_set(outbyte);
}


void handle_sdcard(uint8_t port) {
	bool clk = port & 1;
	bool ss = !((port >> 1) & 1);
	bool mosi = port >> 7;

	static bool last_clk = false;
	static bool last_ss;
	static int bit_counter = 0;

// only care about rising clock
	if (clk == last_clk) {
		return;
	}
	last_clk = clk;
	if (clk == 0) {
		return;
	}

	if (ss && !last_ss) {
		bit_counter = 0;
		sdcard_select();
	}
	last_ss = ss;

// For initialization, the client has to pull&release CLK 74 times.
// The SD card should be deselected, because it's not actual
// data transmission (we ignore this).
	if (!initialized) {
		if (clk == 1) {
			static int init_counter = 0;
			init_counter++;
			if (init_counter >= 70) {
				sdcard_select();
				initialized = true;
			}
		}
		return;
	}

// for everything else, the SD card needs to be selected
	if (!ss) {
		return;
	}

// receive byte
	static uint8_t inbyte, outbyte;
	bool bit = mosi;
	inbyte <<= 1;
	inbyte |= bit;
//	printf("BIT: %d BYTE =$%02x\n", bit, inbyte);
	bit_counter++;
	if (bit_counter != 8) {
		return;
	}

	bit_counter = 0;

	if (initialized) {
		outbyte = sdcard_handle(inbyte);
	}

// send byte
	via2_sr_set(outbyte);
}

void spi_step() {
	uint8_t port = via2_pb_get_reg(0);	//PB

	dispatch_device(port);
}
