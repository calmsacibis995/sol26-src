#ident	"@(#)sparc.il	1.5	96/05/26 SMI"
!
! In-line functions for sparc kernels.
!

! return current thread pointer

	.inline	threadp,0
	mov	%g7, %o0
	.end

! needed by krtld

	.inline doflush,0
	andn	%o0, 3, %o0
	flush	%o0
	.end
!
! extern long long load_double(long long *);
!
        .inline load_double,4
        ldd     [%o0],%o0
        .end

!
! extern long long store_double(long long, long long *);
!
        .inline store_double,12
        std     %o0,[%o2]
        .end

