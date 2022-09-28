#ifdef	lint
void div0trap(void);
void dbgtrap(void);
void nodbgmon(void);
void nmiint(void);
void brktrap(void);
void ovflotrap(void);
void boundstrap(void);
void invoptrap(void);
void ndptrap0(void);
void dbfault(void);
void overrun(void);
void invtsstrap(void);
void segnptrap(void);
void stktrap(void);
void gptrap(void);
void pftrap(void);
void resvtrap(void);
void ndperr(void);
void inval17(void);
void inval18(void);
void inval19(void);
void progent(void);
void inval21(void);
void inval22(void);
void inval23(void);
void inval24(void);
void inval25(void);
void inval26(void);
void inval27(void);
void inval28(void);
void inval29(void);
void inval30(void);
void inval31(void);
void ndptrap2(void);
void inval33(void);
void inval34(void);
void inval35(void);
void inval36(void);
void inval37(void);
void inval38(void);
void inval39(void);
void inval40(void);
void inval41(void);
void inval42(void);
void inval43(void);
void inval44(void);
void inval45(void);
void inval46(void);
void inval47(void);
void inval48(void);
void inval49(void);
void inval50(void);
void inval51(void);
void inval52(void);
void inval53(void);
void inval54(void);
void inval55(void);
void inval56(void);
void inval57(void);
void inval58(void);
void inval59(void);
void inval60(void);
void inval61(void);
void inval62(void);
void inval63(void);
void ivctM0(void);
void ivctM1(void);
void ivctM2(void);
void ivctM3(void);
void ivctM4(void);
void ivctM5(void);
void ivctM6(void);
void ivctM7(void);
void ivctM0S0(void);
void ivctM0S1(void);
void ivctM0S2(void);
void ivctM0S3(void);
void ivctM0S4(void);
void ivctM0S5(void);
void ivctM0S6(void);
void ivctM0S7(void);
void ivctM1S0(void);
void ivctM1S1(void);
void ivctM1S2(void);
void ivctM1S3(void);
void ivctM1S4(void);
void ivctM1S5(void);
void ivctM1S6(void);
void ivctM1S7(void);
void ivctM2S0(void);
void ivctM2S1(void);
void ivctM2S2(void);
void ivctM2S3(void);
void ivctM2S4(void);
void ivctM2S5(void);
void ivctM2S6(void);
void ivctM2S7(void);
void ivctM3S0(void);
void ivctM3S1(void);
void ivctM3S2(void);
void ivctM3S3(void);
void ivctM3S4(void);
void ivctM3S5(void);
void ivctM3S6(void);
void ivctM3S7(void);
void ivctM4S0(void);
void ivctM4S1(void);
void ivctM4S2(void);
void ivctM4S3(void);
void ivctM4S4(void);
void ivctM4S5(void);
void ivctM4S6(void);
void ivctM4S7(void);
void ivctM5S0(void);
void ivctM5S1(void);
void ivctM5S2(void);
void ivctM5S3(void);
void ivctM5S4(void);
void ivctM5S5(void);
void ivctM5S6(void);
void ivctM5S7(void);
void ivctM6S0(void);
void ivctM6S1(void);
void ivctM6S2(void);
void ivctM6S3(void);
void ivctM6S4(void);
void ivctM6S5(void);
void ivctM6S6(void);
void ivctM6S7(void);
void ivctM7S0(void);
void ivctM7S1(void);
void ivctM7S2(void);
void ivctM7S3(void);
void ivctM7S4(void);
void ivctM7S5(void);
void ivctM7S6(void);
void ivctM7S7(void);
void invaltrap(void);
#else
/
/	intr.s	-- Second level boot protected mode interrupt handling 
/			(a.k.a, purposeful panics)
/
/	ident "@(#)intr.s	1.3     96/03/27 SMI"
/
/
	.text

	.align	4
	.globl	div0trap
div0trap:
	movl	$0,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	dbgtrap
dbgtrap:
nodbgmon:
	movl	$1,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	nmiint
nmiint:
	movl	$2,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	brktrap
brktrap:
	movl	$3,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ovflotrap
ovflotrap:
	movl	$4,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	boundstrap
boundstrap:
	movl	$5,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	invoptrap
invoptrap:
	movl	$6,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ndptrap0
ndptrap0:
	movl	$7,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	dbfault
dbfault:
	movl	$8,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	overrun
overrun:
	movl	$9,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	invtsstrap
invtsstrap:
	movl	$10,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	segnptrap
segnptrap:
	movl	$11,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	stktrap
stktrap:
	movl	$12,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	gptrap
gptrap:
	movl	$13,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	pftrap
pftrap:
	movl	$14,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	resvtrap
resvtrap:
	movl	$15,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ndperr
ndperr:
	movl	$16,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval17
inval17:
	movl	$17,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval18
inval18:
	movl	$18,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval19
inval19:
	movl	$19,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	progent
progent:
	movl	$20,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval21
inval21:
	movl	$21,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval22
inval22:
	movl	$22,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval23
inval23:
	movl	$23,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval24
inval24:
	movl	$24,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval25
inval25:
	movl	$25,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval26
inval26:
	movl	$26,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval27
inval27:
	movl	$27,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval28
inval28:
	movl	$28,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval29
inval29:
	movl	$29,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval30
inval30:
	movl	$30,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval31
inval31:
	movl	$31,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ndptrap2
