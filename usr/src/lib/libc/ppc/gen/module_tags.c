/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)module_tags.c	1.4	96/07/31 SMI"

#include <synch.h>
#include <module_tags.h>
#include "synonyms.h"

typedef struct __module_tags *m_t_t;

typedef union __ppc_tags *p_t_t;

static struct __module_tags list_head = {&list_head, &list_head, 0, 0, 0, 0};
static mutex_t lock;

static void *
find_tag_base_address(void *first_addr, void *addr)
{
	unsigned long base = 0;
	signed long delta;
	unsigned long start_offset;
	union __ppc_tags *tag = (union __ppc_tags *)addr;
	unsigned long tag_addr = (unsigned long)addr;

	switch (tag->generic.type) {
	case 0:
		delta = (signed long) (tag->frame.base_offset << 2);
		base = tag_addr  + delta;
		break;
	case 1:
		delta = (signed long) (tag->frame_valid.base_offset << 8);
		delta = delta >> 6;
		base = tag_addr + delta;
		break;
	case 2:
		start_offset = tag->register_valid.start_offset << 2;
		while ((tag->generic.type  == 2) &&
		    (((unsigned long) tag) > ((unsigned long) first_addr))) {
			tag -= 1;
		}
		base = (tag->generic.type  == 2)?0:
		    (unsigned long) find_tag_base_address(first_addr, addr);
		base = base + start_offset;
		break;
	case 3:
		delta = (signed long) (tag->special.base_offset << 2);
		base = tag_addr + delta;
		break;
	}
	return ((void *) base);
}

static void *
find_last_tags_address(void *first, void *last)
{
	unsigned long last_pc;
	unsigned long base;
	union __ppc_tags *tag = (union __ppc_tags *)last;

	base = (unsigned long)find_tag_base_address(first, last);
	switch (tag->generic.type) {
	case 0:
		last_pc = base + (tag->frame.frame_start << 2) +
		    (tag->frame.range << 2);
		break;
	case 1:
		last_pc = base + (tag->frame_valid.frame_start << 2) +
		    (tag->frame_valid.range << 2);
		break;
	case 2:
		last_pc = base + (tag->register_valid.range << 2);
		break;
	case 3:
		last_pc = base + (tag->special.range << 2);
		break;
	}

	return ((void *) last_pc);
}

static void
verify_pc_values(m_t_t mt)
{
	union __ppc_tags *last_tag;

	if (mt->firstpc == 0) {
		mt->firstpc = find_tag_base_address(mt->firsttag, mt->firsttag);
	}
	if (mt->lastpc == 0) {
		last_tag = ((union __ppc_tags *)mt->lasttag) -1;
		mt->lastpc = find_last_tags_address(mt->firsttag, last_tag);
	}
}

void
__add_module_tags(m_t_t mt)
{
	m_t_t cur;

	if (__threaded)
		_mutex_lock(&lock);

	if ((mt->firsttag == mt->lasttag) || (mt->firsttag == 0) ||
	    (mt->lasttag == 0)) {
		/*
		 * if null record make into a list unto itself so delete doesn't
		 * explode, but it isn't included in search
		 */
		mt->next = mt->prev = mt;
		if (__threaded)
			_mutex_unlock(&lock);
		return;
	}

	cur = list_head.next;
	while ((((unsigned long)cur->firsttag) <
	    ((unsigned long)mt->firsttag)) && (cur != &list_head))
		cur = cur->next;

	mt->next = cur;
	mt->prev = cur->prev;
	cur->prev->next = mt;
	cur->prev = mt;

	verify_pc_values(mt);

	if (__threaded)
		_mutex_unlock(&lock);
}

void
__delete_module_tags(m_t_t mt)
{
	if (__threaded)
		_mutex_lock(&lock);

	if ((mt->prev != 0) && (mt->next != 0)) {
		mt->prev->next = mt->next;
		mt->next->prev = mt->prev;
	}

	if (__threaded)
		_mutex_unlock(&lock);
}

m_t_t
__tag_lookup_pc(caddr_t pc)
{
	m_t_t cur;

	if (__threaded)
		_mutex_lock(&lock);

	cur = list_head.next;
	while (cur != &list_head) {
		if (((unsigned long) pc) < ((unsigned long) cur->firstpc)) {
			if (__threaded)
				_mutex_unlock(&lock);
			return ((m_t_t)0);
		}
		if (((unsigned long)pc) <= ((unsigned long)cur->lastpc)) {
			if (__threaded)
				_mutex_unlock(&lock);
			return (cur);
		}
		cur = cur->next;
	}

	if (__threaded)
		_mutex_unlock(&lock);
	return ((m_t_t)0);
}

#ifdef notyet
/*
 * This non-ABI routine has been inserted to allow ppc programs which
 * which dump their state for re-execution to undo the module list
 * initialization by cutting the list head out of the list and
 * re-initializing it
 */
void
__delete_all_module_tags(void)
{
	/* if the list is already empty, all 4 statements are NOP's */
	list_head.next->prev = list_head.prev;
	list_head.prev->next = list_head.next;
	list_head.next = &list_head;
	list_head.prev = &list_head;
}
#endif
