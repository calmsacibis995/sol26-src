#include <sys/machtypes.h>
#include <sys/t_lock.h>
#include <sys/mutex_impl.h>

adaptive_mutex
.>f
#if defined(__ppc)
./"owner"n;(*.&0xfffffffc)=X
<f/"waiters"8t"wlock"8t"type"n{m_waiters,x}{m_wlock,B}{m_type,B}{END}
#else
#ifdef sparc
#ifdef sun4u
<f/"owner/lock"nX
#else
<f/"owner"n;*.*20=X
<f/"lock"n;*.%1000000=X
#endif
#endif
#ifdef i386
<f/"owner"n;*.=X
<f/"lock"n;*.%8000000=X
#endif
<f/"waiters"8t"wlock"8t"type"n{m_waiters,x}{m_wlock,B}{m_type,B}{END}
#endif
