/*
 * Copyright (c) 1994 - 1996, Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)powerd.c 1.22	96/09/20 SMI"

#include <stdio.h>			/* Standard */
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <sys/todio.h>			/* Time-Of-Day chip */
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ipc.h>			/* IPC functions */
#include <sys/shm.h>
#include <signal.h>			/* signal handling */
#include <syslog.h>
#include <unistd.h>
#include <sys/pm.h>			/* power management driver */
#include <sys/uadmin.h>			/* for sys-suspend */
#include <sys/battery.h>		/* battery driver */
#include <sys/openpromio.h>		/* for prom access */
#include <sys/sysmacros.h>		/* for MIN & MAX macros */

#include "powerd.h"

/* External Functions */
extern struct tm *localtime_r(const time_t *, struct tm *);
extern char *crypt();
extern void sysstat_init(void);
extern int check_tty(hrtime_t *, int);
extern int check_disks(hrtime_t *, int);
extern int check_load_ave(hrtime_t *, float);
extern int check_nfs(hrtime_t *, int);
extern int last_disk_activity(hrtime_t *, int);
extern int last_tty_activity(hrtime_t *, int);
extern int last_load_ave_activity(hrtime_t *);
extern int last_nfs_activity(hrtime_t *, int);

#define	BATTERY		"/dev/battery"
#define	PM		"/dev/pm"
#define	TOD		"/dev/tod"
#define	PROM		"/dev/openprom"
#define	ESTAR_PROP	"energystar-v2"
#define	SUSPEND		"/usr/openwin/bin/sys-suspend"
#define	POWERCONF	"/etc/power.conf"
#define	LOGFILE		"./powerd.log"

#define	CHECK_INTERVAL	5
#define	IDLECHK_INTERVAL	15
#define	MINS_TO_SECS	60
#define	HOURS_TO_SECS	(60 * 60)
#define	DAYS_TO_SECS	(24 * 60 * 60)
#define	HOURS_TO_MINS	60
#define	DAYS_TO_MINS	(24 * 60)

#define	LIFETIME_SECS			(7 * 365 * DAYS_TO_SECS)
#define	DEFAULT_POWER_CYCLE_LIMIT	10000
#define	DEFAULT_SYSTEM_BOARD_DATE	804582000	/* July 1, 1995 */

#define	LOGERROR(m)	if (broadcast) {				\
				syslog(LOG_ERR, (m));			\
			}

typedef	enum {root, options} prom_node_t;

/* State Variables */
static time_t		battery_time;	/* Time for next battery check */
static time_t		shutdown_time;	/* Time for next shutdown check */
static time_t		checkidle_time;	/* Time for next idleness check */
static time_t		last_resume;
pwr_info_t		*info;		/* Shared memory address */
static int		pm_fd;		/* power manager pseudo driver */
static int		battery_fd;	/* battery module */
static int		tod_fd;		/* TOD module */
static int		prom_fd;	/* PROM module */
static int		shmid;		/* Shared memory id */
static int		broadcast;	/* Enables syslog messages */
static int		start_calc;
static int		autoshutdown_en;
static int		ttychars_thold;
static float		loadaverage_thold;
static int		diskreads_thold;
static int		nfsreqs_thold;
static char		idlecheck_path[256];
static int		do_idlecheck;
static int		got_sighup;
static int		energystar_prop;
static int		log_power_cycles_error = 0;
static int		log_system_board_date_error = 0;
static int		log_no_autoshutdown_warning = 0;

/* Local Functions */
static void alarm_handler(int);
static void thaw_handler(int);
static void kill_handler(int);
static void work_handler(int);
static void check_shutdown(time_t *, hrtime_t *);
static void check_battery(time_t *);
static void check_idleness(time_t *, hrtime_t *);
static int last_system_activity(hrtime_t *);
static int run_idlecheck(void);
static void set_alarm(time_t);
static void poweroff(int, char *);
static int is_ok2shutdown(time_t *);
static int get_prom(prom_node_t, char *, char *);
#ifdef SETPROM
static int set_prom(char *, char *);
#endif
static int parse_line(FILE *, char *);

