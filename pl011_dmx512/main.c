#include <stdio.h>
#include <bcm2835.h>
#include <hardware.h>

void __attribute__((interrupt("FIQ"))) c_fiq_handler(void) {}

extern void fb_init(void);

uint8_t dmx_data[512];

#define ANALYZER_CH1	RPI_V2_GPIO_P1_23     // CLK
#define ANALYZER_CH2   RPI_V2_GPIO_P1_21     // MISO
#define ANALYZER_CH3   RPI_V2_GPIO_P1_19     // MOSI
#define ANALYZER_CH4   RPI_V2_GPIO_P1_24     // CE0

// -------------------------------------------------------------------------------------- //

static void bcm2835_pl011_dmx512_init(void)
{
	BCM2835_PL011->CR = 0;						/* Disable everything */

    // Set the GPI0 pins to the Alt 0 function to enable PL011 access on them
    bcm2835_gpio_fsel(RPI_V2_GPIO_P1_08, BCM2835_GPIO_FSEL_ALT0); // PL011_TXD
    bcm2835_gpio_fsel(RPI_V2_GPIO_P1_10, BCM2835_GPIO_FSEL_ALT0); // PL011_RXD

    // Disable pull-up/down
    bcm2835_gpio_set_pud(RPI_V2_GPIO_P1_08, BCM2835_GPIO_PUD_OFF);
    bcm2835_gpio_set_pud(RPI_V2_GPIO_P1_10, BCM2835_GPIO_PUD_OFF);

    /* Poll the "flags register" to wait for the UART to stop transmitting or receiving. */
	while (BCM2835_PL011 ->FR & PL011_FR_BUSY ) {
	}

    /* Flush the transmit FIFO by marking FIFOs as disabled in the "line control register". */
	BCM2835_PL011->LCRH &= ~PL011_LCRH_FEN;

    BCM2835_PL011->ICR = 0x7FF;									/* Clear all interrupt status */
    BCM2835_PL011->IBRD = 1;									// config.txt
    BCM2835_PL011->FBRD = 0;									// init_uart_clock=4000000 (4MHz)
    BCM2835_PL011->LCRH = PL011_LCRH_WLEN8 | PL011_LCRH_STP2 ;	/* Set 8, N, 2, FIFO disabled */
    BCM2835_PL011->CR = 0x301;									/* Enable UART */
}

// -------------------------------------------------------------------------------------- //

static void irq_init(void) {
	BCM2835_PL011->IMSC = PL011_IMSC_RXIM;
	BCM2835_IRQ->IRQ_ENABLE2 = 1 << 25;
}

// State of receiving DMX Bytes
typedef enum {
  IDLE, BREAK, DATA
} _dmx_receive_state;

uint8_t dmx_receive_state = IDLE;
uint16_t dmx_channel_index = 0;

void __attribute__((interrupt("IRQ"))) c_irq_handler(void) {
	bcm2835_gpio_set(ANALYZER_CH1);

	if (BCM2835_PL011 ->DR & PL011_DR_BE ) {
		bcm2835_gpio_set(ANALYZER_CH2); // BREAK
		bcm2835_gpio_clr(ANALYZER_CH3);	// DATA
		bcm2835_gpio_clr(ANALYZER_CH4);	// IDLE

		dmx_receive_state = BREAK;
	}  else if (dmx_receive_state == BREAK) {
		if ((BCM2835_PL011 ->DR & 0xFF) == 0) {
			bcm2835_gpio_clr(ANALYZER_CH2); // BREAK
			bcm2835_gpio_set(ANALYZER_CH3);	// DATA
			bcm2835_gpio_clr(ANALYZER_CH4); // IDLE

			dmx_receive_state = DATA;
			dmx_channel_index = 0;
		} else {
			bcm2835_gpio_clr(ANALYZER_CH2); // BREAK
			bcm2835_gpio_clr(ANALYZER_CH3);	// DATA
			bcm2835_gpio_set(ANALYZER_CH4); // IDLE

			dmx_receive_state = IDLE;
		}
	} else if (dmx_receive_state == DATA) {
		dmx_data[dmx_channel_index] = (BCM2835_PL011 ->DR & 0xFF);
		dmx_channel_index++;
		if (dmx_channel_index >= 512) {
			bcm2835_gpio_clr(ANALYZER_CH2); // BREAK
			bcm2835_gpio_clr(ANALYZER_CH3);	// DATA
			bcm2835_gpio_set(ANALYZER_CH4); // IDLE

			dmx_receive_state = IDLE;
		}
	}

	bcm2835_gpio_clr(ANALYZER_CH1);
}

// -------------------------------------------------------------------------------------- //

void task_fb(void) {
#if 0
	printf("%X %X %X %X %X %X %X %X\n", dmx_data[0], dmx_data[1], dmx_data[2],
			dmx_data[3], dmx_data[4], dmx_data[5], dmx_data[6], dmx_data[7]);
	printf("%X %X %X %X %X %X %X %X\n\n", dmx_data[8], dmx_data[9],
			dmx_data[10], dmx_data[11], dmx_data[12], dmx_data[13],
			dmx_data[14], dmx_data[15]);
#else
	printf("%X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X\n", dmx_data[0],
			dmx_data[1], dmx_data[2], dmx_data[3], dmx_data[4], dmx_data[5],
			dmx_data[6], dmx_data[7], dmx_data[8], dmx_data[9], dmx_data[10],
			dmx_data[11], dmx_data[12], dmx_data[13], dmx_data[14],
			dmx_data[15]);
#endif
}

void task_led(void) {
	static unsigned char led_counter = 0;
	led_set(led_counter++ & 0x01);
}

typedef struct _event {
	uint64_t period;
	void (*f)(void);
} event;

const event events[] = {
		{ 1000000, task_fb},
		{ 500000, task_led}
};

uint64_t events_elapsed_time[sizeof(events) / sizeof(events[0])];

void events_init() {
	int i;
	uint64_t st_read_now = bcm2835_st_read();
	for (i = 0; i < (sizeof(events) / sizeof(events[0])); i++) {
		events_elapsed_time[i] += st_read_now;
	}
}

static inline void events_check() {
	int i;
	uint64_t st_read_now = bcm2835_st_read();
	for (i = 0; i < (sizeof(events) / sizeof(events[0])); i++) {
		if (st_read_now > events_elapsed_time[i] + events[i].period) {
			events[i].f();
			events_elapsed_time[i] += events[i].period;
		}
	}
}

int notmain(unsigned int earlypc) {

	int i;

	for(i = 0; i < sizeof(dmx_data); i++) {
		dmx_data[i] = 0;
	}

	__disable_irq();

	fb_init();

	printf("Compiled on %s at %s\n", __DATE__, __TIME__);

	led_init();
	led_set(1);

	bcm2835_gpio_fsel(ANALYZER_CH1, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(ANALYZER_CH2, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(ANALYZER_CH3, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(ANALYZER_CH4, BCM2835_GPIO_FSEL_OUTP);

	bcm2835_gpio_clr(ANALYZER_CH1); // IRQ
	bcm2835_gpio_clr(ANALYZER_CH2);	// BREAK
	bcm2835_gpio_clr(ANALYZER_CH3); // DATA
	bcm2835_gpio_set(ANALYZER_CH4);	// IDLE

	bcm2835_pl011_dmx512_init();

	irq_init();
	__enable_irq();

	events_init();

	for (;;) {
		events_check();
	}

	return (0);
}