ndptrap2:
	movl	$32,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval33
inval33:
	movl	$33,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval34
inval34:
	movl	$34,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval35
inval35:
	movl	$35,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval36
inval36:
	movl	$36,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval37
inval37:
	movl	$37,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval38
inval38:
	movl	$38,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval39
inval39:
	movl	$39,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval40
inval40:
	movl	$40,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval41
inval41:
	movl	$41,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval42
inval42:
	movl	$42,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval43
inval43:
	movl	$43,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval44
inval44:
	movl	$44,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval45
inval45:
	movl	$45,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval46
inval46:
	movl	$46,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval47
inval47:
	movl	$47,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval48
inval48:
	movl	$48,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval49
inval49:
	movl	$49,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval50
inval50:
	movl	$50,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval51
inval51:
	movl	$51,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval52
inval52:
	movl	$52,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval53
inval53:
	movl	$53,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval54
inval54:
	movl	$54,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval55
inval55:
	movl	$55,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval56
inval56:
	movl	$56,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval57
inval57:
	movl	$57,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval58
inval58:
	movl	$58,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval59
inval59:
	movl	$59,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval60
inval60:
	movl	$60,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval61
inval61:
	movl	$61,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval62
inval62:
	movl	$62,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	inval63
inval63:
	movl	$63,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM0
ivctM0:
	movl	$64,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM1
ivctM1:
	movl	$65,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM2
ivctM2:
	movl	$66,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM3
ivctM3:
	movl	$67,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM4
ivctM4:
	movl	$68,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM5
ivctM5:
	movl	$69,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM6
ivctM6:
	movl	$70,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM7
ivctM7:
	movl	$71,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM0S0
ivctM0S0:
	movl	$72,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM0S1
ivctM0S1:
	movl	$73,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM0S2
ivctM0S2:
	movl	$74,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM0S3
ivctM0S3:
	movl	$75,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM0S4
ivctM0S4:
	movl	$76,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM0S5
ivctM0S5:
	movl	$77,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM0S6
ivctM0S6:
	movl	$78,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM0S7
ivctM0S7:
	movl	$79,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM1S0
ivctM1S0:
	movl	$80,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM1S1
ivctM1S1:
	movl	$81,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM1S2
ivctM1S2:
	movl	$82,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM1S3
ivctM1S3:
	movl	$83,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM1S4
ivctM1S4:
	movl	$84,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM1S5
ivctM1S5:
	movl	$85,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM1S6
ivctM1S6:
	movl	$86,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM1S7
ivctM1S7:
	movl	$87,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM2S0
ivctM2S0:
	movl	$88,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM2S1
ivctM2S1:
	movl	$89,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM2S2
ivctM2S2:
	movl	$90,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM2S3
ivctM2S3:
	movl	$91,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM2S4
ivctM2S4:
	movl	$92,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM2S5
ivctM2S5:
	movl	$93,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM2S6
ivctM2S6:
	movl	$94,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM2S7
ivctM2S7:
	movl	$95,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM3S0
ivctM3S0:
	movl	$96,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM3S1
ivctM3S1:
	movl	$97,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM3S2
ivctM3S2:
	movl	$98,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM3S3
ivctM3S3:
	movl	$99,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM3S4
ivctM3S4:
	movl	$100,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM3S5
ivctM3S5:
	movl	$101,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM3S6
ivctM3S6:
	movl	$102,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM3S7
ivctM3S7:
	movl	$103,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM4S0
ivctM4S0:
	movl	$104,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM4S1
ivctM4S1:
	movl	$105,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM4S2
ivctM4S2:
	movl	$106,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM4S3
ivctM4S3:
	movl	$107,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM4S4
ivctM4S4:
	movl	$108,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM4S5
ivctM4S5:
	movl	$109,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM4S6
ivctM4S6:
	movl	$110,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM4S7
ivctM4S7:
	movl	$111,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM5S0
ivctM5S0:
	movl	$112,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM5S1
ivctM5S1:
	movl	$113,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM5S2
ivctM5S2:
	movl	$114,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM5S3
ivctM5S3:
	movl	$115,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM5S4
ivctM5S4:
	movl	$116,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM5S5
ivctM5S5:
	movl	$117,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM5S6
ivctM5S6:
	movl	$118,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM5S7
ivctM5S7:
	movl	$119,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM6S0
ivctM6S0:
	movl	$120,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM6S1
ivctM6S1:
	movl	$121,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM6S2
ivctM6S2:
	movl	$122,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM6S3
ivctM6S3:
	movl	$123,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM6S4
ivctM6S4:
	movl	$124,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM6S5
ivctM6S5:
	movl	$125,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM6S6
ivctM6S6:
	movl	$126,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM6S7
ivctM6S7:
	movl	$127,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM7S0
ivctM7S0:
	movl	$128,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM7S1
ivctM7S1:
	movl	$129,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM7S2
ivctM7S2:
	movl	$130,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM7S3
ivctM7S3:
	movl	$131,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM7S4
ivctM7S4:
	movl	$132,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM7S5
ivctM7S5:
	movl	$133,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM7S6
ivctM7S6:
	movl	$134,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	ivctM7S7
ivctM7S7:
	movl	$135,%ds:[pp_trapno]
	jmp	protpanic

	.align	4
	.globl	invaltrap
invaltrap:
	movl	$255,%ds:[pp_trapno]
	jmp	protpanic
#endif	/* !lint */
