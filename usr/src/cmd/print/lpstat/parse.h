/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */
 
#pragma ident "@(#)parse.h   1.2     96/04/22 SMI"


typedef struct _print_queue {
  ns_bsd_addr_t *binding;
  char *status;
  enum { RAW, IDLE, PRINTING, FAULTED, DISABLED } state;
  job_t **jobs;
} print_queue_t;

extern job_t *parse_bsd_entry(char *data);
extern print_queue_t *parse_bsd_queue(ns_bsd_addr_t *binding, char *data,
				      int len);
