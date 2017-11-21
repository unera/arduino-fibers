#include <fiber.h>
#include <stdint.h>
#include <avr/io.h>
#include <util/atomic.h>
#include <string.h>
#include "list.h"

enum fiber_state {
	READY		= 'r',
	STARTING	= 'R',
	WAKEUP		= 'W',

	SCHEDULED	= 's',
	DEAD		= 'd',
	CANCELLED	= 'c'
};

typedef uint16_t		code_t;
struct fiber {
	struct list_head	list;		// member

	fiber_cb		cb;		// fiber function
	code_t			sp;		// stack pointer

	void			*data;
	enum fiber_state	state;
};

static struct fiber *current;
static LIST_HEAD(ready);
static LIST_HEAD(sch);
static LIST_HEAD(dead);

static void _fiber_run(void);
static struct fiber * _fiber_fetch_next_ready(void);
void _fiber_switch(struct fiber *next, struct list_head *list);
static void _fiber_schedule(enum fiber_state new_state, struct list_head *list);


struct fiber *
fiber_create(fiber_cb cb, void *stack, size_t stack_size, void *data)
{
	struct fiber *c = (struct fiber *)stack;
	c->state = STARTING;
	c->cb = cb;
	c->sp = (code_t)(((uint8_t *)stack) + stack_size - 1);
	c->data = data;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		list_add_tail(&c->list, &ready);
	}
	return c;
}

// return current fiber
struct fiber *
fiber_current(void)
{
	return current;
}

void
fiber_cede(void)
{
	if (!current)		// not init yet
		return;

	struct fiber *next;
	if (!(next = _fiber_fetch_next_ready()))
		return;

	return _fiber_switch(next, &ready);
}

void
fiber_schedule(void)
{
	if (current->state == WAKEUP) {
		current->state = READY;
		return;
	}

	return _fiber_schedule(SCHEDULED, &sch);
}

void
fiber_wakeup(struct fiber *f)
{
	if (!f)
		return;
	if (f->state != SCHEDULED) {
		if (f == current && f->state == READY) {
			f->state = WAKEUP;
			return;
		}
		return;
	}

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		list_del_init(&f->list);
		list_add_tail(&f->list, &ready);
		f->state = READY;
	}
}

void
fibers_init(void)
{
	static struct fiber _main;
	if (current)			// already init done
		return;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		memset(&_main, 0, sizeof(_main));
		_main.state = READY;
		list_add_tail(&_main.list, &ready);
		current = &_main;
	}
}

void
fiber_cancel(struct fiber *f)
{
	if (!f)
		return;
	switch(f->state) {
		case DEAD:
		case CANCELLED:
			return;
		case READY:
		case WAKEUP:
			break;
		case SCHEDULED:
		case STARTING:
			goto DROP;
	}
	if (f == current) {
		return _fiber_schedule(CANCELLED, &dead);
	}

	DROP:
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			f->state = CANCELLED;
			list_del_init(&f->list);
			list_add_tail(&f->list, &dead);
		}
}

unsigned char
fiber_status(const struct fiber *f)
{
	if (!f)
		return 'U';
	switch(f->state) {
		case READY:
		case STARTING:
		case WAKEUP:
			return READY;
		default:
			return f->state;
	}
}

/********** private functions ************************************/

static void
_fiber_run(void)
{
	for(;;) {
		current->state = READY;
		current->cb(current->data);
		current->state = DEAD;
		_fiber_schedule(DEAD, &dead);
		// someone woke up zombie
	}
}

void
_fiber_switch(struct fiber *next, struct list_head *list)
{
	struct fiber *prev;
	prev = current;

	static uint8_t real_sreg;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		list_del_init(&prev->list);
		if (list)
			list_add_tail(&prev->list, list);
		current = next;

		// hack!: we hope ATOMIC_RESTORESTATE is unchanged
		// in the future
		real_sreg	= sreg_save;

		/* switch stack! */
		prev->sp = SP;
		SP = next->sp;

		if (current->state == STARTING) {
			SREG = real_sreg;
			// don't return here
			_fiber_run();
		}
	}

	// stack was switched, so SREG could be changed
	// TODO: review
	SREG = real_sreg;
	current->state = READY;
}

static void
_fiber_schedule(enum fiber_state new_state, struct list_head *list)
{
	if (!current)
		return;

	current->state = new_state;
	struct fiber *next;

	// Deadlock: there is no fiber ready
	// wait until next fiber is present
	// or current is ready
	for (;;) {
		next = _fiber_fetch_next_ready();

		if (current->state == READY && new_state == SCHEDULED) {
				return;
		}
		if (next) {
			return _fiber_switch(next, list);
		}

	}
}

static struct fiber *
_fiber_fetch_next_ready(void)
{
	struct list_head *pos;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		list_for_each(pos, &ready) {
			if (pos == &current->list)
				continue;
			return list_entry(pos, struct fiber, list);
		}
	}
	return NULL;
}
