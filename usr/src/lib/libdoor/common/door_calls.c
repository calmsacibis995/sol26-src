/*
 * Copyright (c) Sun Microsystems Inc. 1994
 */
#pragma ident "@(#)door_calls.c 1.10	96/06/14 SMI"

#include <unistd.h>
#include <thread.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <door.h>

static void door_create_server(void);
static void _door_init();

#pragma	init (_door_init)
#pragma weak __thr_door_unbind
#pragma weak pthread_setcancelstate

extern void (*__door_server_func)();
extern int __thr_door_unbind(void);

extern pid_t __door_create_pid;

/*
 * Initialize the door server create function.
 */
static void
_door_init()
{
	__door_server_func = door_create_server;
}

int
door_create(void (*f)(), void *cookie, u_int flags)
{
	int	d;
	static pid_t	firstcall = 0;

	/*
	 * If this is the first door created for a process,
	 * fire off a server thread.
	 * Additional server threads are created during door invocations.
	 *
	 * If this door wants a private pool of server threads, pass
	 * that info along.
	 */
	if ((d = _door_create(f, cookie, flags)) < 0)
		return (-1);
	__door_create_pid = getpid();
	if (flags & DOOR_PRIVATE) {
		door_info_t	di;

		/* Private pool */
		if (thr_main() == -1) {
			fprintf(stderr,
				"libdoor: Not linked with a thread lib\n");
			errno = ENOSYS;
			return (-1);
		}
		if (door_info(d, &di) < 0)
			return (-1);
		(*__door_server_func)(&di);
	} else {
		/*
		 * Make sure we didn't fork, and hence should create
		 * a new initial server thread in the new process.
		 */
		if (__door_create_pid != firstcall) {
			/* Global pool */
			if (thr_main() == -1) {
				fprintf(stderr,
				"libdoor: Not linked with a thread lib\n");
				errno = ENOSYS;
				return (-1);
			}
			firstcall = __door_create_pid;
			(*__door_server_func)(NULL);
		}
	}
	return (d);
}

int
door_revoke(int did)
{
	return (_door_revoke(did));
}

int
door_info(int did, door_info_t *di)
{
	return (_door_info(did, di));
}

int
door_cred(door_cred_t *dc)
{
	return (_door_cred(dc));
}

int
door_bind(int d)
{
	return (_door_bind(d));
}

int
door_unbind()
{
	return (__thr_door_unbind());
}

int
door_call(int did, door_arg_t *arg)
{
	return (_door_call(did, arg));
}

int
door_return(char *data_ptr, size_t data_size,
	door_desc_t *desc_ptr, size_t desc_size)
{
	return (_door_return(data_ptr, data_size, desc_ptr, desc_size, 0));
}

/*
 * Install a new server creation function.
 */
void (*door_server_create(void (*create_func)()))(void (*)())
{
	void (*prev)() = __door_server_func;

	__door_server_func = create_func;
	return (prev);
}

/*
 * Create door server threads with CANCELATION disabled.
 */
/* ARGSUSED */
static void *
door_create_func(void *not_used)
{
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	(void) door_return(NULL, 0, NULL, 0);

	return (NULL);	/* For lint */
}

/*
 * The default server thread creation routine.
 * Both pthreads and solaris threads can use this.
 */
static void
door_create_server(void)
{
	(void) thr_create(NULL, 0, door_create_func, NULL,
			THR_BOUND | THR_DETACHED, NULL);
	yield();	/* Gives server thread a chance to run */
}
