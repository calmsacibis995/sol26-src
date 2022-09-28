#include <sys/types.h>
#if defined(sparc)
#	include <sys/privregs.h>
#elif defined(i386) || defined(__ppc)
#	include <sys/reg.h>
#endif

regs
#if defined(sparc)
#if defined(sun4m) || defined(sun4c) || defined(sun4d)
./"psr"16t"pc"16t"npc"n{r_ps,X}{r_pc,X}{r_npc,X}
+/"y"16t"g1"16t"g2"16t"g3"n{r_y,X}{r_g1,X}{r_g2,X}{r_g3,X}
+/"g4"16t"g5"16t"g6"16t"g7"n{r_g4,X}{r_g5,X}{r_g6,X}{r_g7,X}
+/"o0"16t"o1"16t"o2"16t"o3"n{r_o0,X}{r_o1,X}{r_o2,X}{r_o3,X}
+/"o4"16t"o5"16t"o6"16t"o7"n{r_o4,X}{r_o5,X}{r_o6,X}{r_o7,X}
#endif /* sun4m/sun4c/sun4d */
#if defined(sun4u)
./"tstate"16t"pc"16t"npc"n{r_tstate,2X}{r_pc,X}{r_npc,X}
+/"y"16t"g1"16t"g2"16t"g3"n{r_y,X}{r_g1,2X}{r_g2,2X}{r_g3,2X}
+/"g4"16t"g5"16t"g6"16t"g7"n{r_g4,2X}{r_g5,2X}{r_g6,2X}{r_g7,2X}
+/"o0"16t"o1"16t"o2"16t"o3"n{r_o0,2X}{r_o1,2X}{r_o2,2X}{r_o3,2X}
+/"o4"16t"o5"16t"o6"16t"o7"n{r_o4,2X}{r_o5,2X}{r_o6,2X}{r_o7,2X}
#endif /* sun4m/sun4c/sun4d */

#elif defined(i386)
./"gs"16t"fs"16t"es"16t"ds"n{r_gs,X}{r_fs,X}{r_es,X}{r_ds,X}
+/"edi"16t"esi"16t"ebp"16t"esp"n{r_edi,X}{r_esi,X}{r_ebp,X}{r_esp,X}
+/"ebx"16t"edx"16t"ecx"16t"eax"n{r_ebx,X}{r_edx,X}{r_ecx,X}{r_eax,X}
+/"trapno"16t"err"16t"eip"16t"cs"n{r_trapno,X}{r_err,X}{r_eip,X}{r_cs,X}
+/"efl"16t"uesp"16t"ss"n{r_efl,X}{r_uesp,X}{r_ss,X}
#elif defined(__ppc)
./"r0"16t"r1"16t"r2"16t"r3"n{r_r0,X}{r_r1,X}{r_r2,X}{r_r3,X}
+/"r4"16t"r5"16t"r6"16t"r7"n{r_r4,X}{r_r5,X}{r_r6,X}{r_r7,X}
+/"r8"16t"r9"16t"r10"16t"r11"n{r_r8,X}{r_r9,X}{r_r10,X}{r_r11,X}
+/"r12"16t"r13"16t"r14"16t"r15"n{r_r12,X}{r_r13,X}{r_r14,X}{r_r15,X}
+/"r16"16t"r17"16t"r18"16t"r19"n{r_r16,X}{r_r17,X}{r_r18,X}{r_r19,X}
+/"r20"16t"r21"16t"r22"16t"r23"n{r_r20,X}{r_r21,X}{r_r22,X}{r_r23,X}
+/"r24"16t"r25"16t"r26"16t"r27"n{r_r24,X}{r_r25,X}{r_r26,X}{r_r27,X}
+/"r28"16t"r29"16t"r30"16t"r31"n{r_r28,X}{r_r29,X}{r_r30,X}{r_r31,X}
+/"cr"16t"lr"16t"pc"16t"msr"n{r_cr,X}{r_lr,X}{r_pc,X}{r_msr,X}
+/"ctr"16t"xer"n{r_ctr,X}{r_xer,X}
#else
#error Unknown architecture!
#endif
