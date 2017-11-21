#include <avr/interrupt.h>
#include <util/atomic.h>

#define FIBER_STACK_SIZE	64
#include <fiber.h>


#define F_CLOCK		128

static struct fiber *_delay[5] = {NULL, NULL, NULL, NULL, NULL};
#define SLOTS	(sizeof(_delay) / sizeof(*_delay))


void
fiber_delay(uint8_t clocks)
{
	while(clocks--) {
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			for (uint8_t i = 0; i < SLOTS; i++) {
				if (!_delay[i]) {
					_delay[i] = fiber_current();
					break;
				}
			}
		}
		fiber_schedule();
	}
}

/* data for all processes */
static struct color_out {
	uint8_t led, period, start;
} colors[] = {
	{ .led = 7, .period = 29, .start = LOW	},
	{ .led = 8, .period = 30, .start = HIGH },
	{ .led = 9, .period = 31, .start = LOW	},
};

/* fiber process */
static void
led_X(void *data)
{
	struct color_out *opts = (struct color_out *)data;

	pinMode(opts->led, OUTPUT);
	digitalWrite(opts->led, opts->start);

	for (;;) {
		fiber_delay(opts->period);
		if (digitalRead(opts->led))
			digitalWrite(opts->led, LOW);
		else
			digitalWrite(opts->led, HIGH);
	}
}




void
setup() {
	// configure timer
	OCR2A = F_CPU / 1024 /  F_CLOCK;
	TCCR2A = (1 << WGM21) | (0 << WGM20);			// CTC to OCR2A
	TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);	// divider = 1024
	TIMSK2 = (1 << OCIE2A);					// interrupt enable
	sei();							// global interrupts

	// enable fibers
	fibers_init();

	FIBER(led_X, (void *)&colors[0]);
	FIBER(led_X, (void *)&colors[1]);
	FIBER(led_X, (void *)&colors[2]);
}

SIGNAL(TIMER2_COMPA_vect) {
	TCNT2 = 0;
	for (uint8_t i = 0; i < SLOTS; i++) {
		fiber_wakeup(_delay[i]);
		_delay[i] = NULL;
	}
}

void
loop() {
	fiber_schedule();
}