int
main(int argc, char *argv[])
{
	pid_t		pid;
	key_t		key;
	struct sigaction act;
	sigset_t	sigmask;
	int		c;
	char		errmsg[256];

	if (geteuid() != 0) {
		(void) fprintf(stderr, "%s: Must be root\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	if ((key = ftok(PM, 'P')) < 0) {
		(void) fprintf(stderr, "%s: Unable to access %s\n",
		    argv[0], PM);
		exit(EXIT_FAILURE);
	}

	/*
	 * Check for left over IPC state
	 */
	shmid = shmget(key, sizeof (pwr_info_t), SHM_RDONLY);
	if (shmid >= 0) {
		info = (pwr_info_t *)shmat(shmid, NULL, SHM_RDONLY);
		if (info != (pwr_info_t *)-1) {
			if (sigsend(P_PID, info->pd_pid, SIGHUP) == 0) {
				(void) fprintf(stderr,
				    "%s: Another daemon is running\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			(void) shmdt((void *)info);
			if (shmctl(shmid, IPC_RMID, NULL) < 0) {
				(void) fprintf(stderr,
				    "%s: Unable to remove shared memory\n",
				    argv[0]);
			}
		}
	}

	/*
	 * Process options
	 */
	broadcast = 1;
	while ((c = getopt(argc, argv, "n")) != EOF) {
		switch (c) {
		case 'n':
			broadcast = 0;
			break;
		case '?':
			(void) fprintf(stderr, "Usage: %s [-n]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	/*
	 * Open power manager, battery, time-of-day, and prom.
	 */
	pm_fd = open(PM, O_RDWR);
	if (pm_fd == -1) {
		(void) sprintf(errmsg, "%s: %s", argv[0], PM);
		perror(errmsg);
		exit(EXIT_FAILURE);
	}
	battery_fd = open(BATTERY, O_RDWR);
	tod_fd = open(TOD, O_RDWR);

	/* CONSTCOND */
	while (1) {
		if ((prom_fd = open(PROM, O_RDWR)) == -1 &&
			(errno == EAGAIN))
				continue;
		break;
	}

	/*
	 * Initialise shared memory
	 */
	shmid = shmget(key, sizeof (pwr_info_t), IPC_CREAT | IPC_EXCL | 0644);
	if (shmid < 0) {
		if (errno != EEXIST) {
			(void) sprintf(errmsg, "%s: shmget", argv[0]);
			perror(errmsg);
		} else
			(void) fprintf(stderr,
			    "%s: Another daemon is running\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	info = (pwr_info_t *)shmat(shmid, NULL, 0644);
	if (info == (pwr_info_t *)-1) {
		(void) sprintf(errmsg, "%s: shmat", argv[0]);
		perror(errmsg);
		(void) shmctl(shmid, IPC_RMID, NULL);
		exit(EXIT_FAILURE);
	}

	/*
	 * Daemon is set to go...
	 */
	if ((pid = fork()) < 0)
		exit(EXIT_FAILURE);
	else if (pid != 0)
		exit(EXIT_SUCCESS);
	pid = getpid();
	(void) setsid();
	(void) chdir("/");
	(void) umask(0);
	openlog(argv[0], 0, LOG_DAEMON);
	info->pd_pid = pid;
	info->pd_flags = PD_AC;
	info->pd_idle_time = -1;
	info->pd_start_time = 0;
	info->pd_finish_time = 0;
	info->pd_charge = -1;
	info->pd_time_left = -1;
	/*
	 *  If tod driver is present, set the autoresume flag.
	 */
	if (tod_fd != -1) {
		info->pd_flags |= PD_AUTORESUME;
	}

	/*
	 * Setup for gathering system's statistic.
	 */
	sysstat_init();

	/*
	 * As of Oct. 1, 1995, any new system shipped will have root
	 * property "energystar-v2" defined in its prom.
	 */
	energystar_prop = get_prom(root, ESTAR_PROP, NULL);

	act.sa_handler = kill_handler;
	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	(void) sigaction(SIGQUIT, &act, NULL);
	(void) sigaction(SIGINT, &act, NULL);
	(void) sigaction(SIGTERM, &act, NULL);

	act.sa_handler = alarm_handler;
	(void) sigaction(SIGALRM, &act, NULL);

	act.sa_handler = work_handler;
	(void) sigaction(SIGHUP, &act, NULL);

	act.sa_handler = thaw_handler;
	(void) sigaction(SIGTHAW, &act, NULL);

	work_handler(SIGHUP);

	/*
	 * Wait for signal to read file
	 */
	(void) sigfillset(&sigmask);
	(void) sigdelset(&sigmask, SIGQUIT);
	(void) sigdelset(&sigmask, SIGINT);
	(void) sigdelset(&sigmask, SIGHUP);
	(void) sigdelset(&sigmask, SIGTERM);
	(void) sigdelset(&sigmask, SIGALRM);
	(void) sigdelset(&sigmask, SIGTHAW);
	do {
		(void) sigsuspend(&sigmask);
	} while (errno == EINTR);
	return (1);
}

/*ARGSUSED*/
static void
thaw_handler(int sig)
{
	start_calc  = 0;
	last_resume = time(NULL);
}

/*ARGSUSED*/
static void
kill_handler(int sig)
{
	/*
	 * Remove all the power-managed devices and that brings
	 * them back to the normal power mode.
	 */
	if (ioctl(pm_fd, PM_REM_DEVICES, NULL) == -1) {
		LOGERROR("Unable to remove power-managed devices.");
	}

	/*
	 * Free resources
	 */
	(void) shmdt((void *)info);
	(void) shmctl(shmid, IPC_RMID, NULL);
	(void) close(pm_fd);
	if (battery_fd != -1)
		(void) close(battery_fd);
	if (tod_fd != -1)
		(void) close(tod_fd);
	if (prom_fd != -1)
		(void) close(prom_fd);
	closelog();
	exit(EXIT_SUCCESS);
}

/*ARGSUSED*/
static void
alarm_handler(int sig)
{
	time_t		now;
	hrtime_t	hr_now;

	now = time(NULL);
	hr_now = gethrtime();
	if (battery_time <= now && battery_time != 0)
		check_battery(&now);
	if (checkidle_time <= now && checkidle_time != 0)
		check_idleness(&now, &hr_now);
	if (shutdown_time <= now && shutdown_time != 0)
		check_shutdown(&now, &hr_now);

	set_alarm(now);
}

/*ARGSUSED*/
static void
work_handler(int sig)
{
	time_t		now;
	hrtime_t	hr_now;
	FILE		*infile;
	char		inbuf[512], name[80], behavior[80] = "noshutdown";
	int		idle, sh, sm, fh, fm;
	struct stat	stat_buf;

	/*
	 * Default idleness thresholds
	 */
	ttychars_thold = 0;
	loadaverage_thold = 0.04;
	diskreads_thold = 0;
	nfsreqs_thold = 0;
	do_idlecheck = 0;

	/*
	 * Parse the config file for autoshutdown and idleness entries.
	 */
	if ((infile = fopen(POWERCONF, "r")) == NULL) {
		return;
	}
	while (parse_line(infile, inbuf) != EOF) {
		if (sscanf(inbuf, "%s", name) != 1) {
			continue;
		}
		if (strcmp(name, "autoshutdown") == 0) {
			if (sscanf(inbuf, "%s%d%d:%d%d:%d%s", name, &idle,
					&sh, &sm, &fh, &fm, behavior) != 7) {
				LOGERROR("illegal \"autoshutdown\" entry.")
				(void) fclose(infile);
				return;
			}
		} else if (strcmp(name, "ttychars") == 0) {
			if (sscanf(inbuf, "%s%d", name, &ttychars_thold) != 2) {
				LOGERROR("illegal \"ttychars\" entry.")
			}
		} else if (strcmp(name, "loadaverage") == 0) {
			if (sscanf(inbuf, "%s%f", name, &loadaverage_thold) !=
					2) {
				LOGERROR("illegal \"loadaverage\" entry.")
			}
		} else if (strcmp(name, "diskreads") == 0) {
			if (sscanf(inbuf, "%s%d", name, &diskreads_thold) !=
					2) {
				LOGERROR("illegal \"diskreads\" entry.")
			}
		} else if (strcmp(name, "nfsreqs") == 0) {
			if (sscanf(inbuf, "%s%d", name, &nfsreqs_thold) != 2) {
				LOGERROR("illegal \"nfsreqs\" entry.")
			}
		} else if (strcmp(name, "idlecheck") == 0) {
			if (sscanf(inbuf, "%s%s", name, idlecheck_path) != 2) {
				LOGERROR("illegal \"idlecheck\" entry.")
			} else if (stat(idlecheck_path, &stat_buf) != 0) {
				(void) sprintf(inbuf, "unable to access "
					"idlecheck program \"%s\".",
					idlecheck_path);
				LOGERROR(inbuf)
			} else if (!(stat_buf.st_mode & S_IXUSR)) {
				(void) sprintf(inbuf, "idlecheck program "
					"\"%s\" is not executable.",
					idlecheck_path);
				LOGERROR(inbuf)
			} else {
				do_idlecheck = 1;
			}
		}
	}
	(void) fclose(infile);

	if (strcmp(behavior, "default") == 0) {
		info->pd_autoshutdown = energystar_prop;
	} else if (strcmp(behavior, "noshutdown") == 0 ||
		strcmp(behavior, "unconfigured") == 0) {
		info->pd_autoshutdown = 0;
	} else if (strcmp(behavior, "shutdown") == 0 ||
		strcmp(behavior, "autowakeup") == 0) {
		info->pd_autoshutdown = 1;
	} else {
		sprintf(inbuf, "autoshutdown behavior \"%s\" unrecognized.",
			behavior);
		LOGERROR(inbuf);
		info->pd_autoshutdown = 0;
	}

	if ((info->pd_autoshutdown == 1) && (stat(SUSPEND, &stat_buf) != 0)) {
		(void) sprintf(inbuf, "Unable to perform autoshutdown. "
			"Can not access file "
			"\"%s\" (SUNWpmowu package).", SUSPEND);
			LOGERROR(inbuf)
		return;
	}

	info->pd_idle_time = idle;
	info->pd_start_time = (sh * 60 + sm) % DAYS_TO_MINS;
	info->pd_finish_time = (fh * 60 + fm) % DAYS_TO_MINS;
	info->pd_autoresume = (strcmp(behavior, "autowakeup") == 0) ? 1 : 0;
	autoshutdown_en = (idle >= 0 && info->pd_autoshutdown) ? 1 : 0;

	got_sighup = 1;
	now = last_resume = time(NULL);
	hr_now = gethrtime();
	check_battery(&now);
	check_idleness(&now, &hr_now);
	check_shutdown(&now, &hr_now);
	set_alarm(now);
}

static void
check_shutdown(time_t *now, hrtime_t *hr_now)
{
	pm_request	req;
	int		kbd, mouse, system, least_idle, idlecheck_time;
	int		next_time;
	int		s, f;
	struct tm	tmp_time;
	time_t		start_of_day, time_since_last_resume;
	time_t		wakeup_time;

	if (!autoshutdown_en) {
		shutdown_time = 0;
		return;
	}

	(void) localtime_r(now, &tmp_time);
	tmp_time.tm_sec = 0;
	tmp_time.tm_min = 0;
	tmp_time.tm_hour = 0;
	start_of_day = mktime(&tmp_time);
	s = start_of_day + info->pd_start_time * 60;
	f = start_of_day + info->pd_finish_time * 60;
	if ((s < f && *now >= s && *now < f) ||
	    (s >= f && (*now < f || *now >= s))) {
		req.who = "/dev/mouse";
		req.select = 0;
		mouse = ioctl(pm_fd, PM_GET_IDLE_TIME, &req);
		req.who = "/dev/kbd";
		kbd = ioctl(pm_fd, PM_GET_IDLE_TIME, &req);
		system = last_system_activity(hr_now);
		/* who is the last to go idle */
		least_idle = MIN(system, MIN(kbd, mouse));
#ifdef DEBUG
		(void) printf("Idle (kbd, mouse, system) = (%d, %d, %d)\n",
			kbd, mouse, system);
#endif
		/*
		 * Calculate time_since_last_resume and the next_time
		 * to auto suspend.
		 */
		start_calc = 1;
		time_since_last_resume = time(NULL) - last_resume;
		next_time = info->pd_idle_time * 60 -
				MIN(least_idle, time_since_last_resume);

		/*
		 * If we have get the SIGTHAW signal at this point - our
		 * calculation of time_since_last_resume is wrong  so
		 * - we need to recalculate.
		 */
		while (start_calc == 0) {
			/* need to redo calculation */
			start_calc = 1;
			time_since_last_resume = time(NULL) - last_resume;
			next_time = info->pd_idle_time * 60 -
				MIN(least_idle, time_since_last_resume);
		}

		/*
		 * Only when everything else is idle, run the user's idlecheck
		 * script.
		 */
		if (next_time <= 0 && do_idlecheck) {
			got_sighup = 0;
			idlecheck_time = run_idlecheck();
			next_time = info->pd_idle_time * 60 -
				MIN(idlecheck_time, MIN(least_idle,
				time_since_last_resume));
			/*
			 * If we have caught SIGTHAW or SIGHUP, need to
			 * recalculate.
			 */
			while (start_calc == 0 || got_sighup == 1) {
				start_calc = 1;
				got_sighup = 0;
				idlecheck_time = run_idlecheck();
				time_since_last_resume = time(NULL) -
					last_resume;
				next_time = info->pd_idle_time * 60 -
					MIN(idlecheck_time, MIN(least_idle,
					time_since_last_resume));
			}
		}

		if (next_time <= 0) {
			if (is_ok2shutdown(now)) {
				/*
				 * Setup the autowakeup alarm.  Clear it
				 * right after poweroff, just in case if
				 * shutdown doesn't go through.
				 */
				if (tod_fd != -1 && info->pd_autoresume) {
					wakeup_time = (*now < f) ? f :
							(f + DAYS_TO_SECS);
					/*
					 * A software fix for hardware
					 * bug 1217415.
					 */
					if ((wakeup_time - *now) < 180) {
						LOGERROR(
		"Since autowakeup time is less than 3 minutes away, "
		"autoshutdown will not occur.");
						shutdown_time = *now + 180;
						return;
					}
					if (ioctl(tod_fd, TOD_SET_ALARM,
							&wakeup_time) == -1) {
						LOGERROR("Unable to program "
							"TOD alarm for "
							"autowakeup.");
						return;
					}
				}

				poweroff(AD_COMPRESS, "Autoshutdown\n");

				if (tod_fd != -1 && info->pd_autoresume) {
					if (ioctl(tod_fd, TOD_CLEAR_ALARM,
							NULL) == -1)
						LOGERROR("Unable to clear "
							"alarm in TOD device.");
				}

				(void) time(now);
				/* wait at least 5 mins */
				shutdown_time = *now +
					((info->pd_idle_time * 60) > 300 ?
					(info->pd_idle_time * 60) : 300);
			} else {
				/* wait 5 mins */
				shutdown_time = *now + 300;
			}
		} else
			shutdown_time = *now + next_time;
	} else if (s < f && *now >= f) {
		shutdown_time = s + DAYS_TO_SECS;
	} else
		shutdown_time = s;
}

static int
is_ok2shutdown(time_t *now)
{
	char	power_cycles_st[80];
	char	power_cycle_limit_st[80];
	char	system_board_date_st[80];
	int	power_cycles, power_cycle_limit, free_cycles, scaled_cycles;
	time_t	life_began, life_passed;
	int	no_power_cycles = 0;
	int	no_system_board_date = 0;

	if (energystar_prop == 0) {
		return (1);
	}

	if (get_prom(root, "power-cycle-limit", power_cycle_limit_st) == 0) {
		power_cycle_limit = DEFAULT_POWER_CYCLE_LIMIT;
	} else {
		power_cycle_limit = atoi(power_cycle_limit_st);
	}

	/*
	 * Allow 10% of power_cycle_limit as free cycles.
	 */
	free_cycles = power_cycle_limit * 0.1;

	if (get_prom(options, "#power-cycles", power_cycles_st) == 0) {
		no_power_cycles++;
	} else {
		power_cycles = atoi(power_cycles_st);
		if (power_cycles < 0) {
			no_power_cycles++;
		} else if (power_cycles <= free_cycles) {
			return (1);
		}
	}
	if (no_power_cycles && log_power_cycles_error == 0) {
		LOGERROR("No or invalid PROM property \"#power-cycles\" "
				"was found.");
		log_power_cycles_error++;
	}

	if (get_prom(options, "system-board-date",
					system_board_date_st) == 0) {
		no_system_board_date++;
	} else {
		life_began = strtol(system_board_date_st, (char **)NULL, 16);
		if (life_began > *now) {
			no_system_board_date++;
		}
	}
	if (no_system_board_date) {
		if (log_system_board_date_error == 0) {
			LOGERROR("No or invalid PROM property \"system-board-"
					"date\" was found.");
			log_system_board_date_error++;
		}
		life_began = DEFAULT_SYSTEM_BOARD_DATE;
#ifdef SETPROM
		(void) sprintf(system_board_date_st, "%lx", life_began);
		(void) set_prom("system-board-date", system_board_date_st);
#endif
	}

	life_passed = *now - life_began;

	/*
	 * Since we don't keep the date that last free_cycle is ended, we
	 * need to spread (power_cycle_limit - free_cycles) over the entire
	 * 7-year life span instead of (lifetime - date free_cycles ended).
	 */
	scaled_cycles = ((float)life_passed / (float)LIFETIME_SECS) *
				(power_cycle_limit - free_cycles);

	if (no_power_cycles) {
#ifdef SETPROM
		(void) sprintf(power_cycles_st, "%d", scaled_cycles);
		(void) set_prom("#power-cycles", power_cycles_st);
#endif
		return (1);
	}
#ifdef DEBUG
	(void) printf("Actual power_cycles = %d\tScaled power_cycles = %d\n",
				power_cycles, scaled_cycles);
#endif
	if (power_cycles > scaled_cycles) {
		if (log_no_autoshutdown_warning == 0) {
			LOGERROR("Automatic shutdown has been temporarily "
				"suspended in order to preserve the reliability"
				" of this system.");
			log_no_autoshutdown_warning++;
		}
		return (0);
	}

	return (1);
}

static void
check_battery(time_t *now)
{
	battery_t	batt_info;
	int		error;
#ifdef LOG
	static int	time_count = 0;
	int		logsize, log_fd;
	char		logbuf[80];
#endif

	if (battery_fd == -1) {
		battery_time = 0;
		return;
	}
	/*
	 * Get information about battery (or lack of it)
	 */
	error = ioctl(battery_fd, BATT_STATUS, &batt_info);
	if (error == -1) {
		battery_time = 0;
		return;
	}
#ifdef LOG
	if (time_count-- == 0) {
		log_fd = open(LOGFILE, O_CREAT | O_RDWR | O_APPEND, 0644);
		if (log_fd == -1) {
			perror("battery logging");
			exit(EXIT_FAILURE);
		}
		logsize = sprintf(logbuf,
		    "%ld: %2.1f (Whrs)   %d%%   %2.1f (W)   %d (mins)   %d"
		    "   %d\n", *now,
		    (float)batt_info.total / 1000, batt_info.capacity,
		    (float)batt_info.discharge_rate / 1000,
		    batt_info.discharge_time / 60, batt_info.status,
		    batt_info.charge);
		write(log_fd, logbuf, logsize);
		close(log_fd);
		time_count = 5;
	}
#endif /* LOG */
	if (batt_info.status == EOL) {
		(void) fprintf(stderr,
				"Battery EOL : Please Replace Battery\n");
		battery_time = 0;
		return;
	} else if (batt_info.status == NOT_PRESENT) {
		info->pd_flags &= ~PD_BATTERY;
		info->pd_flags |= PD_AC;
		info->pd_charge = -1;
		info->pd_time_left = -1;
	} else {	/* The battery is present */
		info->pd_charge = batt_info.capacity;
		info->pd_time_left = batt_info.discharge_time / 60;
		if (batt_info.charge == DISCHARGE) {
			info->pd_flags |= PD_BATTERY;
			info->pd_flags &= ~PD_AC;

			/*
			 * Check for low power system suspend
			 */
			if (batt_info.status == EMPTY ||
			    batt_info.capacity <= 5) {
#ifdef LOG
				log_fd = open(LOGFILE,
				    O_CREAT | O_RDWR | O_APPEND, 0644);
				if (log_fd == -1) {
					perror("battery logging");
					exit(EXIT_FAILURE);
				}
				logsize = sprintf(logbuf,
				    "** Shutoff: status %d, capacity %d%%\n",
				    batt_info.status, batt_info.capacity);
				write(log_fd, logbuf, logsize);
				close(log_fd);
#endif /* LOG */
				poweroff(AD_FORCE, "Low Battery Shutdown\n");
				(void) time(now);
			}
		} else {
			info->pd_flags |= PD_BATTERY | PD_AC;
		}
	}
	battery_time = *now + CHECK_INTERVAL;
}

static void
check_idleness(time_t *now, hrtime_t *hr_now)
{

	/*
	 * Check idleness only when autoshutdown is enabled.
	 */
	if (!autoshutdown_en) {
		checkidle_time = 0;
		return;
	}

	info->pd_ttychars_idle = check_tty(hr_now, ttychars_thold);
	info->pd_loadaverage_idle = check_load_ave(hr_now, loadaverage_thold);
	info->pd_diskreads_idle = check_disks(hr_now, diskreads_thold);
	info->pd_nfsreqs_idle = check_nfs(hr_now, nfsreqs_thold);

#ifdef DEBUG
	(void) printf("Idle ttychars for %d secs.\n", info->pd_ttychars_idle);
	(void) printf("Idle loadaverage for %d secs.\n",
				info->pd_loadaverage_idle);
	(void) printf("Idle diskreads for %d secs.\n", info->pd_diskreads_idle);
	(void) printf("Idle nfsreqs for %d secs.\n", info->pd_nfsreqs_idle);
#endif

	checkidle_time = *now + IDLECHK_INTERVAL;
}

static int
last_system_activity(hrtime_t *hr_now)
{
	int	act_idle, latest;

	latest = info->pd_idle_time * 60;
	act_idle = last_tty_activity(hr_now, ttychars_thold);
	latest = MIN(latest, act_idle);
	act_idle = last_load_ave_activity(hr_now);
	latest = MIN(latest, act_idle);
	act_idle = last_disk_activity(hr_now, diskreads_thold);
	latest = MIN(latest, act_idle);
	act_idle = last_nfs_activity(hr_now, nfsreqs_thold);
	latest = MIN(latest, act_idle);

	return (latest);
}

static int
run_idlecheck()
{
	char		pm_variable[80], script[80];
	char		*cp;
	int		status;
	pid_t		child;

	/*
	 * Reap any child process which has been left over.
	 */
	while (waitpid((pid_t)-1, &status, WNOHANG) > 0);

	/*
	 * Execute the user's idlecheck script and set variable PM_IDLETIME.
	 * Returned exit value is the idle time in minutes.
	 */
	if ((child = fork()) == 0) {
		(void) sprintf(pm_variable, "PM_IDLETIME=%d",
			info->pd_idle_time);
		(void) putenv(pm_variable);
		cp = strrchr(idlecheck_path, '/');
		(void *) strcpy(script, ++cp);
		(void) execl(idlecheck_path, script, NULL);
		exit(-1);
	} else if (child == -1) {
		return (info->pd_idle_time * 60);
	}

	/*
	 * Wait until the idlecheck program completes.
	 */
	if (waitpid(child, &status, 0) != child) {
		/*
		 * We get here if the calling process gets a signal.
		 */
		return (info->pd_idle_time * 60);
	}

	if (WEXITSTATUS(status) < 0) {
		return (info->pd_idle_time * 60);
	} else {
		return (WEXITSTATUS(status) * 60);
	}
}

static void
set_alarm(time_t now)
{
	time_t	btime, itime, stime, next_time, max_time;
	int	next_alarm;

	max_time = MAX(MAX(battery_time, checkidle_time), shutdown_time);
	if (max_time == 0) {
		(void) alarm(0);
		return;
	}
	btime = (battery_time == 0) ? max_time : battery_time;
	itime = (checkidle_time == 0) ? max_time : checkidle_time;
	stime = (shutdown_time == 0) ? max_time : shutdown_time;
	next_time = MIN(MIN(btime, itime), stime);
	next_alarm = (next_time <= now) ? 1 : (next_time - now);
	(void) alarm(next_alarm);

#ifdef DEBUG
	(void) printf("Currently @ %s", ctime(&now));
	(void) printf("Battery   in %d secs\n", battery_time - now);
	(void) printf("Checkidle in %d secs\n", checkidle_time - now);
	(void) printf("Shutdown  in %d secs\n", shutdown_time - now);
	(void) printf("Next alarm goes off in %d secs\n", next_alarm);
	(void) printf("************************************\n");
#endif
}

static void
poweroff(int mode, char *msg)
{
	struct stat	statbuf;
	pid_t		pid, child;
	struct passwd	*pwd;
	char		home[256] = "HOME=";
	char		openwinhome[256] = "OPENWINHOME=";
	char		display[256] = "DISPLAY=";
	char		user[256] = "LOGNAME=";
	int		status;

	if (stat("/dev/console", &statbuf) == -1 ||
	    (pwd = getpwuid(statbuf.st_uid)) == NULL)
		return;

	if (broadcast)
		(void) syslog(LOG_ERR, msg);

	/*
	 * Need to simulate the user enviroment to talk to X server
	 * minimaly need to set HOME, DISPLAY, USER, OPENWINHOME
	 */
	if ((child = fork()) == 0) {
		(void) strcat(home, pwd->pw_dir);
		(void) putenv(home);
		(void) strcat(user, pwd->pw_name);
		(void) putenv(user);
		if (getenv("OPENWINHOME") == NULL) {
			(void) strcat(openwinhome, "/usr/openwin");
			(void) putenv(openwinhome);
		}
		if (getenv("DISPLAY") == NULL) {
			(void) strcat(display, ":0.0");
			(void) putenv(display);
		}
		(void) setgid(statbuf.st_gid);
		(void) setuid(statbuf.st_uid);
		if (mode == AD_FORCE)
			(void) execl(SUSPEND, "sys-suspend", "-fx", NULL);
		else
			(void) execl(SUSPEND, "sys-suspend", "-nx", NULL);
		exit(EXIT_FAILURE);
	} else if (child == -1)
		return;
	pid = 0;
	while (pid != child)
		pid = wait(&status);
	if (WEXITSTATUS(status) == EXIT_FAILURE && broadcast)
		(void) syslog(LOG_ERR, "exec %s failed\n", SUSPEND);
}

#define	PBUFSIZE	256

/*
 * Gets the value of a prom property at either root or options node.  It
 * returns 1 if it is successful, otherwise it returns 0 .
 */
static int
get_prom(prom_node_t node_name, char *property_name, char *property_value)
{
	union {
		char buf[PBUFSIZE + sizeof (u_int)];
		struct openpromio opp;
	} oppbuf;
	register struct openpromio *opp = &(oppbuf.opp);
	int	got_it = 0;

	if (prom_fd == -1) {
		return (0);
	}

	switch (node_name) {
	case root:
		(void *) memset(oppbuf.buf, 0, PBUFSIZE);
		opp->oprom_size = PBUFSIZE;
		if (ioctl(prom_fd, OPROMNEXT, opp) < 0) {
			return (0);
		}

		/*
		 * Passing null string will give us the first property.
		 */
		(void *) memset(oppbuf.buf, 0, PBUFSIZE);
		do {
			opp->oprom_size = PBUFSIZE;
			if (ioctl(prom_fd, OPROMNXTPROP, opp) < 0) {
				return (0);
			}
			if (strcmp(opp->oprom_array, property_name) == 0) {
				got_it++;
				break;
			}
		} while (opp->oprom_size > 0);

		if (!got_it) {
			return (0);
		}
		if (got_it && property_value == NULL) {
			return (1);
		}
		opp->oprom_size = PBUFSIZE;
		if (ioctl(prom_fd, OPROMGETPROP, opp) < 0) {
			return (0);
		}
		if (opp->oprom_size == 0) {
			*property_value = '\0';
		} else {
			(void *) strcpy(property_value, opp->oprom_array);
		}
		break;
	case options:
		(void) strcpy(opp->oprom_array, property_name);
		opp->oprom_size = PBUFSIZE;
		if (ioctl(prom_fd, OPROMGETOPT, opp) < 0) {
			return (0);
		}
		if (opp->oprom_size <= 0) {
			return (0);
		}
		if (property_value != NULL) {
			(void *) strcpy(property_value, opp->oprom_array);
		}
		break;
	default:
		LOGERROR("Only root node and options node are supported.\n");
		return (0);
	}

	return (1);
}

/*
 *  Sets the given prom property at the options node.  Returns 1 if it is
 *  successful, otherwise it returns 0.
 */
#ifdef SETPROM
static int
set_prom(char *property_name, char *property_value)
{
	union {
		char buf[PBUFSIZE];
		struct openpromio opp;
	} oppbuf;
	register struct openpromio *opp = &(oppbuf.opp);
	int name_length;

	if (prom_fd == -1) {
		return (0);
	}

	name_length = strlen(property_name) + 1;
	(void) strcpy(opp->oprom_array, property_name);
	(void) strcpy(opp->oprom_array + name_length, property_value);
	opp->oprom_size = name_length + strlen(property_value);
	if (ioctl(prom_fd, OPROMSETOPT, opp) < 0) {
		return (0);
	}

	/*
	 * Get back the property value in order to verify the set operation.
	 */
	opp->oprom_size = PBUFSIZE;
	if (ioctl(prom_fd, OPROMGETOPT, opp) < 0) {
		return (0);
	}
	if (opp->oprom_size <= 0) {
		return (0);
	}
	if (strcmp(opp->oprom_array, property_value) != 0) {
		return (0);
	}

	return (1);
}
#endif  /* SETPROM */

#define	iseol(ch)	((ch) == '\n' || (ch) == '\r' || (ch) == '\f')

static int
parse_line(FILE *file, char *buf)
{
	register int ch;
	char *cp;

	if ((ch = getc(file)) == EOF) {
		return (EOF);
	} else {
		(void) ungetc(ch, file);
	}

	cp = buf;
	while ((ch = getc(file)) != EOF && !iseol(ch)) {
		if (ch == '\\') {
			ch = getc(file);
			if (iseol(ch)) {
				continue;
			} else {
				(void) ungetc(ch, file);
				ch = '\\';
			}
		}
		*cp = ch;
		cp++;
	}
	*cp = '\0';

	return (0);
}
