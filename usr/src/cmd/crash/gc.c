#ident	"@(#)gc.c	1.1	95/11/13 SMI"

/*
 *		Copyright (C) 1995  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

#include <stddef.h>
#include <malloc.h>
#include "gc.h"

/*
 * This file contains a primitive garbage-collecting allocator.
 * Because it is a pain to constantly have to free all of the things
 * we have allocated (particularly in error situations, where we have
 * allocated some things but not others), this allocator can simplify
 * life a bit.  It remembers all of the things allocated through it
 * and can free them all at once with a single call.  It is useful,
 * for example, in interactive utilities which need to allocate memory
 * to process a command, but much of that memory is no longer needed
 * when the command is finished.  Transaction processing of any kind
 * is a candidate for this kind of garbage collection.
 *
 * gc_create()    sets up a garbage can
 * gc_malloc()     malloc equivalent
 * gc_realloc()   realloc equivalent
 * gc_calloc()     calloc equivalent
 * gc_free()         free equivalent
 * gc_free_all()  frees all memory in a garbage can
 * gc_move()      moves memory from one garbage can to another
 * gc_move_all()  moves all memory from one garbage can to another
 * gc_destroy()   undoes a garbage can (and frees everything in it)
 *
 * gc_dump() is only present if compiled with DEBUG defined.  It dumps
 * the addresses of all buffers in the gc.
 *
 * The implementation simply maintains a doubly-linked list of all of
 * the buffers allocated; extra space for the pointers is allocated
 * at the front of each buffer.
 *
 * gc_move() and gc_move_all() are useful when you want to keep all of the
 * memory around if you are able to successfully complete a transaction,
 * but want to free it all if it can't be completed.  You can code
 * something like this:
 *
 *	gc_malloc(tentative, ...);
 *	 <many of those>
 *	if (error)
 *		gc_free_all(tentative);
 *	gc_move_all(tentative, permanent);
 *
 * Note that malloc/realloc/calloc promise to return memory suitably
 * aligned for any use.  I couldn't find a macro in a public header
 * file that said what that alignment should be, but I know that it is
 * currently 8.  Perhaps future machines will require 16-byte alignment?
 * This package aligns memory to a boundary of 2 * the size of a pointer.
 * That is currently 8 bytes, and will become 16 bytes when user code
 * gets 64-bit pointers (which I think will happen at least as soon as
 * "suitable for any purpose" means 16-byte alignment, but then what do
 * I know).
 */


/*
 * Links for implementing a doubly-linked list.  The gc_info block has
 * one of these and serves as the head of a list of allocated blocks,
 * each of which has one of these tacked on to the front of it.
 *
 * For the list head, the names "head" and "tail" make the code more
 * readily understood.  Because C doesn't have C++'s anonymous unions,
 * we'll just use #define.
 */
typedef struct links links_t;
struct links {
	links_t *next;
	links_t *prev;
};
#define	head next
#define	tail prev

/*
 * This is the information block associated with a garbage can.  We give
 * the user a pointer to one of these as an opaque handle.  We could just
 * use a links_t for this, but someday we might want to throw something
 * else in the header.
 */
struct gc_handle {
	links_t links;    /* head of list of alloc'ed blocks */
};

/*
 * In the routines below, "old" is the name given to a pointer to a block
 * of memory passed to us by the user.  After moving the pointer back to
 * the links_t structure, we can use this macro to access its members.
 */
#define	OLD ((links_t *)old)


/* Create and initialize a gc. */
gc_handle_t
gc_create()
{
	gc_handle_t  gc;

	/* Allocate an info block to hold this stuff in. */
	gc = (gc_handle_t) malloc(sizeof (struct gc_handle));
	if (!gc)
		return (NULL);

	/* Fill in the remaining fields and return the info block. */
	gc->links.head = gc->links.tail = &gc->links;
	return ((gc_handle_t) gc);
}


