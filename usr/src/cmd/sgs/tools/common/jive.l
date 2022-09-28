%e 2000
%p 5000
%n 1000
%k 500
%a 4000
%o 2000
BW [ 	]
EW [ 	.,;!?]

%{
	char buf[128];

%}

%%
file				printf(" stash");
send				printf(" t'row");
program				printf(" honky code");
atlas				printf(" Isaac");
unix				printf(" slow mo-fo");
UNIX				printf(" that slow mo-fo");
takes				printf(" snatch'd");
Mexican				printf(" wet-back");
mexican				printf(" wet-back");
Italian				printf(" greaser");
italian				printf(" greaser");
take				printf(" snatch");
"don't"				printf(" duzn't");
jive				printf(" JIBE");
fool				printf(" honkyfool");
modem				printf(" doodad");
"e the "			printf("e da damn ");
"a the "			printf("a da damn ");
"t the "			printf("t da damn ");
"d the "			printf("d da damn ");
" man "                     printf(" dude ");
woman				printf("mama");
girl				printf("goat");
something			printf("sump'n");
" lie "			printf(" honky jibe ");
-o-                     printf(" -on rebound- ");
-oo-			printf(" -check y'out latah-");
[a-b]"."		{ sprintf(buf, "%s  Sheeeiit.",yytext); printf(buf); }
[e-f]"."		{ sprintf(buf, "%s  What it is, Mama!",yytext); printf(buf); }
[i-j]"."		{ sprintf(buf, "%s  Ya' know?",yytext); printf(buf); }
[m-n]"."		{ sprintf(buf, "%s  'S coo', bro.",yytext); printf(buf); }
[q-r]"."		{ sprintf(buf, "%s  Ah be baaad...",yytext); printf(buf); }
[u-v]"."		{ sprintf(buf, "%s  Man!",yytext); printf(buf); }
[y-z]"."		{ sprintf(buf, "%s  Slap mah fro!",yytext); printf(buf); }
Sure			printf("Sho' nuff");
sure			printf("sho' nuff");
\40+get				printf(" git");
"will have"			printf("gots'ta");
"will "				printf("gots'ta ");
"got to"			printf("gots'ta");
"I am"				printf("I's gots'ta be");
"am not"				printf("aint");
"is not"				printf("aint");
"are not"				printf("aint");
" are your"				printf(" is yo'");
" are you"				printf(" you is");
\40+hat\40+                     printf(" fedora ");
\40+shoe                    printf(" kicker");
haven't				printf("aint");
"have to"			printf("gots'ta");
have				printf("gots'");
" has"				printf(" gots'ta");
"come over"			printf("mosey on down");
\40+come\40+                    printf(" mosey on down ");
!                       printf(".  Right On!  ");
buy				printf("steal");
\40+car\40+                     printf(" wheels ");
drive				printf("roll");
\40+eat\40+                     printf(" feed da bud ");
\40+black                   printf(" brother ");
\40+negro                   printf(" brother");
white 				printf("honky");
\40+nigger                  printf(" gentleman ");
nice				printf("supa' fine");
"person"			printf("sucka'");
\40+thing                   printf(" wahtahmellun");
home					printf("plantation");
name				printf("dojigger");
\40+path                    printf(" alley");
computer			printf("clunker");
or				printf("o'");
killed				printf("wasted");
president			printf("super-dude");
"prime minister"		printf("super honcho");
injured				printf("hosed");
government			printf("guv'ment");
knew				printf("knowed");
because				printf("a'cuz");
Because				printf("A'cuz");
your				printf("yo'");
Your				printf("Yo'");
four				printf("foe");
got				printf("gots");
aren't				printf("ain't");
young				printf("yung");
you				printf("ya'");
You				printf("You's");
first				printf("fust");
police				printf("honky pigs");
\40+string                  printf(" chittlin'");
\40+read		printf(" eyeball");
write				printf("scribble");
th				printf("d");
Th				printf("D");
ing				printf("in'");
\40+a\40+			printf(" some ");
\40+to\40+			printf(" t'");
tion				printf("shun");
\40+almost\40+			printf(" mos' ");
" from"			printf(" fum");
\40+because\40+		printf(" cuz' ");
you're		printf("youse");
You're		printf("Youse");
alright			printf("coo'");
okay			printf("coo'");
"er "			printf("a' ");
known			printf("knode");
want			printf("wants'");
beat			printf("whup'");
exp			printf("'sp");
exs			printf("'s");
" exc"			printf(" 's");
" ex"			printf(" 'es");
like			printf("likes");
did				printf("dun did");
"kind of"			printf("kind'a");
women				printf("honky chicks");
" men "				printf(" dudes ");
" mens "			printf(" dudes ");
" man "				printf(" dude ");
woman				printf("honky chick");
dead			printf("wasted");
good			printf("baaaad");
"open "				printf("jimmey ");
"opened "			printf("jimmey'd ");
" very"				printf(" real");
"per"				printf("puh'");
"pera"				printf("puh'");
"oar"				printf("o'");
" can"				printf(" kin");
"just "				printf("plum ");
detroit			printf("Mo-town");
"western electric"		printf("da' cave");
" believe"			printf(" recon'");
[Ii]"ndianapolis"		printf("Nap-town");
" "[Jj]"ack"			printf(" Buckwheat");
" "[Bb]"ob "		printf(" Liva' Lips ");
" "[Pp]"hil "		printf(" dat fine soul ");
" "[Mm]"ark "		printf(" Amos ");
[Rr]obert		printf("Leroy");
[Ss]"andy"		printf("dat fine femahnaine ladee");
[Jj]"ohn "		printf("Raz'tus ");
" "[Pp]"aul"		printf(" Fuh'rina");
[Rr]"eagan"		printf("Kingfish");
[Dd]"avid"		printf("Issac");
[Rr]"onald"		printf("Rolo");
" "[Jj]"im "		printf(" Bo-Jangles ");
" "[Mm]"ary"		printf(" Snow Flake");
[Ll]"arry"		printf("Remus");
[Jj]"oe"		printf("Massa' ");
[Jj]"oseph"		printf("Massa' ");
mohammed			printf("liva' lips");
pontiff				printf("wiz'");
pope				printf("wiz'");
pravda				printf("dat commie rag");
broken				printf("bugger'd");
"strange "			printf("funky ");
"dance "			printf("boogy ");
" house"			printf(" crib");
ask				printf("ax'");
" so "				printf(" so's ");
head				printf("'haid");
boss				printf("main man");
wife				printf("mama");
people				printf("sucka's");
money				printf("bre'd");
[a-z]":"		{	*(yytext+1) = ',';
				sprintf(buf, "%s dig dis:",yytext);
				printf(buf);
			}
