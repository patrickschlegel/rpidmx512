/**
 * @file bw_spi_dio.c
 *
 */
/* Copyright (C) 2016-2018 by Arjan van Vught mailto:info@raspberrypi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>

#if defined(__linux__) || defined(__circle__)
 #include "bcm2835.h"
#else
 #include "bcm2835_spi.h"
 #include "bcm2835_aux_spi.h"
#endif

#include "bw.h"
#include "bw_dio.h"
#include "bw_spi_dio.h"

#include "device_info.h"

#define BW_DIO_SPI_BYTE_WAIT_US		0

/**
 * @ingroup SPI-DIO
 *
 * @param device_info
 * @return
 */
void bw_spi_dio_start(device_info_t *device_info) {

	if (device_info->slave_address == (uint8_t) 0) {
		device_info->slave_address = BW_DIO_DEFAULT_SLAVE_ADDRESS;
	}

	if (device_info->speed_hz == (uint32_t) 0) {
		device_info->speed_hz = (uint32_t) BW_DIO_SPI_SPEED_DEFAULT_HZ;
	} else if (device_info->speed_hz > (uint32_t) BW_DIO_SPI_SPEED_MAX_HZ) {
		device_info->speed_hz = (uint32_t) BW_DIO_SPI_SPEED_MAX_HZ;
	}

	if (device_info->chip_select >= SPI_CS2) {
		device_info->chip_select = SPI_CS2;
		bcm2835_aux_spi_begin();
		device_info->internal.clk_div = bcm2835_aux_spi_CalcClockDivider(device_info->speed_hz);
	} else {
		bcm2835_spi_begin();
		device_info->internal.clk_div = (uint16_t)((uint32_t) BCM2835_CORE_CLK_HZ / device_info->speed_hz);
	}

	bw_spi_dio_fsel_mask(device_info, 0x7F);
}

/**
 * @ingroup SPI-DIO
 *
 * @param device_info
 * @param mask
 */
void bw_spi_dio_fsel_mask(const device_info_t *device_info, const uint8_t mask) {
	char cmd[3];

	cmd[0] = (char)device_info->slave_address;
	cmd[1] = (char)BW_PORT_WRITE_IO_DIRECTION;
	cmd[2] = (char)mask;

	if (device_info->chip_select == SPI_CS2) {
		bcm2835_aux_spi_setClockDivider(device_info->internal.clk_div);
		bcm2835_aux_spi_writenb(cmd, sizeof(cmd) / sizeof(cmd[0]));
	} else {
		bcm2835_spi_setClockDivider(device_info->internal.clk_div);
		bcm2835_spi_chipSelect(device_info->chip_select);
		bcm2835_spi_writenb(cmd, sizeof(cmd) / sizeof(cmd[0]));
	}
}

/**
 * @ingroup SPI-DIO
 *
 * @param device_info
 * @param pins
 */
void bw_spi_dio_output(const device_info_t *device_info, const uint8_t pins) {
	char cmd[3];

	cmd[0] = (char) device_info->slave_address;
	cmd[1] = (char) BW_PORT_WRITE_SET_ALL_OUTPUTS;
	cmd[2] = (char) pins;

	if (device_info->chip_select == SPI_CS2) {
		bcm2835_aux_spi_setClockDivider(device_info->internal.clk_div);
		bcm2835_aux_spi_writenb(cmd, sizeof(cmd) / sizeof(cmd[0]));
	} else {
		bcm2835_spi_setClockDivider(device_info->internal.clk_div);
		bcm2835_spi_chipSelect(device_info->chip_select);
		bcm2835_spi_writenb(cmd, sizeof(cmd) / sizeof(cmd[0]));
	}
}

