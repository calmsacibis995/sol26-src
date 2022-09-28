/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#ident	"@(#)__tbl_2_b.c	1.4	92/07/14 SMI"

#include "synonyms.h"

 /* table of 16 multiples of 2**176 */
const unsigned short __tbl_2_big_digits [] = { 1,
 /* 2**176 = */
5136/*e   0*/,3647/*e   4*/,1951/*e   8*/,6171/*e  12*/,
2397/*e  16*/,8943/*e  20*/,9196/*e  24*/,9668/*e  28*/,
6473/*e  32*/,8053/*e  36*/, 411/*e  40*/,9713/*e  44*/,
5780/*e  48*/,   9/*e  52*/,
 /* 2**352 = */
8496/*e   0*/,4621/*e   4*/,5027/*e   8*/,2440/*e  12*/,
7421/*e  16*/,2800/*e  20*/,9232/*e  24*/,5393/*e  28*/,
5043/*e  32*/,7691/*e  36*/,5754/*e  40*/,3553/*e  44*/,
4950/*e  48*/,2449/*e  52*/,6731/*e  56*/,9956/*e  60*/,
8625/*e  64*/,7631/*e  68*/,8347/*e  72*/,8120/*e  76*/,
2835/*e  80*/,6443/*e  84*/,8604/*e  88*/,9602/*e  92*/,
4463/*e  96*/,7399/*e 100*/,  91/*e 104*/,
 /* 2**528 = */
5456/*e   0*/,2731/*e   4*/,2627/*e   8*/,5621/*e  12*/,
 947/*e  16*/,4080/*e  20*/,3730/*e  24*/,6396/*e  28*/,
6144/*e  32*/,8751/*e  36*/,4982/*e  40*/,1095/*e  44*/,
7858/*e  48*/,9392/*e  52*/, 370/*e  56*/,6183/*e  60*/,
 466/*e  64*/,6340/*e  68*/,1687/*e  72*/,7467/*e  76*/,
8999/*e  80*/,7474/*e  84*/,4952/*e  88*/,7757/*e  92*/,
1322/*e  96*/, 249/*e 100*/, 924/*e 104*/,8343/*e 108*/,
1841/*e 112*/,4877/*e 116*/,1810/*e 120*/,1833/*e 124*/,
2824/*e 128*/,3302/*e 132*/,1768/*e 136*/, 435/*e 140*/,
6718/*e 144*/,  49/*e 148*/,6941/*e 152*/, 878/*e 156*/,

 /* 2**704 = */
2016/*e   0*/,7250/*e   4*/, 277/*e   8*/, 692/*e  12*/,
7634/*e  16*/,1634/*e  20*/,6149/*e  24*/,2012/*e  28*/,
 666/*e  32*/,3571/*e  36*/,5511/*e  40*/,7380/*e  44*/,
3578/*e  48*/,9928/*e  52*/, 935/*e  56*/, 430/*e  60*/,
3633/*e  64*/, 450/*e  68*/,1810/*e  72*/,4086/*e  76*/,
  29/*e  80*/, 916/*e  84*/, 831/*e  88*/,4834/*e  92*/,
8851/*e  96*/, 169/*e 100*/,7275/*e 104*/,9631/*e 108*/,
1521/*e 112*/,4414/*e 116*/,4580/*e 120*/,1866/*e 124*/,
1538/*e 128*/,3293/*e 132*/,6875/*e 136*/,7144/*e 140*/,
9099/*e 144*/,9444/*e 148*/,9724/*e 152*/,1751/*e 156*/,
7078/*e 160*/,8451/*e 164*/,5436/*e 168*/,4880/*e 172*/,
 586/*e 176*/,6082/*e 180*/,3812/*e 184*/,8558/*e 188*/,
6115/*e 192*/,7397/*e 196*/,4247/*e 200*/,2174/*e 204*/,
8416/*e 208*/,
 /* 2**880 = */
4176/*e   0*/,9387/*e   4*/,1096/*e   8*/,2997/*e  12*/,
7592/*e  16*/,9403/*e  20*/,2053/*e  24*/,8081/*e  28*/,
2903/*e  32*/,8078/*e  36*/,4437/*e  40*/,7637/*e  44*/,
1340/*e  48*/,4806/*e  52*/,2654/*e  56*/,7647/*e  60*/,
8714/*e  64*/,8256/*e  68*/,1192/*e  72*/,2008/*e  76*/,
3864/*e  80*/,5732/*e  84*/,3919/*e  88*/,6625/*e  92*/,
8324/*e  96*/,6829/*e 100*/,7280/*e 104*/,5477/*e 108*/,
  22/*e 112*/,4868/*e 116*/,5460/*e 120*/,1517/*e 124*/,
1460/*e 128*/,5263/*e 132*/,3966/*e 136*/,4850/*e 140*/,
6335/*e 144*/,1663/*e 148*/,3219/*e 152*/,5060/*e 156*/,
9676/*e 160*/,2273/*e 164*/,9847/*e 168*/,2269/*e 172*/,
 875/*e 176*/,1570/*e 180*/,4407/*e 184*/, 867/*e 188*/,
6765/*e 192*/,2595/*e 196*/,5923/*e 200*/,6978/*e 204*/,
6176/*e 208*/,3425/*e 212*/,8627/*e 216*/, 377/*e 220*/,
1494/*e 224*/,6071/*e 228*/,6774/*e 232*/,3313/*e 236*/,
2450/*e 240*/,6470/*e 244*/,4545/*e 248*/,3471/*e 252*/,
3481/*e 256*/, 611/*e 260*/,   8/*e 264*/,
 /* 2**1056 = */
7936/*e   0*/,3648/*e   4*/,7165/*e   8*/,8638/*e  12*/,
4263/*e  16*/,1002/*e  20*/,5822/*e  24*/,8862/*e  28*/,
2094/*e  32*/,1039/*e  36*/,8207/*e  40*/,2757/*e  44*/,
2230/*e  48*/,9672/*e  52*/,1751/*e  56*/,6253/*e  60*/,
 262/*e  64*/,8564/*e  68*/,8485/*e  72*/,8691/*e  76*/,
3810/*e  80*/,8024/*e  84*/,6274/*e  88*/,7809/*e  92*/,
3097/*e  96*/,3119/*e 100*/,7891/*e 104*/, 234/*e 108*/,
8558/*e 112*/,9086/*e 116*/, 788/*e 120*/,3509/*e 124*/,
5621/*e 128*/,6999/*e 132*/,8755/*e 136*/,4608/*e 140*/,
 692/*e 144*/,3861/*e 148*/,4517/*e 152*/, 749/*e 156*/,
3084/*e 160*/,1697/*e 164*/,8514/*e 168*/,5770/*e 172*/,
9255/*e 176*/,  86/*e 180*/,9732/*e 184*/,1839/*e 188*/,
6133/*e 192*/,5709/*e 196*/,3071/*e 200*/,1843/*e 204*/,
5440/*e 208*/,6528/*e 212*/,5194/*e 216*/,9801/*e 220*/,
  16/*e 224*/,4618/*e 228*/,5148/*e 232*/, 935/*e 236*/,
5197/*e 240*/,6009/*e 244*/, 876/*e 248*/,8315/*e 252*/,
6672/*e 256*/, 699/*e 260*/,8740/*e 264*/,2380/*e 268*/,
2882/*e 272*/,2432/*e 276*/,6666/*e 280*/,1901/*e 284*/,
1524/*e 288*/,9194/*e 292*/,6517/*e 296*/,6428/*e 300*/,
4773/*e 304*/,3222/*e 308*/,2103/*e 312*/,  77/*e 316*/,

 /* 2**1232 = */
9296/*e   0*/,2795/*e   4*/,1600/*e   8*/,2386/*e  12*/,
3528/*e  16*/,8182/*e  20*/,2470/*e  24*/,5875/*e  28*/,
1547/*e  32*/,4693/*e  36*/,6515/*e  40*/,5634/*e  44*/,
1849/*e  48*/,5578/*e  52*/,4522/*e  56*/,1873/*e  60*/,
1222/*e  64*/,2745/*e  68*/,4829/*e  72*/,9152/*e  76*/,
 581/*e  80*/,7585/*e  84*/,4856/*e  88*/,1529/*e  92*/,
 275/*e  96*/,5613/*e 100*/,6671/*e 104*/,7350/*e 108*/,
2713/*e 112*/,7784/*e 116*/,1975/*e 120*/,  62/*e 124*/,
9984/*e 128*/,3163/*e 132*/,1691/*e 136*/,2322/*e 140*/,
7802/*e 144*/,4829/*e 148*/,9958/*e 152*/,9338/*e 156*/,
7301/*e 160*/,5809/*e 164*/,3804/*e 168*/,9550/*e 172*/,
9652/*e 176*/,2300/*e 180*/, 298/*e 184*/,9317/*e 188*/,
5873/*e 192*/,4379/*e 196*/,3609/*e 200*/,7666/*e 204*/,
4052/*e 208*/,2353/*e 212*/, 236/*e 216*/,2071/*e 220*/,
8031/*e 224*/,4256/*e 228*/,4023/*e 232*/,5954/*e 236*/,
4832/*e 240*/, 522/*e 244*/,2802/*e 248*/,8998/*e 252*/,
6773/*e 256*/,9397/*e 260*/, 911/*e 264*/,4661/*e 268*/,
5125/*e 272*/,2094/*e 276*/,5931/*e 280*/,2634/*e 284*/,
1065/*e 288*/,5973/*e 292*/,6990/*e 296*/,4337/*e 300*/,
6266/*e 304*/, 517/*e 308*/,9889/*e 312*/,4805/*e 316*/,
8432/*e 320*/,1783/*e 324*/,8492/*e 328*/,6965/*e 332*/,
5897/*e 336*/,2902/*e 340*/,1739/*e 344*/,5011/*e 348*/,
5726/*e 352*/, 246/*e 356*/,6152/*e 360*/,5280/*e 364*/,
 739/*e 368*/,
 /* 2**1408 = */
4256/*e   0*/,2406/*e   4*/,2287/*e   8*/,2012/*e  12*/,
1698/*e  16*/,8745/*e  20*/,5634/*e  24*/,8896/*e  28*/,
6144/*e  32*/,7629/*e  36*/,2175/*e  40*/,7257/*e  44*/,
2098/*e  48*/,6932/*e  52*/,1148/*e  56*/,4053/*e  60*/,
6185/*e  64*/, 493/*e  68*/,6764/*e  72*/,5779/*e  76*/,
1562/*e  80*/,2570/*e  84*/,6310/*e  88*/,5822/*e  92*/,
8505/*e  96*/,5785/*e 100*/,9564/*e 104*/,4293/*e 108*/,
3651/*e 112*/,4242/*e 116*/,4129/*e 120*/,5838/*e 124*/,
  93/*e 128*/,9185/*e 132*/,6796/*e 136*/,8783/*e 140*/,
7924/*e 144*/,2095/*e 148*/,7676/*e 152*/,5622/*e 156*/,
7182/*e 160*/,7341/*e 164*/,3876/*e 168*/,5916/*e 172*/,
5737/*e 176*/,4561/*e 180*/,7348/*e 184*/,7948/*e 188*/,
6153/*e 192*/,3750/*e 196*/,6974/*e 200*/,3504/*e 204*/,
8556/*e 208*/,4032/*e 212*/,7520/*e 216*/,9842/*e 220*/,
7934/*e 224*/,6339/*e 228*/,6110/*e 232*/,1588/*e 236*/,
9814/*e 240*/,5994/*e 244*/,2368/*e 248*/,4995/*e 252*/,
 335/*e 256*/,7949/*e 260*/,5083/*e 264*/,8529/*e 268*/,
4518/*e 272*/,7326/*e 276*/,2521/*e 280*/,4445/*e 284*/,
2523/*e 288*/,1548/*e 292*/,6996/*e 296*/, 178/*e 300*/,
1923/*e 304*/,4958/*e 308*/,8234/*e 312*/,2576/*e 316*/,
1387/*e 320*/, 494/*e 324*/,7654/*e 328*/,2799/*e 332*/,
3077/*e 336*/,3775/*e 340*/,6720/*e 344*/,9487/*e 348*/,
2144/*e 352*/,1330/*e 356*/,5252/*e 360*/,7293/*e 364*/,
 442/*e 368*/,2024/*e 372*/,7275/*e 376*/, 420/*e 380*/,
7237/*e 384*/,7033/*e 388*/,8399/*e 392*/,7199/*e 396*/,
 193/*e 400*/,7575/*e 404*/,6078/*e 408*/, 390/*e 412*/,
2716/*e 416*/,7083/*e 420*/,
 /* 2**1584 = */
8816/*e   0*/,1033/*e   4*/,6958/*e   8*/,5085/*e  12*/,
4650/*e  16*/,8562/*e  20*/,6359/*e  24*/,9166/*e  28*/,
9828/*e  32*/,3162/*e  36*/,8499/*e  40*/,1979/*e  44*/,
1550/*e  48*/,9583/*e  52*/,1730/*e  56*/,1447/*e  60*/,
4468/*e  64*/,9351/*e  68*/,1864/*e  72*/,4578/*e  76*/,
9711/*e  80*/,7045/*e  84*/,9255/*e  88*/,6775/*e  92*/,
7678/*e  96*/,2479/*e 100*/,9236/*e 104*/,5352/*e 108*/,
6804/*e 112*/,6163/*e 116*/,6886/*e 120*/, 665/*e 124*/,
3720/*e 128*/,1639/*e 132*/, 746/*e 136*/, 362/*e 140*/,
2966/*e 144*/,3596/*e 148*/, 821/*e 152*/,7819/*e 156*/,
8508/*e 160*/,5521/*e 164*/,4329/*e 168*/,8602/*e 172*/,
1845/*e 176*/,5625/*e 180*/, 459/*e 184*/,6744/*e 188*/,
3755/*e 192*/,3198/*e 196*/,7799/*e 200*/,4293/*e 204*/,
6343/*e 208*/,7363/*e 212*/,  72/*e 216*/,1693/*e 220*/,
7023/*e 224*/,9600/*e 228*/,5305/*e 232*/, 978/*e 236*/,
 316/*e 240*/,6262/*e 244*/,2958/*e 248*/,3869/*e 252*/,
1019/*e 256*/,2480/*e 260*/,7657/*e 264*/,5154/*e 268*/,
8446/*e 272*/,1970/*e 276*/,6185/*e 280*/,4384/*e 284*/,
7814/*e 288*/,4661/*e 292*/,9387/*e 296*/,5894/*e 300*/,
7890/*e 304*/,4818/*e 308*/, 890/*e 312*/,6227/*e 316*/,
3731/*e 320*/,5449/*e 324*/,1988/*e 328*/,3319/*e 332*/,
3924/*e 336*/,5720/*e 340*/,9314/*e 344*/,4840/*e 348*/,
6220/*e 352*/,8564/*e 356*/, 406/*e 360*/,3577/*e 364*/,
1638/*e 368*/,4744/*e 372*/,3764/*e 376*/,2639/*e 380*/,
6477/*e 384*/,1838/*e 388*/,5841/*e 392*/,4964/*e 396*/,
9408/*e 400*/,6112/*e 404*/,6153/*e 408*/,2640/*e 412*/,
5212/*e 416*/,6099/*e 420*/,3138/*e 424*/,3009/*e 428*/,
 331/*e 432*/,2605/*e 436*/,5170/*e 440*/,9012/*e 444*/,
3102/*e 448*/,7995/*e 452*/,9831/*e 456*/, 238/*e 460*/,
2330/*e 464*/,2634/*e 468*/,7844/*e 472*/,   6/*e 476*/,

 /* 2**1760 = */
8976/*e   0*/,1967/*e   4*/,7401/*e   8*/,6975/*e  12*/,
9839/*e  16*/,7575/*e  20*/,5015/*e  24*/,5290/*e  28*/,
5989/*e  32*/,2211/*e  36*/,3726/*e  40*/,8955/*e  44*/,
3754/*e  48*/,4611/*e  52*/,7975/*e  56*/,5400/*e  60*/,
 366/*e  64*/,1794/*e  68*/,8788/*e  72*/,4630/*e  76*/,
8080/*e  80*/,7081/*e  84*/,3134/*e  88*/,1495/*e  92*/,
5831/*e  96*/,3023/*e 100*/,5850/*e 104*/,6840/*e 108*/,
7968/*e 112*/,5490/*e 116*/,1966/*e 120*/,1460/*e 124*/,
4139/*e 128*/,8406/*e 132*/, 179/*e 136*/,5250/*e 140*/,
5092/*e 144*/,6629/*e 148*/, 993/*e 152*/,1867/*e 156*/,
8709/*e 160*/,4496/*e 164*/, 324/*e 168*/,6753/*e 172*/,
2338/*e 176*/,8582/*e 180*/,4832/*e 184*/,4066/*e 188*/,
9970/*e 192*/,3533/*e 196*/,3396/*e 200*/,9989/*e 204*/,
5653/*e 208*/,3629/*e 212*/,1303/*e 216*/,2824/*e 220*/,
 280/*e 224*/,7208/*e 228*/,5609/*e 232*/,  73/*e 236*/,
4308/*e 240*/,8383/*e 244*/, 536/*e 248*/,8482/*e 252*/,
5357/*e 256*/,9868/*e 260*/,6577/*e 264*/,6642/*e 268*/,
7900/*e 272*/,7650/*e 276*/,9230/*e 280*/,1954/*e 284*/,
6999/*e 288*/,7837/*e 292*/,6889/*e 296*/,1878/*e 300*/,
2622/*e 304*/, 299/*e 308*/,2946/*e 312*/,1724/*e 316*/,
3683/*e 320*/,7264/*e 324*/,1942/*e 328*/, 513/*e 332*/,
1476/*e 336*/, 225/*e 340*/,2727/*e 344*/,8296/*e 348*/,
1062/*e 352*/,1322/*e 356*/, 434/*e 360*/,5231/*e 364*/,
4226/*e 368*/, 944/*e 372*/,5665/*e 376*/, 214/*e 380*/,
6781/*e 384*/,1024/*e 388*/,1051/*e 392*/,3188/*e 396*/,
9305/*e 400*/,4881/*e 404*/,6090/*e 408*/, 980/*e 412*/,
2572/*e 416*/,9937/*e 420*/,1979/*e 424*/,8274/*e 428*/,
 289/*e 432*/,4511/*e 436*/, 900/*e 440*/,9132/*e 444*/,
1018/*e 448*/,3470/*e 452*/,4731/*e 456*/,6667/*e 460*/,
1030/*e 464*/,9134/*e 468*/,9123/*e 472*/,7994/*e 476*/,
1484/*e 480*/,7641/*e 484*/,8795/*e 488*/,4444/*e 492*/,
6262/*e 496*/,1117/*e 500*/,3636/*e 504*/,7776/*e 508*/,
4625/*e 512*/, 961/*e 516*/,9448/*e 520*/,9818/*e 524*/,
  64/*e 528*/,
 /* 2**1936 = */
 736/*e   0*/,2594/*e   4*/,1645/*e   8*/,9830/*e  12*/,
6313/*e  16*/,7503/*e  20*/,7959/*e  24*/,2508/*e  28*/,
2307/*e  32*/,8248/*e  36*/,3501/*e  40*/,5230/*e  44*/,
7757/*e  48*/,2443/*e  52*/,9926/*e  56*/,7924/*e  60*/,
 504/*e  64*/,6233/*e  68*/,7092/*e  72*/, 457/*e  76*/,
2744/*e  80*/,7080/*e  84*/,9126/*e  88*/,5847/*e  92*/,
 844/*e  96*/,2794/*e 100*/,5617/*e 104*/,9346/*e 108*/,
2729/*e 112*/,1702/*e 116*/,1691/*e 120*/, 132/*e 124*/,
9617/*e 128*/,4819/*e 132*/,1863/*e 136*/,8464/*e 140*/,
6916/*e 144*/,9300/*e 148*/,1606/*e 152*/,4962/*e 156*/,
6326/*e 160*/,1203/*e 164*/,8610/*e 168*/,3712/*e 172*/,
3275/*e 176*/, 474/*e 180*/,1713/*e 184*/,1858/*e 188*/,
9086/*e 192*/, 560/*e 196*/,9858/*e 200*/, 597/*e 204*/,
4042/*e 208*/,8796/*e 212*/,9489/*e 216*/, 423/*e 220*/,
6587/*e 224*/,3729/*e 228*/,8841/*e 232*/,9337/*e 236*/,
3557/*e 240*/,1998/*e 244*/,8042/*e 248*/,5889/*e 252*/,
8496/*e 256*/,3764/*e 260*/,8502/*e 264*/,1003/*e 268*/,
2253/*e 272*/,2705/*e 276*/,5565/*e 280*/,9322/*e 284*/,
 250/*e 288*/,5262/*e 292*/,9432/*e 296*/,4647/*e 300*/,
7117/*e 304*/,1678/*e 308*/,3101/*e 312*/,4753/*e 316*/,
9941/*e 320*/,3305/*e 324*/,4310/*e 328*/,6531/*e 332*/,
9913/*e 336*/, 568/*e 340*/,5219/*e 344*/,7930/*e 348*/,
7448/*e 352*/,2233/*e 356*/,4632/*e 360*/,2848/*e 364*/,
5102/*e 368*/,6675/*e 372*/,4088/*e 376*/,7492/*e 380*/,
 522/*e 384*/,3428/*e 388*/,5159/*e 392*/,6760/*e 396*/,
4734/*e 400*/,6017/*e 404*/,2513/*e 408*/,9549/*e 412*/,
3481/*e 416*/,5450/*e 420*/,1592/*e 424*/, 652/*e 428*/,
9951/*e 432*/,9352/*e 436*/,3508/*e 440*/,6226/*e 444*/,
5357/*e 448*/,6998/*e 452*/,8378/*e 456*/,8583/*e 460*/,
6506/*e 464*/,7954/*e 468*/,7572/*e 472*/,9103/*e 476*/,
8339/*e 480*/,3193/*e 484*/,5917/*e 488*/,2883/*e 492*/,
3893/*e 496*/,2760/*e 500*/,4911/*e 504*/,5776/*e 508*/,
2294/*e 512*/,4081/*e 516*/, 347/*e 520*/,8562/*e 524*/,
3810/*e 528*/,1472/*e 532*/,7555/*e 536*/,  21/*e 540*/,
9331/*e 544*/,5173/*e 548*/,7173/*e 552*/,7298/*e 556*/,
1488/*e 560*/,7171/*e 564*/,6819/*e 568*/,9705/*e 572*/,
4028/*e 576*/, 622/*e 580*/,
 /* 2**2112 = */
  96/*e   0*/,7354/*e   4*/,6574/*e   8*/, 879/*e  12*/,
1348/*e  16*/,7336/*e  20*/,8290/*e  24*/,4767/*e  28*/,
3217/*e  32*/,6899/*e  36*/,8146/*e  40*/,4051/*e  44*/,
8478/*e  48*/,6083/*e  52*/,3968/*e  56*/,8630/*e  60*/,
3876/*e  64*/, 716/*e  68*/,4149/*e  72*/,1734/*e  76*/,
3172/*e  80*/,1868/*e  84*/,6600/*e  88*/,3707/*e  92*/,
8497/*e  96*/,  61/*e 100*/,4534/*e 104*/, 859/*e 108*/,
6969/*e 112*/,2039/*e 116*/,3936/*e 120*/,6185/*e 124*/,
5589/*e 128*/,3076/*e 132*/,7259/*e 136*/,7710/*e 140*/,
7697/*e 144*/,3782/*e 148*/,1153/*e 152*/,4762/*e 156*/,
8117/*e 160*/,5831/*e 164*/,6790/*e 168*/,1275/*e 172*/,
1248/*e 176*/,8517/*e 180*/,5472/*e 184*/,8342/*e 188*/,
8506/*e 192*/,5573/*e 196*/, 455/*e 200*/,1147/*e 204*/,
4103/*e 208*/,9109/*e 212*/,8908/*e 216*/,9227/*e 220*/,
3711/*e 224*/,2973/*e 228*/,1176/*e 232*/,1621/*e 236*/,
1616/*e 240*/,3098/*e 244*/,8212/*e 248*/,4664/*e 252*/,
1742/*e 256*/,7560/*e 260*/,8222/*e 264*/,8218/*e 268*/,
4391/*e 272*/,9973/*e 276*/,4051/*e 280*/,9146/*e 284*/,
7760/*e 288*/,8804/*e 292*/,8527/*e 296*/,1312/*e 300*/,
 696/*e 304*/,6547/*e 308*/,7813/*e 312*/,8818/*e 316*/,
 303/*e 320*/,4040/*e 324*/,4619/*e 328*/,1287/*e 332*/,
6436/*e 336*/,2357/*e 340*/,1999/*e 344*/,5321/*e 348*/,
7267/*e 352*/,5164/*e 356*/,8458/*e 360*/, 495/*e 364*/,
4323/*e 368*/,8528/*e 372*/,1421/*e 376*/,1658/*e 380*/,
 848/*e 384*/,5833/*e 388*/,9820/*e 392*/,8269/*e 396*/,
 518/*e 400*/,3462/*e 404*/,2254/*e 408*/,5442/*e 412*/,
9984/*e 416*/,2485/*e 420*/,2299/*e 424*/,6826/*e 428*/,
1542/*e 432*/,8068/*e 436*/,5236/*e 440*/,9285/*e 444*/,
8388/*e 448*/,3718/*e 452*/,7955/*e 456*/,5563/*e 460*/,
7186/*e 464*/,6884/*e 468*/,8221/*e 472*/,3647/*e 476*/,
3960/*e 480*/, 358/*e 484*/,4647/*e 488*/,3566/*e 492*/,
4944/*e 496*/,9433/*e 500*/,5631/*e 504*/,4602/*e 508*/,
1539/*e 512*/,5057/*e 516*/,9315/*e 520*/, 317/*e 524*/,
5323/*e 528*/, 821/*e 532*/,2955/*e 536*/,1977/*e 540*/,
6755/*e 544*/,7225/*e 548*/,8197/*e 552*/,1858/*e 556*/,
7650/*e 560*/,3365/*e 564*/,3385/*e 568*/,1270/*e 572*/,
2993/*e 576*/,6279/*e 580*/,5530/*e 584*/,4674/*e 588*/,
 347/*e 592*/,2892/*e 596*/,8341/*e 600*/,8845/*e 604*/,
4166/*e 608*/,6302/*e 612*/,2314/*e 616*/,9919/*e 620*/,
 225/*e 624*/,4354/*e 628*/,5961/*e 632*/,
 /* 2**2288 = */
3056/*e   0*/, 305/*e   4*/,5210/*e   8*/,6069/*e  12*/,
 904/*e  16*/,9858/*e  20*/,3941/*e  24*/,7723/*e  28*/,
9125/*e  32*/,3335/*e  36*/,3841/*e  40*/, 964/*e  44*/,
6935/*e  48*/,8974/*e  52*/,7366/*e  56*/,9992/*e  60*/,
5813/*e  64*/,6635/*e  68*/,7734/*e  72*/,8357/*e  76*/,
3112/*e  80*/,2437/*e  84*/,8477/*e  88*/,3727/*e  92*/,
6834/*e  96*/,3664/*e 100*/,8958/*e 104*/,9125/*e 108*/,
9388/*e 112*/,2293/*e 116*/, 890/*e 120*/, 720/*e 124*/,
8271/*e 128*/,5123/*e 132*/,9938/*e 136*/, 574/*e 140*/,
1003/*e 144*/,2473/*e 148*/,1503/*e 152*/,6588/*e 156*/,
2833/*e 160*/,8773/*e 164*/,9926/*e 168*/,9973/*e 172*/,
4799/*e 176*/,3729/*e 180*/, 210/*e 184*/, 834/*e 188*/,
3733/*e 192*/,4090/*e 196*/,4693/*e 200*/,3441/*e 204*/,
1448/*e 208*/, 543/*e 212*/,5177/*e 216*/,5073/*e 220*/,
3603/*e 224*/,2974/*e 228*/,6869/*e 232*/,4376/*e 236*/,
2335/*e 240*/,9243/*e 244*/,1383/*e 248*/,9244/*e 252*/,
5333/*e 256*/,5912/*e 260*/,7477/*e 264*/,2503/*e 268*/,
2545/*e 272*/,7532/*e 276*/,8479/*e 280*/, 728/*e 284*/,
3392/*e 288*/,9606/*e 292*/,4674/*e 296*/,8217/*e 300*/,
9467/*e 304*/,1894/*e 308*/,1834/*e 312*/,2868/*e 316*/,
4160/*e 320*/,4918/*e 324*/,1788/*e 328*/,6242/*e 332*/,
5634/*e 336*/,8561/*e 340*/,4974/*e 344*/, 176/*e 348*/,
5442/*e 352*/,4292/*e 356*/,4509/*e 360*/,7399/*e 364*/,
6251/*e 368*/,1546/*e 372*/,4804/*e 376*/,8796/*e 380*/,
8457/*e 384*/,9101/*e 388*/,7589/*e 392*/, 392/*e 396*/,
3120/*e 400*/,9567/*e 404*/,9333/*e 408*/,9170/*e 412*/,
3322/*e 416*/,8672/*e 420*/, 977/*e 424*/,7605/*e 428*/,
7135/*e 432*/, 410/*e 436*/,9454/*e 440*/,9342/*e 444*/,
1631/*e 448*/,1444/*e 452*/,5836/*e 456*/,2004/*e 460*/,
9046/*e 464*/,7719/*e 468*/,3888/*e 472*/,7540/*e 476*/,
8995/*e 480*/,3712/*e 484*/,9832/*e 488*/,2255/*e 492*/,
8858/*e 496*/,2559/*e 500*/,7417/*e 504*/,1296/*e 508*/,
9972/*e 512*/,4079/*e 516*/,4930/*e 520*/,5367/*e 524*/,
6352/*e 528*/,2509/*e 532*/, 268/*e 536*/,7675/*e 540*/,
3564/*e 544*/,6932/*e 548*/, 958/*e 552*/,2879/*e 556*/,
1143/*e 560*/,6657/*e 564*/,3790/*e 568*/,9125/*e 572*/,
9183/*e 576*/,1547/*e 580*/,3476/*e 584*/, 595/*e 588*/,
3251/*e 592*/,7944/*e 596*/,6405/*e 600*/,9124/*e 604*/,
 205/*e 608*/,1481/*e 612*/,9978/*e 616*/,3157/*e 620*/,
6155/*e 624*/,3487/*e 628*/,3423/*e 632*/,6067/*e 636*/,
4884/*e 640*/,7049/*e 644*/,5434/*e 648*/,3166/*e 652*/,
4690/*e 656*/,8419/*e 660*/, 741/*e 664*/,9851/*e 668*/,
1078/*e 672*/,1952/*e 676*/,2073/*e 680*/,7099/*e 684*/,
   5/*e 688*/,
 /* 2**2464 = */
5616/*e   0*/,3281/*e   4*/,4422/*e   8*/,8268/*e  12*/,
5646/*e  16*/,4613/*e  20*/, 164/*e  24*/,4182/*e  28*/,
4968/*e  32*/,1119/*e  36*/,8517/*e  40*/,5615/*e  44*/,
3242/*e  48*/,4111/*e  52*/,5252/*e  56*/, 758/*e  60*/,
6942/*e  64*/,4286/*e  68*/,1915/*e  72*/,5700/*e  76*/,
5398/*e  80*/,6087/*e  84*/,3722/*e  88*/,8981/*e  92*/,
 407/*e  96*/, 100/*e 100*/,2359/*e 104*/,2560/*e 108*/,
6659/*e 112*/,5259/*e 116*/,3448/*e 120*/,5940/*e 124*/,
2947/*e 128*/,9905/*e 132*/,6622/*e 136*/,6090/*e 140*/,
6201/*e 144*/,5369/*e 148*/,9480/*e 152*/,1557/*e 156*/,
7699/*e 160*/,5200/*e 164*/,1904/*e 168*/,8177/*e 172*/,
8066/*e 176*/,2469/*e 180*/,7239/*e 184*/,9213/*e 188*/,
9080/*e 192*/,9161/*e 196*/,3563/*e 200*/,7595/*e 204*/,
9957/*e 208*/,1623/*e 212*/,8034/*e 216*/,1555/*e 220*/,
3246/*e 224*/,7668/*e 228*/,  43/*e 232*/,3954/*e 236*/,
5553/*e 240*/, 250/*e 244*/,8912/*e 248*/,1204/*e 252*/,
1648/*e 256*/,1771/*e 260*/,5848/*e 264*/,9047/*e 268*/,
4419/*e 272*/,2989/*e 276*/,7062/*e 280*/,9011/*e 284*/,
9698/*e 288*/,8620/*e 292*/,8758/*e 296*/, 354/*e 300*/,
1031/*e 304*/,4014/*e 308*/,3735/*e 312*/,5166/*e 316*/,
4272/*e 320*/,7362/*e 324*/, 138/*e 328*/,4037/*e 332*/,
6471/*e 336*/,1385/*e 340*/,6506/*e 344*/,8351/*e 348*/,
5912/*e 352*/,6462/*e 356*/, 186/*e 360*/,1092/*e 364*/,
7932/*e 368*/,2424/*e 372*/,8710/*e 376*/,7183/*e 380*/,
 256/*e 384*/, 496/*e 388*/,6640/*e 392*/, 756/*e 396*/,
3158/*e 400*/,2760/*e 404*/,1688/*e 408*/,7506/*e 412*/,
2406/*e 416*/,9956/*e 420*/,5192/*e 424*/,6362/*e 428*/,
2663/*e 432*/,8431/*e 436*/,2566/*e 440*/,6920/*e 444*/,
1755/*e 448*/, 524/*e 452*/,8805/*e 456*/, 558/*e 460*/,
4995/*e 464*/,4690/*e 468*/, 621/*e 472*/,6107/*e 476*/,
 867/*e 480*/,3888/*e 484*/,6143/*e 488*/, 542/*e 492*/,
3829/*e 496*/,9721/*e 500*/,7923/*e 504*/,5370/*e 508*/,
6487/*e 512*/,5210/*e 516*/,2006/*e 520*/,2847/*e 524*/,
1330/*e 528*/,6076/*e 532*/,7458/*e 536*/,8879/*e 540*/,
9518/*e 544*/,5640/*e 548*/, 445/*e 552*/,3992/*e 556*/,
4856/*e 560*/,2366/*e 564*/,8935/*e 568*/,7479/*e 572*/,
5893/*e 576*/,9027/*e 580*/,6510/*e 584*/,9341/*e 588*/,
2197/*e 592*/,3372/*e 596*/,9531/*e 600*/,2218/*e 604*/,
3018/*e 608*/,4270/*e 612*/,8189/*e 616*/,1483/*e 620*/,
3535/*e 624*/,2734/*e 628*/,6923/*e 632*/,4855/*e 636*/,
5885/*e 640*/,1717/*e 644*/,9352/*e 648*/,2877/*e 652*/,
4563/*e 656*/,8281/*e 660*/,1520/*e 664*/, 345/*e 668*/,
2221/*e 672*/,3224/*e 676*/,9537/*e 680*/,2475/*e 684*/,
 858/*e 688*/,5327/*e 692*/,4356/*e 696*/,4871/*e 700*/,
6683/*e 704*/,5134/*e 708*/,8335/*e 712*/,2202/*e 716*/,
4983/*e 720*/,3599/*e 724*/,7589/*e 728*/,7537/*e 732*/,
6901/*e 736*/,  54/*e 740*/,
 /* 2**2640 = */
3776/*e   0*/,5652/*e   4*/,7748/*e   8*/,3612/*e  12*/,
7142/*e  16*/, 954/*e  20*/,3922/*e  24*/,7325/*e  28*/,
7139/*e  32*/, 708/*e  36*/,8463/*e  40*/,5667/*e  44*/,
5049/*e  48*/,8715/*e  52*/, 938/*e  56*/,1460/*e  60*/,
1490/*e  64*/,7229/*e  68*/,6945/*e  72*/,3568/*e  76*/,
1804/*e  80*/,4454/*e  84*/,4803/*e  88*/,4147/*e  92*/,
3317/*e  96*/,8229/*e 100*/,5250/*e 104*/,3079/*e 108*/,
9002/*e 112*/,9171/*e 116*/,1140/*e 120*/,6535/*e 124*/,
8448/*e 128*/,1897/*e 132*/,3783/*e 136*/,7651/*e 140*/,
6796/*e 144*/, 113/*e 148*/,4449/*e 152*/,3852/*e 156*/,
6009/*e 160*/,8462/*e 164*/,1657/*e 168*/,4141/*e 172*/,
1023/*e 176*/,6231/*e 180*/,3823/*e 184*/,4851/*e 188*/,
1326/*e 192*/,7946/*e 196*/,4300/*e 200*/,6554/*e 204*/,
 564/*e 208*/,6790/*e 212*/,3235/*e 216*/,1856/*e 220*/,
1008/*e 224*/,8655/*e 228*/, 332/*e 232*/,8941/*e 236*/,
3660/*e 240*/,4755/*e 244*/,8061/*e 248*/, 454/*e 252*/,
5885/*e 256*/,2931/*e 260*/,9805/*e 264*/,8655/*e 268*/,
5994/*e 272*/,6039/*e 276*/,7891/*e 280*/,2375/*e 284*/,
8132/*e 288*/, 110/*e 292*/,3079/*e 296*/, 400/*e 300*/,
9106/*e 304*/,9833/*e 308*/,6955/*e 312*/,2432/*e 316*/,
3078/*e 320*/,9988/*e 324*/,9644/*e 328*/,3201/*e 332*/,
5368/*e 336*/,4330/*e 340*/,9706/*e 344*/,6786/*e 348*/,
9449/*e 352*/,9967/*e 356*/,1001/*e 360*/,6177/*e 364*/,
4855/*e 368*/, 453/*e 372*/,6225/*e 376*/,  61/*e 380*/,
4570/*e 384*/,8610/*e 388*/,8633/*e 392*/,6197/*e 396*/,
5926/*e 400*/,6459/*e 404*/,7352/*e 408*/,1273/*e 412*/,
 827/*e 416*/,8965/*e 420*/,2177/*e 424*/,8861/*e 428*/,
5891/*e 432*/,8814/*e 436*/,8816/*e 440*/,3662/*e 444*/,
 525/*e 448*/,7455/*e 452*/,7387/*e 456*/,1987/*e 460*/,
7557/*e 464*/,5394/*e 468*/,7309/*e 472*/, 449/*e 476*/,
 636/*e 480*/,8219/*e 484*/,2842/*e 488*/,9327/*e 492*/,
6007/*e 496*/,6267/*e 500*/,3506/*e 504*/,2558/*e 508*/,
9977/*e 512*/,5567/*e 516*/,4846/*e 520*/,1149/*e 524*/,
7334/*e 528*/,9021/*e 532*/,1303/*e 536*/,4912/*e 540*/,
2910/*e 544*/,1552/*e 548*/,6906/*e 552*/,7022/*e 556*/,
4266/*e 560*/,9419/*e 564*/,8677/*e 568*/,1793/*e 572*/,
3458/*e 576*/,1154/*e 580*/,3911/*e 584*/,4204/*e 588*/,
3427/*e 592*/,3528/*e 596*/,3748/*e 600*/,1061/*e 604*/,
3333/*e 608*/,1475/*e 612*/,3814/*e 616*/,4003/*e 620*/,
8871/*e 624*/,1213/*e 628*/,7520/*e 632*/,9763/*e 636*/,
2422/*e 640*/,1018/*e 644*/, 617/*e 648*/,2281/*e 652*/,
3118/*e 656*/,2368/*e 660*/,4164/*e 664*/,1944/*e 668*/,
2190/*e 672*/,2152/*e 676*/,1711/*e 680*/,9584/*e 684*/,
5585/*e 688*/,9031/*e 692*/,8124/*e 696*/,4328/*e 700*/,
3044/*e 704*/,2634/*e 708*/,1124/*e 712*/,5266/*e 716*/,
8373/*e 720*/,5232/*e 724*/,5502/*e 728*/,3852/*e 732*/,
4392/*e 736*/,3903/*e 740*/,2146/*e 744*/,3855/*e 748*/,
2021/*e 752*/, 840/*e 756*/,5081/*e 760*/,6974/*e 764*/,
9412/*e 768*/,7306/*e 772*/, 224/*e 776*/,5807/*e 780*/,
1184/*e 784*/,8278/*e 788*/, 523/*e 792*/,
0};

 /* table of starting indexes into previous table */
const unsigned short __tbl_2_big_start [] = {
0,1,15,42,82,135,202,282,
375,481,601,734,880,1039,1212,1398,
1597,0};