/* Malloc a block. */
void *
gc_malloc(gc_handle_t gc, size_t size)
{
	links_t  *mem;

	/* Allocate the requested memory plus space for a links_t. */
	mem = malloc(sizeof (links_t) + size);
	if (!mem)
		return (NULL);

	/* Link it on to the end of the list, and return it. */
	mem->next = &gc->links;
	mem->prev = gc->links.tail;
	gc->links.tail->next = mem;
	gc->links.tail = mem;
	return (mem+1);
}


/* Realloc a block; it may move. */
void *
gc_realloc(gc_handle_t gc, void *old, size_t size)
{
	links_t  *mem;

	/* If no old memory is specified, then this is a malloc. */
	if (!old)
		return (gc_malloc(gc, size));

	/* If size is 0, then this is a free. */
	if (!size) {
		gc_free(gc, old);
		return (NULL);
	}

	/* Fix the user's pointer to point to the header we added. */
	old = OLD-1;

	/* Reallocate the requested memory. */
	mem = realloc(OLD, size);
	if (!mem)
		return (NULL);

	/* Fix the pointers to it in the list, and return it. */
	if (mem != OLD)
		OLD->next->prev = OLD->prev->next = mem;
	return (mem + 1);
}


/* Malloc a block. */
void *
gc_calloc(gc_handle_t gc, size_t num, size_t size_each)
{
	links_t  *mem;
	size_t    size = num * size_each;

	/* Allocate the requested memory using our malloc-equivalent. */
	mem = gc_malloc(gc, size);
	if (!mem)
		return (NULL);

	/* Zero it and return it. */
	bzero(mem, size);
	return (mem);
}


/* Free a single block (probably won't be used much). */
void
gc_free(gc_handle_t gc, void *old)
{
	/* Fix the user's pointer to point to the header we added. */
	old = OLD-1;

	/* Unlink it from the list and free it. */
	OLD->prev->next = OLD->next;
	OLD->next->prev = OLD->prev;
	free(OLD);
}


/* Free all blocks allocated (the point of all of this). */
void
gc_free_all(gc_handle_t gc)
{
	links_t  *p = gc->links.head;

	/* Free all of the allocated blocks on the list. */
	while (p != &gc->links) {
		links_t *next = p->next;
		free(p);
		p = next;
	}

	/* Fix the header links. */
	gc->links.head = gc->links.tail = &gc->links;
}


/* Move contents of one gc to another. */
void
gc_move(gc_handle_t gc_from, void *old, gc_handle_t gc_to)
{
	/* Fix the user's pointer to point to the header we added. */
	old = OLD-1;

	/* Unlink the buffer from the list it's on. */
	OLD->prev->next = OLD->next;
	OLD->next->prev = OLD->prev;

	/* Add it to the end of the TO list. */
	OLD->prev = gc_to->links.tail;
	OLD->next = &gc_to->links;
	gc_to->links.tail->next = OLD;
	gc_to->links.tail = OLD;
}


/* Move contents of one gc to another. */
void
gc_move_all(gc_handle_t gc_from, gc_handle_t gc_to)
{
	links_t *hd;
	links_t *tl;

#define	FROM (&gc_from->links)
#define	TO   (&gc_to  ->links)

	/* If FROM is empty, just return. */
	if (FROM->head == FROM)
		return;

	/* Unlink everything from the FROM list. */
	hd = FROM->head;
	tl = FROM->tail;
	FROM->head = FROM->tail = FROM;

	/* Add it to the end of the TO list. */
	hd->prev = TO->tail;
	tl->next = TO;
	TO->tail->next = hd;
	TO->tail = tl;
}


/* Destructor for a gc. */
void
gc_destroy(gc_handle_t gc)
{
	gc_free_all(gc);
	free(gc);
}


#ifdef DEBUG
/* Show all blocks allocated. */
#include <stdio.h>
void
gc_dump(gc_handle_t gc)
{
	links_t  *p = gc->links.head;
	int  i = 0;

	fprintf(stderr, "gc_dump(0x%08x):\n", gc);

	/* Show the address of each allocated block on the list. */
	while (p != &gc->links) {
		fprintf(stderr, "  %4d  0x%08x\n", i++, p);
		p = p->next;
	}
}
#endif
