#ifndef FIBER_H
#define FIBER_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * struct fiber
 * - a structure contains fiber private data.
 */
struct fiber;

/**
 * typedef fiber_cb
 * fiber startup function
 */
typedef void (*fiber_cb)(void *);

/**
 * fibers_init() - init fiber system
 *
 * Note: can be call once by session. Main process will
 * be registered as current fiber. The fiber can be cancelled
 * if the other fibers are present.
 */
void fibers_init(void);

/**
 * fiber_create() - create new fiber
 * @cb - fiber startup function
 * @stack - stack for created fiber
 * @stack_size - stack size
 * @data - data that will passed to fiber startup function
 *
 * Note: the function will allocate struct fiber (in the stack)
 * and use the stack while doing switch to the fiber.
 *
 * New fiber creates with 'R' status. One of the following fiber_cede
 * or fiber_schedule calls will switch to the fiber.
 */
struct fiber *
fiber_create(fiber_cb cb, void *stack, size_t stack_size, void *data);

/**
 * fiber_current - get current fiber handler
 */
struct fiber * fiber_current(void);

/** fiber_status - get fiber status
 *
 * - 'r' - normal processing fiber
 * - 's' - scheduled fiber (use fiber_wakeup to make the fiber ready)
 * - 'd' - dead fiber (callback was done)
 * - 'c' - cancelled by fiber_cancel
 */
unsigned char fiber_status(const struct fiber *f);

/** fiber_cede - switch to the other processing fiber
 *
 * return immediately if there is no one other processing fibers
 */
void fiber_cede(void);

/** fiber_cancel - cancel processing fiber
 *
 * Note: the function can use to cancel current fiber.
 */
void fiber_cancel(struct fiber *fiber);

/** fiber_schedule - switch to the other ready fiber,
 * marks current fiber as scheduled.
 *
 */
void fiber_schedule(void);

/** fiber_wakeup - wakeup scheduled fiber
 *
 * Note: the function can be call inside interrupt handler.
 */
void fiber_wakeup(struct fiber *w);


#define FIBER_CREATE(__cb, __stack_size, __data)			\
	do {								\
		static unsigned char __stack[__stack_size];		\
		fiber_create(__cb, __stack, __stack_size, __data);	\
	} while(0);

#define FIBERV_CREATE(__cb, __stack_size)				\
	FIBER_CREATE(__cb, __stack_size, NULL)

#ifdef FIBER_STACK_SIZE
#define FIBER(__cb, __data)						\
	FIBER_CREATE(__cb, FIBER_STACK_SIZE, __data)

#define FIBERV(__cb)							\
	FIBER_CREATE(__cb, FIBER_STACK_SIZE, NULL)

#endif	/* defined FIBER_STACK_SIZE */


#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif /* FIBER_H */