amateur				printf("begina'");
radio					printf("transista'");
" of "				printf(" uh ");
what				printf("whut");
does				printf("duz");
was				printf("wuz");
" were"				printf(" wuz");
"understand it"			printf("dig it");
understand			printf("dig it");
" my"				printf(" mah'");
" "[Ii]" "			printf(" ah' ");
"meta"				printf("meta-fuckin'");
"hair"			printf("fro");
"talk"			printf("rap");
"music"			printf("beat");
"basket"		printf("hoop");
"football"		printf("ball");
"friend"		printf("homey");
"school"		printf("farm");
"boss"			printf("Man");
"want to"		printf("wanna");
"wants to"		printf("be hankerin' aftah");
"well"			printf("sheeit");
"Well"			printf("Sheeit");
"big"			printf("big-ass");
"bad"			printf("bad-ass");
"small"			printf("little-ass");
"sort of"		printf("radical");
" is "			printf(" be ");
water			printf("booze");
book			printf("scribblin'");
magazine		printf("issue of GQ");
paper			printf("sheet");
up			printf("down");
down			printf("waaay down");
break			printf("boogie");
Hi			printf("'Sup, dude");
VAX			printf("pink Cadillac");
.			printf(yytext);
\n			printf("\n");

%%

main()
{
	yylex();
}
