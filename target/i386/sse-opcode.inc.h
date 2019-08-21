#define FMTI____      (0, 0, 0, )
#define FMTI__R__     (1, 1, 0, r)
#define FMTI__RR__    (2, 2, 0, rr)
#define FMTI__RRR__   (3, 3, 0, rrr)
#define FMTI__W__     (1, 0, 1, w)
#define FMTI__WR__    (2, 1, 1, wr)
#define FMTI__WRR__   (3, 2, 1, wrr)
#define FMTI__WRRR__  (4, 3, 1, wrrr)
#define FMTI__WRRRR__ (5, 4, 1, wrrrr)
#define FMTI__WWRRR__ (5, 3, 2, wwrrr)

#define FMTI__(prop, fmti) FMTI_ ## prop ## __ fmti

#define FMTI_ARGC__(argc, argc_rd, argc_wr, lower)    argc
#define FMTI_ARGC_RD__(argc, argc_rd, argc_wr, lower) argc_rd
#define FMTI_ARGC_WR__(argc, argc_rd, argc_wr, lower) argc_wr
#define FMTI_LOWER__(argc, argc_rd, argc_wr, lower)   lower

#define FMT_ARGC(fmt)    FMTI__(ARGC, FMTI__ ## fmt ## __)
#define FMT_ARGC_RD(fmt) FMTI__(ARGC_RD, FMTI__ ## fmt ## __)
#define FMT_ARGC_WR(fmt) FMTI__(ARGC_WR, FMTI__ ## fmt ## __)
#define FMT_LOWER(fmt)   FMTI__(LOWER, FMTI__ ## fmt ## __)
#define FMT_UPPER(fmt)   fmt

#ifndef OPCODE
#   define OPCODE(mnem, opcode, feat, fmt, ...)
#endif /* OPCODE */

#ifndef OPCODE_GRP
#   define OPCODE_GRP(grpname, opcode)
#endif /* OPCODE_GRP */

#ifndef OPCODE_GRP_BEGIN
#   define OPCODE_GRP_BEGIN(grpname)
#endif /* OPCODE_GRP_BEGIN */

#ifndef OPCODE_GRPMEMB
#   define OPCODE_GRPMEMB(grpname, mnem, opcode, feat, fmt, ...)
#endif /* OPCODE_GRPMEMB */

#ifndef OPCODE_GRP_END
#   define OPCODE_GRP_END(grpname)
#endif /* OPCODE_GRP_END */

/*
 * MMX Instructions
 * -----------------
 * NP 0F 6E /r          MOVD mm,r/m32
 * NP 0F 7E /r          MOVD r/m32,mm
 * NP REX.W + 0F 6E /r  MOVQ mm,r/m64
 * NP REX.W + 0F 7E /r  MOVQ r/m64,mm
 * NP 0F 6F /r          MOVQ mm, mm/m64
 * NP 0F 7F /r          MOVQ mm/m64, mm
 * NP 0F FC /r          PADDB mm, mm/m64
 * NP 0F FD /r          PADDW mm, mm/m64
 * NP 0F FE /r          PADDD mm, mm/m64
 * NP 0F EC /r          PADDSB mm, mm/m64
 * NP 0F ED /r          PADDSW mm, mm/m64
 * NP 0F DC /r          PADDUSB mm,mm/m64
 * NP 0F DD /r          PADDUSW mm,mm/m64
 * NP 0F F8 /r          PSUBB mm, mm/m64
 * NP 0F F9 /r          PSUBW mm, mm/m64
 * NP 0F FA /r          PSUBD mm, mm/m64
 * NP 0F E8 /r          PSUBSB mm, mm/m64
 * NP 0F E9 /r          PSUBSW mm, mm/m64
 * NP 0F D8 /r          PSUBUSB mm, mm/m64
 * NP 0F D9 /r          PSUBUSW mm, mm/m64
 * NP 0F D5 /r          PMULLW mm, mm/m64
 * NP 0F E5 /r          PMULHW mm, mm/m64
 * NP 0F F5 /r          PMADDWD mm, mm/m64
 * NP 0F 74 /r          PCMPEQB mm,mm/m64
 * NP 0F 75 /r          PCMPEQW mm,mm/m64
 * NP 0F 76 /r          PCMPEQD mm,mm/m64
 * NP 0F 64 /r          PCMPGTB mm,mm/m64
 * NP 0F 65 /r          PCMPGTW mm,mm/m64
 * NP 0F 66 /r          PCMPGTD mm,mm/m64
 * NP 0F DB /r          PAND mm, mm/m64
 * NP 0F DF /r          PANDN mm, mm/m64
 * NP 0F EB /r          POR mm, mm/m64
 * NP 0F EF /r          PXOR mm, mm/m64
 * NP 0F F1 /r          PSLLW mm, mm/m64
 * NP 0F F2 /r          PSLLD mm, mm/m64
 * NP 0F F3 /r          PSLLQ mm, mm/m64
 * NP 0F D1 /r          PSRLW mm, mm/m64
 * NP 0F D2 /r          PSRLD mm, mm/m64
 * NP 0F D3 /r          PSRLQ mm, mm/m64
 * NP 0F E1 /r          PSRAW mm,mm/m64
 * NP 0F E2 /r          PSRAD mm,mm/m64
 * NP 0F 63 /r          PACKSSWB mm1, mm2/m64
 * NP 0F 6B /r          PACKSSDW mm1, mm2/m64
 * NP 0F 67 /r          PACKUSWB mm, mm/m64
 * NP 0F 68 /r          PUNPCKHBW mm, mm/m64
 * NP 0F 69 /r          PUNPCKHWD mm, mm/m64
 * NP 0F 6A /r          PUNPCKHDQ mm, mm/m64
 * NP 0F 60 /r          PUNPCKLBW mm, mm/m32
 * NP 0F 61 /r          PUNPCKLWD mm, mm/m32
 * NP 0F 62 /r          PUNPCKLDQ mm, mm/m32
 * NP 0F 77             EMMS
 * NP 0F 71 /6 ib       PSLLW mm1, imm8
 * NP 0F 71 /2 ib       PSRLW mm, imm8
 * NP 0F 71 /4 ib       PSRAW mm,imm8
 * NP 0F 72 /6 ib       PSLLD mm, imm8
 * NP 0F 72 /2 ib       PSRLD mm, imm8
 * NP 0F 72 /4 ib       PSRAD mm,imm8
 * NP 0F 73 /6 ib       PSLLQ mm, imm8
 * NP 0F 73 /2 ib       PSRLQ mm, imm8
 *
 * SSE Instructions
 * -----------------
 * NP 0F 28 /r          MOVAPS xmm1, xmm2/m128
 * NP 0F 29 /r          MOVAPS xmm2/m128, xmm1
 * NP 0F 10 /r          MOVUPS xmm1, xmm2/m128
 * NP 0F 11 /r          MOVUPS xmm2/m128, xmm1
 * F3 0F 10 /r          MOVSS xmm1, xmm2/m32
 * F3 0F 11 /r          MOVSS xmm2/m32, xmm1
 * NP 0F 12 /r          MOVHLPS xmm1, xmm2
 * NP 0F 12 /r          MOVLPS xmm1, m64
 * 0F 13 /r             MOVLPS m64, xmm1
 * NP 0F 16 /r          MOVLHPS xmm1, xmm2
 * NP 0F 16 /r          MOVHPS xmm1, m64
 * NP 0F 17 /r          MOVHPS m64, xmm1
 * NP 0F D7 /r          PMOVMSKB r32, mm
 * NP REX.W 0F D7 /r    PMOVMSKB r64, mm
 * NP 0F 50 /r          MOVMSKPS r32, xmm
 * NP REX.W 0F 50 /r    MOVMSKPS r64, xmm
 * NP 0F 58 /r          ADDPS xmm1, xmm2/m128
 * F3 0F 58 /r          ADDSS xmm1, xmm2/m32
 * NP 0F 5C /r          SUBPS xmm1, xmm2/m128
 * F3 0F 5C /r          SUBSS xmm1, xmm2/m32
 * NP 0F E4 /r          PMULHUW mm1, mm2/m64
 * NP 0F 59 /r          MULPS xmm1, xmm2/m128
 * F3 0F 59 /r          MULSS xmm1,xmm2/m32
 * NP 0F 5E /r          DIVPS xmm1, xmm2/m128
 * F3 0F 5E /r          DIVSS xmm1, xmm2/m32
 * NP 0F 53 /r          RCPPS xmm1, xmm2/m128
 * F3 0F 53 /r          RCPSS xmm1, xmm2/m32
 * NP 0F 51 /r          SQRTPS xmm1, xmm2/m128
 * F3 0F 51 /r          SQRTSS xmm1, xmm2/m32
 * NP 0F 52 /r          RSQRTPS xmm1, xmm2/m128
 * F3 0F 52 /r          RSQRTSS xmm1, xmm2/m32
 * NP 0F DA /r          PMINUB mm1, mm2/m64
 * NP 0F EA /r          PMINSW mm1, mm2/m64
 * NP 0F 5D /r          MINPS xmm1, xmm2/m128
 * F3 0F 5D /r          MINSS xmm1,xmm2/m32
 * NP 0F DE /r          PMAXUB mm1, mm2/m64
 * NP 0F EE /r          PMAXSW mm1, mm2/m64
 * NP 0F 5F /r          MAXPS xmm1, xmm2/m128
 * F3 0F 5F /r          MAXSS xmm1, xmm2/m32
 * NP 0F E0 /r          PAVGB mm1, mm2/m64
 * NP 0F E3 /r          PAVGW mm1, mm2/m64
 * NP 0F F6 /r          PSADBW mm1, mm2/m64
 * NP 0F C2 /r ib       CMPPS xmm1, xmm2/m128, imm8
 * F3 0F C2 /r ib       CMPSS xmm1, xmm2/m32, imm8
 * NP 0F 2E /r          UCOMISS xmm1, xmm2/m32
 * NP 0F 2F /r          COMISS xmm1, xmm2/m32
 * NP 0F 54 /r          ANDPS xmm1, xmm2/m128
 * NP 0F 55 /r          ANDNPS xmm1, xmm2/m128
 * NP 0F 56 /r          ORPS xmm1, xmm2/m128
 * NP 0F 57 /r          XORPS xmm1, xmm2/m128
 * NP 0F 14 /r          UNPCKLPS xmm1, xmm2/m128
 * NP 0F 15 /r          UNPCKHPS xmm1, xmm2/m128
 * NP 0F 70 /r ib       PSHUFW mm1, mm2/m64, imm8
 * NP 0F C6 /r ib       SHUFPS xmm1, xmm3/m128, imm8
 * NP 0F C4 /r ib       PINSRW mm, r32/m16, imm8
 * NP 0F C5 /r ib       PEXTRW r32, mm, imm8
 * NP REX.W 0F C5 /r ib PEXTRW r64, mm, imm8
 * NP 0F 2A /r          CVTPI2PS xmm, mm/m64
 * F3 0F 2A /r          CVTSI2SS xmm1,r/m32
 * F3 REX.W 0F 2A /r    CVTSI2SS xmm1,r/m64
 * NP 0F 2D /r          CVTPS2PI mm, xmm/m64
 * F3 0F 2D /r          CVTSS2SI r32,xmm1/m32
 * F3 REX.W 0F 2D /r    CVTSS2SI r64,xmm1/m32
 * NP 0F 2C /r          CVTTPS2PI mm, xmm/m64
 * F3 0F 2C /r          CVTTSS2SI r32,xmm1/m32
 * F3 REX.W 0F 2C /r    CVTTSS2SI r64,xmm1/m32
 * NP 0F F7 /r          MASKMOVQ mm1, mm2
 * NP 0F 2B /r          MOVNTPS m128, xmm1
 * NP 0F E7 /r          MOVNTQ m64, mm
 * NP 0F AE /7          SFENCE
 * NP 0F AE /2          LDMXCSR m32
 * NP 0F AE /3          STMXCSR m32
 * 0F 18 /1             PREFETCHT0 m8
 * 0F 18 /2             PREFETCHT1 m8
 * 0F 18 /3             PREFETCHT2 m8
 * 0F 18 /0             PREFETCHNTA m8
 *
 * SSE2 Instructions
 * ------------------
 * 66 0F 6E /r          MOVD xmm,r/m32
 * 66 0F 7E /r          MOVD r/m32,xmm
 * 66 REX.W 0F 6E /r    MOVQ xmm,r/m64
 * 66 REX.W 0F 7E /r    MOVQ r/m64,xmm
 * F3 0F 7E /r          MOVQ xmm1, xmm2/m64
 * 66 0F D6 /r          MOVQ xmm2/m64, xmm1
 * 66 0F 28 /r          MOVAPD xmm1, xmm2/m128
 * 66 0F 29 /r          MOVAPD xmm2/m128, xmm1
 * 66 0F 6F /r          MOVDQA xmm1, xmm2/m128
 * 66 0F 7F /r          MOVDQA xmm2/m128, xmm1
 * 66 0F 10 /r          MOVUPD xmm1, xmm2/m128
 * 66 0F 11 /r          MOVUPD xmm2/m128, xmm1
 * F3 0F 6F /r          MOVDQU xmm1,xmm2/m128
 * F3 0F 7F /r          MOVDQU xmm2/m128,xmm1
 * F2 0F 10 /r          MOVSD xmm1, xmm2/m64
 * F2 0F 11 /r          MOVSD xmm1/m64, xmm2
 * F3 0F D6 /r          MOVQ2DQ xmm, mm
 * F2 0F D6 /r          MOVDQ2Q mm, xmm
 * 66 0F 12 /r          MOVLPD xmm1,m64
 * 66 0F 13 /r          MOVLPD m64,xmm1
 * 66 0F 16 /r          MOVHPD xmm1, m64
 * 66 0F 17 /r          MOVHPD m64, xmm1
 * 66 0F D7 /r          PMOVMSKB r32, xmm
 * 66 REX.W 0F D7 /r    PMOVMSKB r64, xmm
 * 66 0F 50 /r          MOVMSKPD r32, xmm
 * 66 REX.W 0F 50 /r    MOVMSKPD r64, xmm
 * 66 0F FC /r          PADDB xmm1, xmm2/m128
 * 66 0F FD /r          PADDW xmm1, xmm2/m128
 * 66 0F FE /r          PADDD xmm1, xmm2/m128
 * NP 0F D4 /r          PADDQ mm, mm/m64
 * 66 0F D4 /r          PADDQ xmm1, xmm2/m128
 * 66 0F EC /r          PADDSB xmm1, xmm2/m128
 * 66 0F ED /r          PADDSW xmm1, xmm2/m128
 * 66 0F DC /r          PADDUSB xmm1,xmm2/m128
 * 66 0F DD /r          PADDUSW xmm1,xmm2/m128
 * 66 0F 58 /r          ADDPD xmm1, xmm2/m128
 * F2 0F 58 /r          ADDSD xmm1, xmm2/m64
 * 66 0F F8 /r          PSUBB xmm1, xmm2/m128
 * 66 0F F9 /r          PSUBW xmm1, xmm2/m128
 * 66 0F FA /r          PSUBD xmm1, xmm2/m128
 * NP 0F FB /r          PSUBQ mm1, mm2/m64
 * 66 0F FB /r          PSUBQ xmm1, xmm2/m128
 * 66 0F E8 /r          PSUBSB xmm1, xmm2/m128
 * 66 0F E9 /r          PSUBSW xmm1, xmm2/m128
 * 66 0F D8 /r          PSUBUSB xmm1, xmm2/m128
 * 66 0F D9 /r          PSUBUSW xmm1, xmm2/m128
 * 66 0F 5C /r          SUBPD xmm1, xmm2/m128
 * F2 0F 5C /r          SUBSD xmm1, xmm2/m64
 * 66 0F D5 /r          PMULLW xmm1, xmm2/m128
 * 66 0F E5 /r          PMULHW xmm1, xmm2/m128
 * 66 0F E4 /r          PMULHUW xmm1, xmm2/m128
 * NP 0F F4 /r          PMULUDQ mm1, mm2/m64
 * 66 0F F4 /r          PMULUDQ xmm1, xmm2/m128
 * 66 0F 59 /r          MULPD xmm1, xmm2/m128
 * F2 0F 59 /r          MULSD xmm1,xmm2/m64
 * 66 0F F5 /r          PMADDWD xmm1, xmm2/m128
 * 66 0F 5E /r          DIVPD xmm1, xmm2/m128
 * F2 0F 5E /r          DIVSD xmm1, xmm2/m64
 * 66 0F 51 /r          SQRTPD xmm1, xmm2/m128
 * F2 0F 51 /r          SQRTSD xmm1,xmm2/m64
 * 66 0F DA /r          PMINUB xmm1, xmm2/m128
 * 66 0F EA /r          PMINSW xmm1, xmm2/m128
 * 66 0F 5D /r          MINPD xmm1, xmm2/m128
 * F2 0F 5D /r          MINSD xmm1, xmm2/m64
 * 66 0F DE /r          PMAXUB xmm1, xmm2/m128
 * 66 0F EE /r          PMAXSW xmm1, xmm2/m128
 * 66 0F 5F /r          MAXPD xmm1, xmm2/m128
 * F2 0F 5F /r          MAXSD xmm1, xmm2/m64
 * 66 0F E0 /r          PAVGB xmm1, xmm2/m128
 * 66 0F E3 /r          PAVGW xmm1, xmm2/m128
 * 66 0F F6 /r          PSADBW xmm1, xmm2/m128
 * 66 0F 74 /r          PCMPEQB xmm1,xmm2/m128
 * 66 0F 75 /r          PCMPEQW xmm1,xmm2/m128
 * 66 0F 76 /r          PCMPEQD xmm1,xmm2/m128
 * 66 0F 64 /r          PCMPGTB xmm1,xmm2/m128
 * 66 0F 65 /r          PCMPGTW xmm1,xmm2/m128
 * 66 0F 66 /r          PCMPGTD xmm1,xmm2/m128
 * 66 0F C2 /r ib       CMPPD xmm1, xmm2/m128, imm8
 * F2 0F C2 /r ib       CMPSD xmm1, xmm2/m64, imm8
 * 66 0F 2E /r          UCOMISD xmm1, xmm2/m64
 * 66 0F 2F /r          COMISD xmm1, xmm2/m64
 * 66 0F DB /r          PAND xmm1, xmm2/m128
 * 66 0F 54 /r          ANDPD xmm1, xmm2/m128
 * 66 0F DF /r          PANDN xmm1, xmm2/m128
 * 66 0F 55 /r          ANDNPD xmm1, xmm2/m128
 * 66 0F EB /r          POR xmm1, xmm2/m128
 * 66 0F 56 /r          ORPD xmm1, xmm2/m128
 * 66 0F EF /r          PXOR xmm1, xmm2/m128
 * 66 0F 57 /r          XORPD xmm1, xmm2/m128
 * 66 0F F1 /r          PSLLW xmm1, xmm2/m128
 * 66 0F F2 /r          PSLLD xmm1, xmm2/m128
 * 66 0F F3 /r          PSLLQ xmm1, xmm2/m128
 * 66 0F D1 /r          PSRLW xmm1, xmm2/m128
 * 66 0F D2 /r          PSRLD xmm1, xmm2/m128
 * 66 0F D3 /r          PSRLQ xmm1, xmm2/m128
 * 66 0F E1 /r          PSRAW xmm1,xmm2/m128
 * 66 0F E2 /r          PSRAD xmm1,xmm2/m128
 * 66 0F 63 /r          PACKSSWB xmm1, xmm2/m128
 * 66 0F 6B /r          PACKSSDW xmm1, xmm2/m128
 * 66 0F 67 /r          PACKUSWB xmm1, xmm2/m128
 * 66 0F 68 /r          PUNPCKHBW xmm1, xmm2/m128
 * 66 0F 69 /r          PUNPCKHWD xmm1, xmm2/m128
 * 66 0F 6A /r          PUNPCKHDQ xmm1, xmm2/m128
 * 66 0F 6D /r          PUNPCKHQDQ xmm1, xmm2/m128
 * 66 0F 60 /r          PUNPCKLBW xmm1, xmm2/m128
 * 66 0F 61 /r          PUNPCKLWD xmm1, xmm2/m128
 * 66 0F 62 /r          PUNPCKLDQ xmm1, xmm2/m128
 * 66 0F 6C /r          PUNPCKLQDQ xmm1, xmm2/m128
 * 66 0F 14 /r          UNPCKLPD xmm1, xmm2/m128
 * 66 0F 15 /r          UNPCKHPD xmm1, xmm2/m128
 * F2 0F 70 /r ib       PSHUFLW xmm1, xmm2/m128, imm8
 * F3 0F 70 /r ib       PSHUFHW xmm1, xmm2/m128, imm8
 * 66 0F 70 /r ib       PSHUFD xmm1, xmm2/m128, imm8
 * 66 0F C6 /r ib       SHUFPD xmm1, xmm2/m128, imm8
 * 66 0F C4 /r ib       PINSRW xmm, r32/m16, imm8
 * 66 0F C5 /r ib       PEXTRW r32, xmm, imm8
 * 66 REX.W 0F C5 /r ib PEXTRW r64, xmm, imm8
 * 66 0F 2A /r          CVTPI2PD xmm, mm/m64
 * F2 0F 2A /r          CVTSI2SD xmm1,r32/m32
 * F2 REX.W 0F 2A /r    CVTSI2SD xmm1,r/m64
 * 66 0F 2D /r          CVTPD2PI mm, xmm/m128
 * F2 0F 2D /r          CVTSD2SI r32,xmm1/m64
 * F2 REX.W 0F 2D /r    CVTSD2SI r64,xmm1/m64
 * 66 0F 2C /r          CVTTPD2PI mm, xmm/m128
 * F2 0F 2C /r          CVTTSD2SI r32,xmm1/m64
 * F2 REX.W 0F 2C /r    CVTTSD2SI r64,xmm1/m64
 * F2 0F E6 /r          CVTPD2DQ xmm1, xmm2/m128
 * 66 0F E6 /r          CVTTPD2DQ xmm1, xmm2/m128
 * F3 0F E6 /r          CVTDQ2PD xmm1, xmm2/m64
 * NP 0F 5A /r          CVTPS2PD xmm1, xmm2/m64
 * 66 0F 5A /r          CVTPD2PS xmm1, xmm2/m128
 * F3 0F 5A /r          CVTSS2SD xmm1, xmm2/m32
 * F2 0F 5A /r          CVTSD2SS xmm1, xmm2/m64
 * NP 0F 5B /r          CVTDQ2PS xmm1, xmm2/m128
 * 66 0F 5B /r          CVTPS2DQ xmm1, xmm2/m128
 * F3 0F 5B /r          CVTTPS2DQ xmm1, xmm2/m128
 * 66 0F F7 /r          MASKMOVDQU xmm1, xmm2
 * 66 0F 2B /r          MOVNTPD m128, xmm1
 * NP 0F C3 /r          MOVNTI m32, r32
 * NP REX.W + 0F C3 /r  MOVNTI m64, r64
 * 66 0F E7 /r          MOVNTDQ m128, xmm1
 * F3 90                PAUSE
 * 66 0F 71 /6 ib       PSLLW xmm1, imm8
 * 66 0F 71 /2 ib       PSRLW xmm1, imm8
 * 66 0F 71 /4 ib       PSRAW xmm1,imm8
 * 66 0F 72 /6 ib       PSLLD xmm1, imm8
 * 66 0F 72 /2 ib       PSRLD xmm1, imm8
 * 66 0F 72 /4 ib       PSRAD xmm1,imm8
 * 66 0F 73 /6 ib       PSLLQ xmm1, imm8
 * 66 0F 73 /7 ib       PSLLDQ xmm1, imm8
 * 66 0F 73 /2 ib       PSRLQ xmm1, imm8
 * 66 0F 73 /3 ib       PSRLDQ xmm1, imm8
 * NP 0F AE /7          CLFLUSH m8
 * NP 0F AE /5          LFENCE
 * NP 0F AE /6          MFENCE
 *
 * SSE3 Instructions
 * ------------------
 * F2 0F F0 /r          LDDQU xmm1, m128
 * F3 0F 16 /r          MOVSHDUP xmm1, xmm2/m128
 * F3 0F 12 /r          MOVSLDUP xmm1, xmm2/m128
 * F2 0F 12 /r          MOVDDUP xmm1, xmm2/m64
 * F2 0F 7C /r          HADDPS xmm1, xmm2/m128
 * 66 0F 7C /r          HADDPD xmm1, xmm2/m128
 * F2 0F 7D /r          HSUBPS xmm1, xmm2/m128
 * 66 0F 7D /r          HSUBPD xmm1, xmm2/m128
 * F2 0F D0 /r          ADDSUBPS xmm1, xmm2/m128
 * 66 0F D0 /r          ADDSUBPD xmm1, xmm2/m128
 *
 * SSSE3 Instructions
 * -------------------
 * NP 0F 38 01 /r          PHADDW mm1, mm2/m64
 * 66 0F 38 01 /r          PHADDW xmm1, xmm2/m128
 * NP 0F 38 02 /r          PHADDD mm1, mm2/m64
 * 66 0F 38 02 /r          PHADDD xmm1, xmm2/m128
 * NP 0F 38 03 /r          PHADDSW mm1, mm2/m64
 * 66 0F 38 03 /r          PHADDSW xmm1, xmm2/m128
 * NP 0F 38 05 /r          PHSUBW mm1, mm2/m64
 * 66 0F 38 05 /r          PHSUBW xmm1, xmm2/m128
 * NP 0F 38 06 /r          PHSUBD mm1, mm2/m64
 * 66 0F 38 06 /r          PHSUBD xmm1, xmm2/m128
 * NP 0F 38 07 /r          PHSUBSW mm1, mm2/m64
 * 66 0F 38 07 /r          PHSUBSW xmm1, xmm2/m128
 * NP 0F 38 0B /r          PMULHRSW mm1, mm2/m64
 * 66 0F 38 0B /r          PMULHRSW xmm1, xmm2/m128
 * NP 0F 38 04 /r          PMADDUBSW mm1, mm2/m64
 * 66 0F 38 04 /r          PMADDUBSW xmm1, xmm2/m128
 * NP 0F 38 1C /r          PABSB mm1, mm2/m64
 * 66 0F 38 1C /r          PABSB xmm1, xmm2/m128
 * NP 0F 38 1D /r          PABSW mm1, mm2/m64
 * 66 0F 38 1D /r          PABSW xmm1, xmm2/m128
 * NP 0F 38 1E /r          PABSD mm1, mm2/m64
 * 66 0F 38 1E /r          PABSD xmm1, xmm2/m128
 * NP 0F 38 08 /r          PSIGNB mm1, mm2/m64
 * 66 0F 38 08 /r          PSIGNB xmm1, xmm2/m128
 * NP 0F 38 09 /r          PSIGNW mm1, mm2/m64
 * 66 0F 38 09 /r          PSIGNW xmm1, xmm2/m128
 * NP 0F 38 0A /r          PSIGND mm1, mm2/m64
 * 66 0F 38 0A /r          PSIGND xmm1, xmm2/m128
 * NP 0F 3A 0F /r ib       PALIGNR mm1, mm2/m64, imm8
 * 66 0F 3A 0F /r ib       PALIGNR xmm1, xmm2/m128, imm8
 * NP 0F 38 00 /r          PSHUFB mm1, mm2/m64
 * 66 0F 38 00 /r          PSHUFB xmm1, xmm2/m128
 *
 * SSE4.1 Instructions
 * --------------------
 * 66 0F 38 40 /r          PMULLD xmm1, xmm2/m128
 * 66 0F 38 28 /r          PMULDQ xmm1, xmm2/m128
 * 66 0F 38 3A /r          PMINUW xmm1, xmm2/m128
 * 66 0F 38 3B /r          PMINUD xmm1, xmm2/m128
 * 66 0F 38 38 /r          PMINSB xmm1, xmm2/m128
 * 66 0F 38 39 /r          PMINSD xmm1, xmm2/m128
 * 66 0F 38 41 /r          PHMINPOSUW xmm1, xmm2/m128
 * 66 0F 38 3E /r          PMAXUW xmm1, xmm2/m128
 * 66 0F 38 3F /r          PMAXUD xmm1, xmm2/m128
 * 66 0F 38 3C /r          PMAXSB xmm1, xmm2/m128
 * 66 0F 38 3D /r          PMAXSD xmm1, xmm2/m128
 * 66 0F 3A 42 /r ib       MPSADBW xmm1, xmm2/m128, imm8
 * 66 0F 3A 40 /r ib       DPPS xmm1, xmm2/m128, imm8
 * 66 0F 3A 41 /r ib       DPPD xmm1, xmm2/m128, imm8
 * 66 0F 3A 08 /r ib       ROUNDPS xmm1, xmm2/m128, imm8
 * 66 0F 3A 09 /r ib       ROUNDPD xmm1, xmm2/m128, imm8
 * 66 0F 3A 0A /r ib       ROUNDSS xmm1, xmm2/m32, imm8
 * 66 0F 3A 0B /r ib       ROUNDSD xmm1, xmm2/m64, imm8
 * 66 0F 38 29 /r          PCMPEQQ xmm1, xmm2/m128
 * 66 0F 38 17 /r          PTEST xmm1, xmm2/m128
 * 66 0F 38 2B /r          PACKUSDW xmm1, xmm2/m128
 * 66 0F 3A 0C /r ib       BLENDPS xmm1, xmm2/m128, imm8
 * 66 0F 3A 0D /r ib       BLENDPD xmm1, xmm2/m128, imm8
 * 66 0F 38 14 /r          BLENDVPS xmm1, xmm2/m128, <XMM0>
 * 66 0F 38 15 /r          BLENDVPD xmm1, xmm2/m128 , <XMM0>
 * 66 0F 38 10 /r          PBLENDVB xmm1, xmm2/m128, <XMM0>
 * 66 0F 3A 0E /r ib       PBLENDW xmm1, xmm2/m128, imm8
 * 66 0F 3A 21 /r ib       INSERTPS xmm1, xmm2/m32, imm8
 * 66 0F 3A 20 /r ib       PINSRB xmm1,r32/m8,imm8
 * 66 0F 3A 22 /r ib       PINSRD xmm1,r/m32,imm8
 * 66 REX.W 0F 3A 22 /r ib PINSRQ xmm1,r/m64,imm8
 * 66 0F 3A 17 /r ib       EXTRACTPS reg/m32, xmm1, imm8
 * 66 0F 3A 14 /r ib       PEXTRB r32/m8,xmm2,imm8
 * 66 0F 3A 15 /r ib       PEXTRW r32/m16, xmm, imm8
 * 66 0F 3A 16 /r ib       PEXTRD r/m32,xmm2,imm8
 * 66 REX.W 0F 3A 16 /r ib PEXTRQ r/m64,xmm2,imm8
 * 66 0f 38 20 /r          PMOVSXBW xmm1, xmm2/m64
 * 66 0f 38 21 /r          PMOVSXBD xmm1, xmm2/m32
 * 66 0f 38 22 /r          PMOVSXBQ xmm1, xmm2/m16
 * 66 0f 38 23 /r          PMOVSXWD xmm1, xmm2/m64
 * 66 0f 38 24 /r          PMOVSXWQ xmm1, xmm2/m32
 * 66 0f 38 25 /r          PMOVSXDQ xmm1, xmm2/m64
 * 66 0f 38 30 /r          PMOVZXBW xmm1, xmm2/m64
 * 66 0f 38 31 /r          PMOVZXBD xmm1, xmm2/m32
 * 66 0f 38 32 /r          PMOVZXBQ xmm1, xmm2/m16
 * 66 0f 38 33 /r          PMOVZXWD xmm1, xmm2/m64
 * 66 0f 38 34 /r          PMOVZXWQ xmm1, xmm2/m32
 * 66 0f 38 35 /r          PMOVZXDQ xmm1, xmm2/m64
 * 66 0F 38 2A /r          MOVNTDQA xmm1, m128
 *
 * SSE4.2 Instructions
 * --------------------
 * 66 0F 38 37 /r          PCMPGTQ xmm1,xmm2/m128
 * 66 0F 3A 60 /r imm8     PCMPESTRM xmm1, xmm2/m128, imm8
 * 66 0F 3A 61 /r imm8     PCMPESTRI xmm1, xmm2/m128, imm8
 * 66 0F 3A 62 /r imm8     PCMPISTRM xmm1, xmm2/m128, imm8
 * 66 0F 3A 63 /r imm8     PCMPISTRI xmm1, xmm2/m128, imm8
 *
 * AES Instructions
 * -----------------
 * 66 0F 38 DE /r                  AESDEC xmm1, xmm2/m128
 * VEX.128.66.0F38.WIG DE /r       VAESDEC xmm1, xmm2, xmm3/m128
 * 66 0F 38 DF /r                  AESDECLAST xmm1, xmm2/m128
 * VEX.128.66.0F38.WIG DF /r       VAESDECLAST xmm1, xmm2, xmm3/m128
 * 66 0F 38 DC /r                  AESENC xmm1, xmm2/m128
 * VEX.128.66.0F38.WIG DC /r       VAESENC xmm1, xmm2, xmm3/m128
 * 66 0F 38 DD /r                  AESENCLAST xmm1, xmm2/m128
 * VEX.128.66.0F38.WIG DD /r       VAESENCLAST xmm1, xmm2, xmm3/m128
 * 66 0F 38 DB /r                  AESIMC xmm1, xmm2/m128
 * VEX.128.66.0F38.WIG DB /r       VAESIMC xmm1, xmm2/m128
 * 66 0F 3A DF /r ib               AESKEYGENASSIST xmm1, xmm2/m128, imm8
 * VEX.128.66.0F3A.WIG DF /r ib    VAESKEYGENASSIST xmm1, xmm2/m128, imm8
 *
 * PCLMULQDQ Instructions
 * -----------------------
 * 66 0F 3A 44 /r ib               PCLMULQDQ xmm1, xmm2/m128, imm8
 * VEX.128.66.0F3A.WIG 44 /r ib    VPCLMULQDQ xmm1, xmm2, xmm3/m128, imm8
 *
 * AVX Instructions
 * -----------------
 * VEX.128.66.0F.W0 6E /r          VMOVD xmm1,r32/m32
 * VEX.128.66.0F.W0 7E /r          VMOVD r32/m32,xmm1
 * VEX.128.66.0F.W1 6E /r          VMOVQ xmm1,r64/m64
 * VEX.128.66.0F.W1 7E /r          VMOVQ r64/m64,xmm1
 * VEX.128.F3.0F.WIG 7E /r         VMOVQ xmm1, xmm2/m64
 * VEX.128.66.0F.WIG D6 /r         VMOVQ xmm1/m64, xmm2
 * VEX.128.0F.WIG 28 /r            VMOVAPS xmm1, xmm2/m128
 * VEX.128.0F.WIG 29 /r            VMOVAPS xmm2/m128, xmm1
 * VEX.256.0F.WIG 28 /r            VMOVAPS ymm1, ymm2/m256
 * VEX.256.0F.WIG 29 /r            VMOVAPS ymm2/m256, ymm1
 * VEX.128.66.0F.WIG 28 /r         VMOVAPD xmm1, xmm2/m128
 * VEX.128.66.0F.WIG 29 /r         VMOVAPD xmm2/m128, xmm1
 * VEX.256.66.0F.WIG 28 /r         VMOVAPD ymm1, ymm2/m256
 * VEX.256.66.0F.WIG 29 /r         VMOVAPD ymm2/m256, ymm1
 * VEX.128.66.0F.WIG 6F /r         VMOVDQA xmm1, xmm2/m128
 * VEX.128.66.0F.WIG 7F /r         VMOVDQA xmm2/m128, xmm1
 * VEX.256.66.0F.WIG 6F /r         VMOVDQA ymm1, ymm2/m256
 * VEX.256.66.0F.WIG 7F /r         VMOVDQA ymm2/m256, ymm1
 * VEX.128.0F.WIG 10 /r            VMOVUPS xmm1, xmm2/m128
 * VEX.128.0F.WIG 11 /r            VMOVUPS xmm2/m128, xmm1
 * VEX.256.0F.WIG 10 /r            VMOVUPS ymm1, ymm2/m256
 * VEX.256.0F.WIG 11 /r            VMOVUPS ymm2/m256, ymm1
 * VEX.128.66.0F.WIG 10 /r         VMOVUPD xmm1, xmm2/m128
 * VEX.128.66.0F.WIG 11 /r         VMOVUPD xmm2/m128, xmm1
 * VEX.256.66.0F.WIG 10 /r         VMOVUPD ymm1, ymm2/m256
 * VEX.256.66.0F.WIG 11 /r         VMOVUPD ymm2/m256, ymm1
 * VEX.128.F3.0F.WIG 6F /r         VMOVDQU xmm1,xmm2/m128
 * VEX.128.F3.0F.WIG 7F /r         VMOVDQU xmm2/m128,xmm1
 * VEX.256.F3.0F.WIG 6F /r         VMOVDQU ymm1,ymm2/m256
 * VEX.256.F3.0F.WIG 7F /r         VMOVDQU ymm2/m256,ymm1
 * VEX.LIG.F3.0F.WIG 10 /r         VMOVSS xmm1, xmm2, xmm3
 * VEX.LIG.F3.0F.WIG 10 /r         VMOVSS xmm1, m32
 * VEX.LIG.F3.0F.WIG 11 /r         VMOVSS xmm1, xmm2, xmm3
 * VEX.LIG.F3.0F.WIG 11 /r         VMOVSS m32, xmm1
 * VEX.LIG.F2.0F.WIG 10 /r         VMOVSD xmm1, xmm2, xmm3
 * VEX.LIG.F2.0F.WIG 10 /r         VMOVSD xmm1, m64
 * VEX.LIG.F2.0F.WIG 11 /r         VMOVSD xmm1, xmm2, xmm3
 * VEX.LIG.F2.0F.WIG 11 /r         VMOVSD m64, xmm1
 * VEX.128.0F.WIG 12 /r            VMOVHLPS xmm1, xmm2, xmm3
 * VEX.128.0F.WIG 12 /r            VMOVLPS xmm2, xmm1, m64
 * VEX.128.0F.WIG 13 /r            VMOVLPS m64, xmm1
 * VEX.128.66.0F.WIG 12 /r         VMOVLPD xmm2,xmm1,m64
 * VEX.128.66.0F.WIG 13 /r         VMOVLPD m64,xmm1
 * VEX.128.0F.WIG 16 /r            VMOVLHPS xmm1, xmm2, xmm3
 * VEX.128.0F.WIG 16 /r            VMOVHPS xmm2, xmm1, m64
 * VEX.128.0F.WIG 17 /r            VMOVHPS m64, xmm1
 * VEX.128.66.0F.WIG 16 /r         VMOVHPD xmm2, xmm1, m64
 * VEX.128.66.0F.WIG 17 /r         VMOVHPD m64, xmm1
 * VEX.128.66.0F.W0 D7 /r          VPMOVMSKB r32, xmm1
 * VEX.128.66.0F.W1 D7 /r          VPMOVMSKB r64, xmm1
 * VEX.128.0F.W0 50 /r             VMOVMSKPS r32, xmm2
 * VEX.128.0F.W1 50 /r             VMOVMSKPS r64, xmm2
 * VEX.256.0F.W0 50 /r             VMOVMSKPS r32, ymm2
 * VEX.256.0F.W1 50 /r             VMOVMSKPS r64, ymm2
 * VEX.128.66.0F.W0 50 /r          VMOVMSKPD r32, xmm2
 * VEX.128.66.0F.W1 50 /r          VMOVMSKPD r64, xmm2
 * VEX.256.66.0F.W0 50 /r          VMOVMSKPD r32, ymm2
 * VEX.256.66.0F.W1 50 /r          VMOVMSKPD r64, ymm2
 * VEX.128.F2.0F.WIG F0 /r         VLDDQU xmm1, m128
 * VEX.256.F2.0F.WIG F0 /r         VLDDQU ymm1, m256
 * VEX.128.F3.0F.WIG 16 /r         VMOVSHDUP xmm1, xmm2/m128
 * VEX.256.F3.0F.WIG 16 /r         VMOVSHDUP ymm1, ymm2/m256
 * VEX.128.F3.0F.WIG 12 /r         VMOVSLDUP xmm1, xmm2/m128
 * VEX.256.F3.0F.WIG 12 /r         VMOVSLDUP ymm1, ymm2/m256
 * VEX.128.F2.0F.WIG 12 /r         VMOVDDUP xmm1, xmm2/m64
 * VEX.256.F2.0F.WIG 12 /r         VMOVDDUP ymm1, ymm2/m256
 * VEX.128.66.0F.WIG FC /r         VPADDB xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG FD /r         VPADDW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG FE /r         VPADDD xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG D4 /r         VPADDQ xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG EC /r         VPADDSB xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG ED /r         VPADDSW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG DC /r         VPADDUSB xmm1,xmm2,xmm3/m128
 * VEX.128.66.0F.WIG DD /r         VPADDUSW xmm1,xmm2,xmm3/m128
 * VEX.128.0F.WIG 58 /r            VADDPS xmm1,xmm2, xmm3/m128
 * VEX.256.0F.WIG 58 /r            VADDPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG 58 /r         VADDPD xmm1,xmm2, xmm3/m128
 * VEX.256.66.0F.WIG 58 /r         VADDPD ymm1, ymm2, ymm3/m256
 * VEX.LIG.F3.0F.WIG 58 /r         VADDSS xmm1,xmm2, xmm3/m32
 * VEX.LIG.F2.0F.WIG 58 /r         VADDSD xmm1, xmm2, xmm3/m64
 * VEX.128.66.0F38.WIG 01 /r       VPHADDW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 02 /r       VPHADDD xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 03 /r       VPHADDSW xmm1, xmm2, xmm3/m128
 * VEX.128.F2.0F.WIG 7C /r         VHADDPS xmm1, xmm2, xmm3/m128
 * VEX.256.F2.0F.WIG 7C /r         VHADDPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG 7C /r         VHADDPD xmm1,xmm2, xmm3/m128
 * VEX.256.66.0F.WIG 7C /r         VHADDPD ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG F8 /r         VPSUBB xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG F9 /r         VPSUBW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG FA /r         VPSUBD xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG FB /r         VPSUBQ xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG E8 /r         VPSUBSB xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG E9 /r         VPSUBSW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG D8 /r         VPSUBUSB xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG D9 /r         VPSUBUSW xmm1, xmm2, xmm3/m128
 * VEX.128.0F.WIG 5C /r            VSUBPS xmm1,xmm2, xmm3/m128
 * VEX.256.0F.WIG 5C /r            VSUBPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG 5C /r         VSUBPD xmm1,xmm2, xmm3/m128
 * VEX.256.66.0F.WIG 5C /r         VSUBPD ymm1, ymm2, ymm3/m256
 * VEX.LIG.F3.0F.WIG 5C /r         VSUBSS xmm1,xmm2, xmm3/m32
 * VEX.LIG.F2.0F.WIG 5C /r         VSUBSD xmm1,xmm2, xmm3/m64
 * VEX.128.66.0F38.WIG 05 /r       VPHSUBW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 06 /r       VPHSUBD xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 07 /r       VPHSUBSW xmm1, xmm2, xmm3/m128
 * VEX.128.F2.0F.WIG 7D /r         VHSUBPS xmm1, xmm2, xmm3/m128
 * VEX.256.F2.0F.WIG 7D /r         VHSUBPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG 7D /r         VHSUBPD xmm1,xmm2, xmm3/m128
 * VEX.256.66.0F.WIG 7D /r         VHSUBPD ymm1, ymm2, ymm3/m256
 * VEX.128.F2.0F.WIG D0 /r         VADDSUBPS xmm1, xmm2, xmm3/m128
 * VEX.256.F2.0F.WIG D0 /r         VADDSUBPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG D0 /r         VADDSUBPD xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F.WIG D0 /r         VADDSUBPD ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG D5 /r         VPMULLW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 40 /r       VPMULLD xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG E5 /r         VPMULHW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG E4 /r         VPMULHUW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 28 /r       VPMULDQ xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG F4 /r         VPMULUDQ xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 0B /r       VPMULHRSW xmm1, xmm2, xmm3/m128
 * VEX.128.0F.WIG 59 /r            VMULPS xmm1,xmm2, xmm3/m128
 * VEX.256.0F.WIG 59 /r            VMULPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG 59 /r         VMULPD xmm1,xmm2, xmm3/m128
 * VEX.256.66.0F.WIG 59 /r         VMULPD ymm1, ymm2, ymm3/m256
 * VEX.LIG.F3.0F.WIG 59 /r         VMULSS xmm1,xmm2, xmm3/m32
 * VEX.LIG.F2.0F.WIG 59 /r         VMULSD xmm1,xmm2, xmm3/m64
 * VEX.128.66.0F.WIG F5 /r         VPMADDWD xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 04 /r       VPMADDUBSW xmm1, xmm2, xmm3/m128
 * VEX.128.0F.WIG 5E /r            VDIVPS xmm1, xmm2, xmm3/m128
 * VEX.256.0F.WIG 5E /r            VDIVPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG 5E /r         VDIVPD xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F.WIG 5E /r         VDIVPD ymm1, ymm2, ymm3/m256
 * VEX.LIG.F3.0F.WIG 5E /r         VDIVSS xmm1, xmm2, xmm3/m32
 * VEX.LIG.F2.0F.WIG 5E /r         VDIVSD xmm1, xmm2, xmm3/m64
 * VEX.128.0F.WIG 53 /r            VRCPPS xmm1, xmm2/m128
 * VEX.256.0F.WIG 53 /r            VRCPPS ymm1, ymm2/m256
 * VEX.LIG.F3.0F.WIG 53 /r         VRCPSS xmm1, xmm2, xmm3/m32
 * VEX.128.0F.WIG 51 /r            VSQRTPS xmm1, xmm2/m128
 * VEX.256.0F.WIG 51 /r            VSQRTPS ymm1, ymm2/m256
 * VEX.128.66.0F.WIG 51 /r         VSQRTPD xmm1, xmm2/m128
 * VEX.256.66.0F.WIG 51 /r         VSQRTPD ymm1, ymm2/m256
 * VEX.LIG.F3.0F.WIG 51 /r         VSQRTSS xmm1, xmm2, xmm3/m32
 * VEX.LIG.F2.0F.WIG 51 /r         VSQRTSD xmm1,xmm2, xmm3/m64
 * VEX.128.0F.WIG 52 /r            VRSQRTPS xmm1, xmm2/m128
 * VEX.256.0F.WIG 52 /r            VRSQRTPS ymm1, ymm2/m256
 * VEX.LIG.F3.0F.WIG 52 /r         VRSQRTSS xmm1, xmm2, xmm3/m32
 * VEX.128.66.0F DA /r             VPMINUB xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38 3A /r           VPMINUW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 3B /r       VPMINUD xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38 38 /r           VPMINSB xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F EA /r             VPMINSW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 39 /r       VPMINSD xmm1, xmm2, xmm3/m128
 * VEX.128.0F.WIG 5D /r            VMINPS xmm1, xmm2, xmm3/m128
 * VEX.256.0F.WIG 5D /r            VMINPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG 5D /r         VMINPD xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F.WIG 5D /r         VMINPD ymm1, ymm2, ymm3/m256
 * VEX.LIG.F3.0F.WIG 5D /r         VMINSS xmm1,xmm2, xmm3/m32
 * VEX.LIG.F2.0F.WIG 5D /r         VMINSD xmm1, xmm2, xmm3/m64
 * VEX.128.66.0F38.WIG 41 /r       VPHMINPOSUW xmm1, xmm2/m128
 * VEX.128.66.0F DE /r             VPMAXUB xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38 3E /r           VPMAXUW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 3F /r       VPMAXUD xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 3C /r       VPMAXSB xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG EE /r         VPMAXSW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 3D /r       VPMAXSD xmm1, xmm2, xmm3/m128
 * VEX.128.0F.WIG 5F /r            VMAXPS xmm1, xmm2, xmm3/m128
 * VEX.256.0F.WIG 5F /r            VMAXPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG 5F /r         VMAXPD xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F.WIG 5F /r         VMAXPD ymm1, ymm2, ymm3/m256
 * VEX.LIG.F3.0F.WIG 5F /r         VMAXSS xmm1, xmm2, xmm3/m32
 * VEX.LIG.F2.0F.WIG 5F /r         VMAXSD xmm1, xmm2, xmm3/m64
 * VEX.128.66.0F.WIG E0 /r         VPAVGB xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG E3 /r         VPAVGW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG F6 /r         VPSADBW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F3A.WIG 42 /r ib    VMPSADBW xmm1, xmm2, xmm3/m128, imm8
 * VEX.128.66.0F38.WIG 1C /r       VPABSB xmm1, xmm2/m128
 * VEX.128.66.0F38.WIG 1D /r       VPABSW xmm1, xmm2/m128
 * VEX.128.66.0F38.WIG 1E /r       VPABSD xmm1, xmm2/m128
 * VEX.128.66.0F38.WIG 08 /r       VPSIGNB xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 09 /r       VPSIGNW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38.WIG 0A /r       VPSIGND xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F3A.WIG 40 /r ib    VDPPS xmm1,xmm2, xmm3/m128, imm8
 * VEX.256.66.0F3A.WIG 40 /r ib    VDPPS ymm1, ymm2, ymm3/m256, imm8
 * VEX.128.66.0F3A.WIG 41 /r ib    VDPPD xmm1,xmm2, xmm3/m128, imm8
 * VEX.128.66.0F3A.WIG 08 /r ib    VROUNDPS xmm1, xmm2/m128, imm8
 * VEX.256.66.0F3A.WIG 08 /r ib    VROUNDPS ymm1, ymm2/m256, imm8
 * VEX.128.66.0F3A.WIG 09 /r ib    VROUNDPD xmm1, xmm2/m128, imm8
 * VEX.256.66.0F3A.WIG 09 /r ib    VROUNDPD ymm1, ymm2/m256, imm8
 * VEX.LIG.66.0F3A.WIG 0A /r ib    VROUNDSS xmm1, xmm2, xmm3/m32, imm8
 * VEX.LIG.66.0F3A.WIG 0B /r ib    VROUNDSD xmm1, xmm2, xmm3/m64, imm8
 * VEX.128.66.0F.WIG 74 /r         VPCMPEQB xmm1,xmm2,xmm3/m128
 * VEX.128.66.0F.WIG 75 /r         VPCMPEQW xmm1,xmm2,xmm3/m128
 * VEX.128.66.0F.WIG 76 /r         VPCMPEQD xmm1,xmm2,xmm3/m128
 * VEX.128.66.0F38.WIG 29 /r       VPCMPEQQ xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG 64 /r         VPCMPGTB xmm1,xmm2,xmm3/m128
 * VEX.128.66.0F.WIG 65 /r         VPCMPGTW xmm1,xmm2,xmm3/m128
 * VEX.128.66.0F.WIG 66 /r         VPCMPGTD xmm1,xmm2,xmm3/m128
 * VEX.128.66.0F38.WIG 37 /r       VPCMPGTQ xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F3A 60 /r ib        VPCMPESTRM xmm1, xmm2/m128, imm8
 * VEX.128.66.0F3A 61 /r ib        VPCMPESTRI xmm1, xmm2/m128, imm8
 * VEX.128.66.0F3A.WIG 62 /r ib    VPCMPISTRM xmm1, xmm2/m128, imm8
 * VEX.128.66.0F3A.WIG 63 /r ib    VPCMPISTRI xmm1, xmm2/m128, imm8
 * VEX.128.66.0F38.WIG 17 /r       VPTEST xmm1, xmm2/m128
 * VEX.256.66.0F38.WIG 17 /r       VPTEST ymm1, ymm2/m256
 * VEX.128.66.0F38.W0 0E /r        VTESTPS xmm1, xmm2/m128
 * VEX.256.66.0F38.W0 0E /r        VTESTPS ymm1, ymm2/m256
 * VEX.128.66.0F38.W0 0F /r        VTESTPD xmm1, xmm2/m128
 * VEX.256.66.0F38.W0 0F /r        VTESTPD ymm1, ymm2/m256
 * VEX.128.0F.WIG C2 /r ib         VCMPPS xmm1, xmm2, xmm3/m128, imm8
 * VEX.256.0F.WIG C2 /r ib         VCMPPS ymm1, ymm2, ymm3/m256, imm8
 * VEX.128.66.0F.WIG C2 /r ib      VCMPPD xmm1, xmm2, xmm3/m128, imm8
 * VEX.256.66.0F.WIG C2 /r ib      VCMPPD ymm1, ymm2, ymm3/m256, imm8
 * VEX.LIG.F3.0F.WIG C2 /r ib      VCMPSS xmm1, xmm2, xmm3/m32, imm8
 * VEX.LIG.F2.0F.WIG C2 /r ib      VCMPSD xmm1, xmm2, xmm3/m64, imm8
 * VEX.LIG.0F.WIG 2E /r            VUCOMISS xmm1, xmm2/m32
 * VEX.LIG.66.0F.WIG 2E /r         VUCOMISD xmm1, xmm2/m64
 * VEX.LIG.0F.WIG 2F /r            VCOMISS xmm1, xmm2/m32
 * VEX.LIG.66.0F.WIG 2F /r         VCOMISD xmm1, xmm2/m64
 * VEX.128.66.0F.WIG DB /r         VPAND xmm1, xmm2, xmm3/m128
 * VEX.128.0F 54 /r                VANDPS xmm1,xmm2, xmm3/m128
 * VEX.256.0F 54 /r                VANDPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F 54 /r             VANDPD xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F 54 /r             VANDPD ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG DF /r         VPANDN xmm1, xmm2, xmm3/m128
 * VEX.128.0F 55 /r                VANDNPS xmm1, xmm2, xmm3/m128
 * VEX.256.0F 55 /r                VANDNPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F 55 /r             VANDNPD xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F 55 /r             VANDNPD ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG EB /r         VPOR xmm1, xmm2, xmm3/m128
 * VEX.128.0F 56 /r                VORPS xmm1,xmm2, xmm3/m128
 * VEX.256.0F 56 /r                VORPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F 56 /r             VORPD xmm1,xmm2, xmm3/m128
 * VEX.256.66.0F 56 /r             VORPD ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG EF /r         VPXOR xmm1, xmm2, xmm3/m128
 * VEX.128.0F.WIG 57 /r            VXORPS xmm1,xmm2, xmm3/m128
 * VEX.256.0F.WIG 57 /r            VXORPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG 57 /r         VXORPD xmm1,xmm2, xmm3/m128
 * VEX.256.66.0F.WIG 57 /r         VXORPD ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG F1 /r         VPSLLW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG F2 /r         VPSLLD xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG F3 /r         VPSLLQ xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG D1 /r         VPSRLW xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG D2 /r         VPSRLD xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG D3 /r         VPSRLQ xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG E1 /r         VPSRAW xmm1,xmm2,xmm3/m128
 * VEX.128.66.0F.WIG E2 /r         VPSRAD xmm1,xmm2,xmm3/m128
 * VEX.128.66.0F3A.WIG 0F /r ib    VPALIGNR xmm1, xmm2, xmm3/m128, imm8
 * VEX.128.66.0F.WIG 63 /r         VPACKSSWB xmm1,xmm2, xmm3/m128
 * VEX.128.66.0F.WIG 6B /r         VPACKSSDW xmm1,xmm2, xmm3/m128
 * VEX.128.66.0F.WIG 67 /r         VPACKUSWB xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F38 2B /r           VPACKUSDW xmm1,xmm2, xmm3/m128
 * VEX.128.66.0F.WIG 68 /r         VPUNPCKHBW xmm1,xmm2, xmm3/m128
 * VEX.128.66.0F.WIG 69 /r         VPUNPCKHWD xmm1,xmm2, xmm3/m128
 * VEX.128.66.0F.WIG 6A /r         VPUNPCKHDQ xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG 6D /r         VPUNPCKHQDQ xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG 60 /r         VPUNPCKLBW xmm1,xmm2, xmm3/m128
 * VEX.128.66.0F.WIG 61 /r         VPUNPCKLWD xmm1,xmm2, xmm3/m128
 * VEX.128.66.0F.WIG 62 /r         VPUNPCKLDQ xmm1, xmm2, xmm3/m128
 * VEX.128.66.0F.WIG 6C /r         VPUNPCKLQDQ xmm1, xmm2, xmm3/m128
 * VEX.128.0F.WIG 14 /r            VUNPCKLPS xmm1,xmm2, xmm3/m128
 * VEX.256.0F.WIG 14 /r            VUNPCKLPS ymm1,ymm2,ymm3/m256
 * VEX.128.66.0F.WIG 14 /r         VUNPCKLPD xmm1,xmm2, xmm3/m128
 * VEX.256.66.0F.WIG 14 /r         VUNPCKLPD ymm1,ymm2, ymm3/m256
 * VEX.128.0F.WIG 15 /r            VUNPCKHPS xmm1, xmm2, xmm3/m128
 * VEX.256.0F.WIG 15 /r            VUNPCKHPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F.WIG 15 /r         VUNPCKHPD xmm1,xmm2, xmm3/m128
 * VEX.256.66.0F.WIG 15 /r         VUNPCKHPD ymm1,ymm2, ymm3/m256
 * VEX.128.66.0F38.WIG 00 /r       VPSHUFB xmm1, xmm2, xmm3/m128
 * VEX.128.F2.0F.WIG 70 /r ib      VPSHUFLW xmm1, xmm2/m128, imm8
 * VEX.128.F3.0F.WIG 70 /r ib      VPSHUFHW xmm1, xmm2/m128, imm8
 * VEX.128.66.0F.WIG 70 /r ib      VPSHUFD xmm1, xmm2/m128, imm8
 * VEX.128.0F.WIG C6 /r ib         VSHUFPS xmm1, xmm2, xmm3/m128, imm8
 * VEX.256.0F.WIG C6 /r ib         VSHUFPS ymm1, ymm2, ymm3/m256, imm8
 * VEX.128.66.0F.WIG C6 /r ib      VSHUFPD xmm1, xmm2, xmm3/m128, imm8
 * VEX.256.66.0F.WIG C6 /r ib      VSHUFPD ymm1, ymm2, ymm3/m256, imm8
 * VEX.128.66.0F3A.WIG 0C /r ib    VBLENDPS xmm1, xmm2, xmm3/m128, imm8
 * VEX.256.66.0F3A.WIG 0C /r ib    VBLENDPS ymm1, ymm2, ymm3/m256, imm8
 * VEX.128.66.0F3A.WIG 0D /r ib    VBLENDPD xmm1, xmm2, xmm3/m128, imm8
 * VEX.256.66.0F3A.WIG 0D /r ib    VBLENDPD ymm1, ymm2, ymm3/m256, imm8
 * VEX.128.66.0F3A.W0 4A /r /is4   VBLENDVPS xmm1, xmm2, xmm3/m128, xmm4
 * VEX.256.66.0F3A.W0 4A /r /is4   VBLENDVPS ymm1, ymm2, ymm3/m256, ymm4
 * VEX.128.66.0F3A.W0 4B /r /is4   VBLENDVPD xmm1, xmm2, xmm3/m128, xmm4
 * VEX.256.66.0F3A.W0 4B /r /is4   VBLENDVPD ymm1, ymm2, ymm3/m256, ymm4
 * VEX.128.66.0F3A.W0 4C /r /is4   VPBLENDVB xmm1, xmm2, xmm3/m128, xmm4
 * VEX.128.66.0F3A.WIG 0E /r ib    VPBLENDW xmm1, xmm2, xmm3/m128, imm8
 * VEX.128.66.0F3A.WIG 21 /r ib    VINSERTPS xmm1, xmm2, xmm3/m32, imm8
 * VEX.128.66.0F3A.W0 20 /r ib     VPINSRB xmm1,xmm2,r32/m8,imm8
 * VEX.128.66.0F.W0 C4 /r ib       VPINSRW xmm1, xmm2, r32/m16, imm8
 * VEX.128.66.0F3A.W0 22 /r ib     VPINSRD xmm1,xmm2,r/m32,imm8
 * VEX.128.66.0F3A.W1 22 /r ib     VPINSRQ xmm1,xmm2,r/m64,imm8
 * VEX.256.66.0F3A.W0 18 /r ib     VINSERTF128 ymm1, ymm2, xmm3/m128, imm8
 * VEX.128.66.0F3A.WIG 17 /r ib    VEXTRACTPS reg/m32, xmm1, imm8
 * VEX.128.66.0F3A.W0 14 /r ib     VPEXTRB r32/m8,xmm2,imm8
 * VEX.128.66.0F3A.W0 15 /r ib     VPEXTRW r32/m16, xmm2, imm8
 * VEX.128.66.0F3A.W0 16 /r ib     VPEXTRD r/m32,xmm2,imm8
 * VEX.128.66.0F3A.W1 16 /r ib     VPEXTRQ r64/m64,xmm2,imm8
 * VEX.128.66.0F.W0 C5 /r ib       VPEXTRW r32, xmm1, imm8
 * VEX.128.66.0F.W1 C5 /r ib       VPEXTRW r64, xmm1, imm8
 * VEX.256.66.0F3A.W0 19 /r ib     VEXTRACTF128 xmm1/m128, ymm2, imm8
 * VEX.128.66.0F38.W0 18 /r        VBROADCASTSS xmm1, m32
 * VEX.256.66.0F38.W0 18 /r        VBROADCASTSS ymm1, m32
 * VEX.256.66.0F38.W0 19 /r        VBROADCASTSD ymm1, m64
 * VEX.256.66.0F38.W0 1A /r        VBROADCASTF128 ymm1, m128
 * VEX.256.66.0F3A.W0 06 /r ib     VPERM2F128 ymm1, ymm2, ymm3/m256, imm8
 * VEX.128.66.0F38.W0 0C /r        VPERMILPS xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F38.W0 0C /r        VPERMILPS ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F3A.W0 04 /r ib     VPERMILPS xmm1, xmm2/m128, imm8
 * VEX.256.66.0F3A.W0 04 /r ib     VPERMILPS ymm1, ymm2/m256, imm8
 * VEX.128.66.0F38.W0 0D /r        VPERMILPD xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F38.W0 0D /r        VPERMILPD ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F3A.W0 05 /r ib     VPERMILPD xmm1, xmm2/m128, imm8
 * VEX.256.66.0F3A.W0 05 /r ib     VPERMILPD ymm1, ymm2/m256, imm8
 * VEX.128.66.0F38.WIG 20 /r       VPMOVSXBW xmm1, xmm2/m64
 * VEX.128.66.0F38.WIG 21 /r       VPMOVSXBD xmm1, xmm2/m32
 * VEX.128.66.0F38.WIG 22 /r       VPMOVSXBQ xmm1, xmm2/m16
 * VEX.128.66.0F38.WIG 23 /r       VPMOVSXWD xmm1, xmm2/m64
 * VEX.128.66.0F38.WIG 24 /r       VPMOVSXWQ xmm1, xmm2/m32
 * VEX.128.66.0F38.WIG 25 /r       VPMOVSXDQ xmm1, xmm2/m64
 * VEX.128.66.0F38.WIG 30 /r       VPMOVZXBW xmm1, xmm2/m64
 * VEX.128.66.0F38.WIG 31 /r       VPMOVZXBD xmm1, xmm2/m32
 * VEX.128.66.0F38.WIG 32 /r       VPMOVZXBQ xmm1, xmm2/m16
 * VEX.128.66.0F38.WIG 33 /r       VPMOVZXWD xmm1, xmm2/m64
 * VEX.128.66.0F38.WIG 34 /r       VPMOVZXWQ xmm1, xmm2/m32
 * VEX.128.66.0F38.WIG 35 /r       VPMOVZXDQ xmm1, xmm2/m64
 * VEX.LIG.F3.0F.W0 2A /r          VCVTSI2SS xmm1,xmm2,r/m32
 * VEX.LIG.F3.0F.W1 2A /r          VCVTSI2SS xmm1,xmm2,r/m64
 * VEX.LIG.F2.0F.W0 2A /r          VCVTSI2SD xmm1,xmm2,r/m32
 * VEX.LIG.F2.0F.W1 2A /r          VCVTSI2SD xmm1,xmm2,r/m64
 * VEX.LIG.F3.0F.W0 2D /r          VCVTSS2SI r32,xmm1/m32
 * VEX.LIG.F3.0F.W1 2D /r          VCVTSS2SI r64,xmm1/m32
 * VEX.LIG.F2.0F.W0 2D /r          VCVTSD2SI r32,xmm1/m64
 * VEX.LIG.F2.0F.W1 2D /r          VCVTSD2SI r64,xmm1/m64
 * VEX.LIG.F3.0F.W0 2C /r          VCVTTSS2SI r32,xmm1/m32
 * VEX.LIG.F3.0F.W1 2C /r          VCVTTSS2SI r64,xmm1/m32
 * VEX.LIG.F2.0F.W0 2C /r          VCVTTSD2SI r32,xmm1/m64
 * VEX.LIG.F2.0F.W1 2C /r          VCVTTSD2SI r64,xmm1/m64
 * VEX.128.F2.0F.WIG E6 /r         VCVTPD2DQ xmm1, xmm2/m128
 * VEX.256.F2.0F.WIG E6 /r         VCVTPD2DQ xmm1, ymm2/m256
 * VEX.128.66.0F.WIG E6 /r         VCVTTPD2DQ xmm1, xmm2/m128
 * VEX.256.66.0F.WIG E6 /r         VCVTTPD2DQ xmm1, ymm2/m256
 * VEX.128.F3.0F.WIG E6 /r         VCVTDQ2PD xmm1, xmm2/m64
 * VEX.256.F3.0F.WIG E6 /r         VCVTDQ2PD ymm1, xmm2/m128
 * VEX.128.0F.WIG 5A /r            VCVTPS2PD xmm1, xmm2/m64
 * VEX.256.0F.WIG 5A /r            VCVTPS2PD ymm1, xmm2/m128
 * VEX.128.66.0F.WIG 5A /r         VCVTPD2PS xmm1, xmm2/m128
 * VEX.256.66.0F.WIG 5A /r         VCVTPD2PS xmm1, ymm2/m256
 * VEX.LIG.F3.0F.WIG 5A /r         VCVTSS2SD xmm1, xmm2, xmm3/m32
 * VEX.LIG.F2.0F.WIG 5A /r         VCVTSD2SS xmm1,xmm2, xmm3/m64
 * VEX.128.0F.WIG 5B /r            VCVTDQ2PS xmm1, xmm2/m128
 * VEX.256.0F.WIG 5B /r            VCVTDQ2PS ymm1, ymm2/m256
 * VEX.128.66.0F.WIG 5B /r         VCVTPS2DQ xmm1, xmm2/m128
 * VEX.256.66.0F.WIG 5B /r         VCVTPS2DQ ymm1, ymm2/m256
 * VEX.128.F3.0F.WIG 5B /r         VCVTTPS2DQ xmm1, xmm2/m128
 * VEX.256.F3.0F.WIG 5B /r         VCVTTPS2DQ ymm1, ymm2/m256
 * VEX.128.66.0F.WIG F7 /r         VMASKMOVDQU xmm1, xmm2
 * VEX.128.66.0F38.W0 2C /r        VMASKMOVPS xmm1, xmm2, m128
 * VEX.128.66.0F38.W0 2E /r        VMASKMOVPS m128, xmm1, xmm2
 * VEX.256.66.0F38.W0 2C /r        VMASKMOVPS ymm1, ymm2, m256
 * VEX.256.66.0F38.W0 2E /r        VMASKMOVPS m256, ymm1, ymm2
 * VEX.128.66.0F38.W0 2D /r        VMASKMOVPD xmm1, xmm2, m128
 * VEX.128.66.0F38.W0 2F /r        VMASKMOVPD m128, xmm1, xmm2
 * VEX.256.66.0F38.W0 2D /r        VMASKMOVPD ymm1, ymm2, m256
 * VEX.256.66.0F38.W0 2F /r        VMASKMOVPD m256, ymm1, ymm2
 * VEX.128.0F.WIG 2B /r            VMOVNTPS m128, xmm1
 * VEX.256.0F.WIG 2B /r            VMOVNTPS m256, ymm1
 * VEX.128.66.0F.WIG 2B /r         VMOVNTPD m128, xmm1
 * VEX.256.66.0F.WIG 2B /r         VMOVNTPD m256, ymm1
 * VEX.128.66.0F.WIG E7 /r         VMOVNTDQ m128, xmm1
 * VEX.256.66.0F.WIG E7 /r         VMOVNTDQ m256, ymm1
 * VEX.128.66.0F38.WIG 2A /r       VMOVNTDQA xmm1, m128
 * VEX.128.0F.WIG 77               VZEROUPPER
 * VEX.256.0F.WIG 77               VZEROALL
 * VEX.128.66.0F.WIG 71 /6 ib      VPSLLW xmm1, xmm2, imm8
 * VEX.128.66.0F.WIG 71 /2 ib      VPSRLW xmm1, xmm2, imm8
 * VEX.128.66.0F.WIG 71 /4 ib      VPSRAW xmm1,xmm2,imm8
 * VEX.128.66.0F.WIG 72 /6 ib      VPSLLD xmm1, xmm2, imm8
 * VEX.128.66.0F.WIG 72 /2 ib      VPSRLD xmm1, xmm2, imm8
 * VEX.128.66.0F.WIG 72 /4 ib      VPSRAD xmm1,xmm2,imm8
 * VEX.128.66.0F.WIG 73 /6 ib      VPSLLQ xmm1, xmm2, imm8
 * VEX.128.66.0F.WIG 73 /7 ib      VPSLLDQ xmm1, xmm2, imm8
 * VEX.128.66.0F.WIG 73 /2 ib      VPSRLQ xmm1, xmm2, imm8
 * VEX.128.66.0F.WIG 73 /3 ib      VPSRLDQ xmm1, xmm2, imm8
 * VEX.LZ.0F.WIG AE /2             VLDMXCSR m32
 * VEX.LZ.0F.WIG AE /3             VSTMXCSR m32
 *
 * AVX2 Instructions
 * ------------------
 * VEX.256.66.0F.W0 D7 /r          VPMOVMSKB r32, ymm1
 * VEX.256.66.0F.W1 D7 /r          VPMOVMSKB r64, ymm1
 * VEX.256.66.0F.WIG FC /r         VPADDB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG FD /r         VPADDW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG FE /r         VPADDD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG D4 /r         VPADDQ ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG EC /r         VPADDSB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG ED /r         VPADDSW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG DC /r         VPADDUSB ymm1,ymm2,ymm3/m256
 * VEX.256.66.0F.WIG DD /r         VPADDUSW ymm1,ymm2,ymm3/m256
 * VEX.256.66.0F38.WIG 01 /r       VPHADDW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 02 /r       VPHADDD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 03 /r       VPHADDSW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG F8 /r         VPSUBB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG F9 /r         VPSUBW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG FA /r         VPSUBD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG FB /r         VPSUBQ ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG E8 /r         VPSUBSB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG E9 /r         VPSUBSW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG D8 /r         VPSUBUSB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG D9 /r         VPSUBUSW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 05 /r       VPHSUBW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 06 /r       VPHSUBD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 07 /r       VPHSUBSW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG D5 /r         VPMULLW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 40 /r       VPMULLD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG E5 /r         VPMULHW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG E4 /r         VPMULHUW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 28 /r       VPMULDQ ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG F4 /r         VPMULUDQ ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 0B /r       VPMULHRSW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG F5 /r         VPMADDWD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 04 /r       VPMADDUBSW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F DA /r             VPMINUB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38 3A /r           VPMINUW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 3B /r       VPMINUD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38 38 /r           VPMINSB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F EA /r             VPMINSW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 39 /r       VPMINSD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F DE /r             VPMAXUB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38 3E /r           VPMAXUW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 3F /r       VPMAXUD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 3C /r       VPMAXSB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG EE /r         VPMAXSW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 3D /r       VPMAXSD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG E0 /r         VPAVGB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG E3 /r         VPAVGW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG F6 /r         VPSADBW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F3A.WIG 42 /r ib    VMPSADBW ymm1, ymm2, ymm3/m256, imm8
 * VEX.256.66.0F38.WIG 1C /r       VPABSB ymm1, ymm2/m256
 * VEX.256.66.0F38.WIG 1D /r       VPABSW ymm1, ymm2/m256
 * VEX.256.66.0F38.WIG 1E /r       VPABSD ymm1, ymm2/m256
 * VEX.256.66.0F38.WIG 08 /r       VPSIGNB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 09 /r       VPSIGNW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 0A /r       VPSIGND ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG 74 /r         VPCMPEQB ymm1,ymm2,ymm3/m256
 * VEX.256.66.0F.WIG 75 /r         VPCMPEQW ymm1,ymm2,ymm3/m256
 * VEX.256.66.0F.WIG 76 /r         VPCMPEQD ymm1,ymm2,ymm3/m256
 * VEX.256.66.0F38.WIG 29 /r       VPCMPEQQ ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG 64 /r         VPCMPGTB ymm1,ymm2,ymm3/m256
 * VEX.256.66.0F.WIG 65 /r         VPCMPGTW ymm1,ymm2,ymm3/m256
 * VEX.256.66.0F.WIG 66 /r         VPCMPGTD ymm1,ymm2,ymm3/m256
 * VEX.256.66.0F38.WIG 37 /r       VPCMPGTQ ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG DB /r         VPAND ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG DF /r         VPANDN ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG EB /r         VPOR ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG EF /r         VPXOR ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG F1 /r         VPSLLW ymm1, ymm2, xmm3/m128
 * VEX.256.66.0F.WIG F2 /r         VPSLLD ymm1, ymm2, xmm3/m128
 * VEX.256.66.0F.WIG F3 /r         VPSLLQ ymm1, ymm2, xmm3/m128
 * VEX.128.66.0F38.W0 47 /r        VPSLLVD xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F38.W0 47 /r        VPSLLVD ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F38.W1 47 /r        VPSLLVQ xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F38.W1 47 /r        VPSLLVQ ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG D1 /r         VPSRLW ymm1, ymm2, xmm3/m128
 * VEX.256.66.0F.WIG D2 /r         VPSRLD ymm1, ymm2, xmm3/m128
 * VEX.256.66.0F.WIG D3 /r         VPSRLQ ymm1, ymm2, xmm3/m128
 * VEX.128.66.0F38.W0 45 /r        VPSRLVD xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F38.W0 45 /r        VPSRLVD ymm1, ymm2, ymm3/m256
 * VEX.128.66.0F38.W1 45 /r        VPSRLVQ xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F38.W1 45 /r        VPSRLVQ ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG E1 /r         VPSRAW ymm1,ymm2,xmm3/m128
 * VEX.256.66.0F.WIG E2 /r         VPSRAD ymm1,ymm2,xmm3/m128
 * VEX.128.66.0F38.W0 46 /r        VPSRAVD xmm1, xmm2, xmm3/m128
 * VEX.256.66.0F38.W0 46 /r        VPSRAVD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F3A.WIG 0F /r ib    VPALIGNR ymm1, ymm2, ymm3/m256, imm8
 * VEX.256.66.0F.WIG 63 /r         VPACKSSWB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG 6B /r         VPACKSSDW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG 67 /r         VPACKUSWB ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38 2B /r           VPACKUSDW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG 68 /r         VPUNPCKHBW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG 69 /r         VPUNPCKHWD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG 6A /r         VPUNPCKHDQ ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG 6D /r         VPUNPCKHQDQ ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG 60 /r         VPUNPCKLBW ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG 61 /r         VPUNPCKLWD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG 62 /r         VPUNPCKLDQ ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F.WIG 6C /r         VPUNPCKLQDQ ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.WIG 00 /r       VPSHUFB ymm1, ymm2, ymm3/m256
 * VEX.256.F2.0F.WIG 70 /r ib      VPSHUFLW ymm1, ymm2/m256, imm8
 * VEX.256.F3.0F.WIG 70 /r ib      VPSHUFHW ymm1, ymm2/m256, imm8
 * VEX.256.66.0F.WIG 70 /r ib      VPSHUFD ymm1, ymm2/m256, imm8
 * VEX.256.66.0F3A.W0 4C /r /is4   VPBLENDVB ymm1, ymm2, ymm3/m256, ymm4
 * VEX.256.66.0F3A.WIG 0E /r ib    VPBLENDW ymm1, ymm2, ymm3/m256, imm8
 * VEX.128.66.0F3A.W0 02 /r ib     VPBLENDD xmm1, xmm2, xmm3/m128, imm8
 * VEX.256.66.0F3A.W0 02 /r ib     VPBLENDD ymm1, ymm2, ymm3/m256, imm8
 * VEX.256.66.0F3A.W0 38 /r ib     VINSERTI128 ymm1, ymm2, xmm3/m128, imm8
 * VEX.256.66.0F3A.W0 39 /r ib     VEXTRACTI128 xmm1/m128, ymm2, imm8
 * VEX.128.66.0F38.W0 78 /r        VPBROADCASTB xmm1,xmm2/m8
 * VEX.256.66.0F38.W0 78 /r        VPBROADCASTB ymm1,xmm2/m8
 * VEX.128.66.0F38.W0 79 /r        VPBROADCASTW xmm1,xmm2/m16
 * VEX.256.66.0F38.W0 79 /r        VPBROADCASTW ymm1,xmm2/m16
 * VEX.128.66.0F38.W0 58 /r        VPBROADCASTD xmm1,xmm2/m32
 * VEX.256.66.0F38.W0 58 /r        VPBROADCASTD ymm1,xmm2/m32
 * VEX.128.66.0F38.W0 59 /r        VPBROADCASTQ xmm1,xmm2/m64
 * VEX.256.66.0F38.W0 59 /r        VPBROADCASTQ ymm1,xmm2/m64
 * VEX.128.66.0F38.W0 18 /r        VBROADCASTSS xmm1, xmm2
 * VEX.256.66.0F38.W0 18 /r        VBROADCASTSS ymm1, xmm2
 * VEX.256.66.0F38.W0 19 /r        VBROADCASTSD ymm1, xmm2
 * VEX.256.66.0F38.W0 5A /r        VBROADCASTI128 ymm1,m128
 * VEX.256.66.0F3A.W0 46 /r ib     VPERM2I128 ymm1, ymm2, ymm3/m256, imm8
 * VEX.256.66.0F38.W0 36 /r        VPERMD ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F38.W0 16 /r        VPERMPS ymm1, ymm2, ymm3/m256
 * VEX.256.66.0F3A.W1 00 /r ib     VPERMQ ymm1, ymm2/m256, imm8
 * VEX.256.66.0F3A.W1 01 /r ib     VPERMPD ymm1, ymm2/m256, imm8
 * VEX.128.66.0F38.W0 92 /r        VGATHERDPS xmm1, vm32x, xmm2
 * VEX.256.66.0F38.W0 92 /r        VGATHERDPS ymm1, vm32y, ymm2
 * VEX.128.66.0F38.W1 92 /r        VGATHERDPD xmm1, vm32x, xmm2
 * VEX.256.66.0F38.W1 92 /r        VGATHERDPD ymm1, vm32x, ymm2
 * VEX.128.66.0F38.W0 93 /r        VGATHERQPS xmm1, vm64x, xmm2
 * VEX.256.66.0F38.W0 93 /r        VGATHERQPS xmm1, vm64y, xmm2
 * VEX.128.66.0F38.W1 93 /r        VGATHERQPD xmm1, vm64x, xmm2
 * VEX.256.66.0F38.W1 93 /r        VGATHERQPD ymm1, vm64y, ymm2
 * VEX.128.66.0F38.W0 90 /r        VPGATHERDD xmm1, vm32x, xmm2
 * VEX.256.66.0F38.W0 90 /r        VPGATHERDD ymm1, vm32y, ymm2
 * VEX.128.66.0F38.W1 90 /r        VPGATHERDQ xmm1, vm32x, xmm2
 * VEX.256.66.0F38.W1 90 /r        VPGATHERDQ ymm1, vm32x, ymm2
 * VEX.128.66.0F38.W0 91 /r        VPGATHERQD xmm1, vm64x, xmm2
 * VEX.256.66.0F38.W0 91 /r        VPGATHERQD xmm1, vm64y, xmm2
 * VEX.128.66.0F38.W1 91 /r        VPGATHERQQ xmm1, vm64x, xmm2
 * VEX.256.66.0F38.W1 91 /r        VPGATHERQQ ymm1, vm64y, ymm2
 * VEX.256.66.0F38.WIG 20 /r       VPMOVSXBW ymm1, xmm2/m128
 * VEX.256.66.0F38.WIG 21 /r       VPMOVSXBD ymm1, xmm2/m64
 * VEX.256.66.0F38.WIG 22 /r       VPMOVSXBQ ymm1, xmm2/m32
 * VEX.256.66.0F38.WIG 23 /r       VPMOVSXWD ymm1, xmm2/m128
 * VEX.256.66.0F38.WIG 24 /r       VPMOVSXWQ ymm1, xmm2/m64
 * VEX.256.66.0F38.WIG 25 /r       VPMOVSXDQ ymm1, xmm2/m128
 * VEX.256.66.0F38.WIG 30 /r       VPMOVZXBW ymm1, xmm2/m128
 * VEX.256.66.0F38.WIG 31 /r       VPMOVZXBD ymm1, xmm2/m64
 * VEX.256.66.0F38.WIG 32 /r       VPMOVZXBQ ymm1, xmm2/m32
 * VEX.256.66.0F38.WIG 33 /r       VPMOVZXWD ymm1, xmm2/m128
 * VEX.256.66.0F38.WIG 34 /r       VPMOVZXWQ ymm1, xmm2/m64
 * VEX.256.66.0F38.WIG 35 /r       VPMOVZXDQ ymm1, xmm2/m128
 * VEX.128.66.0F38.W0 8C /r        VPMASKMOVD xmm1, xmm2, m128
 * VEX.128.66.0F38.W0 8E /r        VPMASKMOVD m128, xmm1, xmm2
 * VEX.256.66.0F38.W0 8C /r        VPMASKMOVD ymm1, ymm2, m256
 * VEX.256.66.0F38.W0 8E /r        VPMASKMOVD m256, ymm1, ymm2
 * VEX.128.66.0F38.W1 8C /r        VPMASKMOVQ xmm1, xmm2, m128
 * VEX.128.66.0F38.W1 8E /r        VPMASKMOVQ m128, xmm1, xmm2
 * VEX.256.66.0F38.W1 8C /r        VPMASKMOVQ ymm1, ymm2, m256
 * VEX.256.66.0F38.W1 8E /r        VPMASKMOVQ m256, ymm1, ymm2
 * VEX.256.66.0F38.WIG 2A /r       VMOVNTDQA ymm1, m256
 * VEX.256.66.0F.WIG 71 /6 ib      VPSLLW ymm1, ymm2, imm8
 * VEX.256.66.0F.WIG 71 /2 ib      VPSRLW ymm1, ymm2, imm8
 * VEX.256.66.0F.WIG 71 /4 ib      VPSRAW ymm1,ymm2,imm8
 * VEX.256.66.0F.WIG 72 /6 ib      VPSLLD ymm1, ymm2, imm8
 * VEX.256.66.0F.WIG 72 /2 ib      VPSRLD ymm1, ymm2, imm8
 * VEX.256.66.0F.WIG 72 /4 ib      VPSRAD ymm1,ymm2,imm8
 * VEX.256.66.0F.WIG 73 /6 ib      VPSLLQ ymm1, ymm2, imm8
 * VEX.256.66.0F.WIG 73 /7 ib      VPSLLDQ ymm1, ymm2, imm8
 * VEX.256.66.0F.WIG 73 /2 ib      VPSRLQ ymm1, ymm2, imm8
 * VEX.256.66.0F.WIG 73 /3 ib      VPSRLDQ ymm1, ymm2, imm8
 */

OPCODE(movd, LEG(NP, 0F, 0, 0x6e), MMX, WR, Pq, Ed)
OPCODE(movd, LEG(NP, 0F, 0, 0x7e), MMX, WR, Ed, Pq)
OPCODE(movd, LEG(66, 0F, 0, 0x6e), SSE2, WR, Vdq, Ed)
OPCODE(movd, LEG(66, 0F, 0, 0x7e), SSE2, WR, Ed, Vdq)
OPCODE(vmovd, VEX(128, 66, 0F, 0, 0x6e), AVX, WR, Vdq, Ed)
OPCODE(vmovd, VEX(128, 66, 0F, 0, 0x7e), AVX, WR, Ed, Vdq)
OPCODE(movq, LEG(NP, 0F, 1, 0x6e), MMX, WR, Pq, Eq)
OPCODE(movq, LEG(NP, 0F, 1, 0x7e), MMX, WR, Eq, Pq)
OPCODE(movq, LEG(66, 0F, 1, 0x6e), SSE2, WR, Vdq, Eq)
OPCODE(movq, LEG(66, 0F, 1, 0x7e), SSE2, WR, Eq, Vdq)
OPCODE(vmovq, VEX(128, 66, 0F, 1, 0x6e), AVX, WR, Vdq, Eq)
OPCODE(vmovq, VEX(128, 66, 0F, 1, 0x7e), AVX, WR, Eq, Vdq)
OPCODE(movq, LEG(NP, 0F, 0, 0x6f), MMX, WR, Pq, Qq)
OPCODE(movq, LEG(NP, 0F, 0, 0x7f), MMX, WR, Qq, Pq)
OPCODE(movq, LEG(F3, 0F, 0, 0x7e), SSE2, WR, Vdq, Wq)
OPCODE(movq, LEG(66, 0F, 0, 0xd6), SSE2, WR, UdqMq, Vq)
OPCODE(vmovq, VEX(128, F3, 0F, IG, 0x7e), AVX, WR, Vdq, Wq)
OPCODE(vmovq, VEX(128, 66, 0F, IG, 0xd6), AVX, WR, UdqMq, Vq)
OPCODE(movaps, LEG(NP, 0F, 0, 0x28), SSE, WR, Vdq, Wdq)
OPCODE(movaps, LEG(NP, 0F, 0, 0x29), SSE, WR, Wdq, Vdq)
OPCODE(vmovaps, VEX(128, NP, 0F, IG, 0x28), AVX, WR, Vdq, Wdq)
OPCODE(vmovaps, VEX(128, NP, 0F, IG, 0x29), AVX, WR, Wdq, Vdq)
OPCODE(vmovaps, VEX(256, NP, 0F, IG, 0x28), AVX, WR, Vqq, Wqq)
OPCODE(vmovaps, VEX(256, NP, 0F, IG, 0x29), AVX, WR, Wqq, Vqq)
OPCODE(movapd, LEG(66, 0F, 0, 0x28), SSE2, WR, Vdq, Wdq)
OPCODE(movapd, LEG(66, 0F, 0, 0x29), SSE2, WR, Wdq, Vdq)
OPCODE(vmovapd, VEX(128, 66, 0F, IG, 0x28), AVX, WR, Vdq, Wdq)
OPCODE(vmovapd, VEX(128, 66, 0F, IG, 0x29), AVX, WR, Wdq, Vdq)
OPCODE(vmovapd, VEX(256, 66, 0F, IG, 0x28), AVX, WR, Vqq, Wqq)
OPCODE(vmovapd, VEX(256, 66, 0F, IG, 0x29), AVX, WR, Wqq, Vqq)
OPCODE(movdqa, LEG(66, 0F, 0, 0x6f), SSE2, WR, Vdq, Wdq)
OPCODE(movdqa, LEG(66, 0F, 0, 0x7f), SSE2, WR, Wdq, Vdq)
OPCODE(vmovdqa, VEX(128, 66, 0F, IG, 0x6f), AVX, WR, Vdq, Wdq)
OPCODE(vmovdqa, VEX(128, 66, 0F, IG, 0x7f), AVX, WR, Wdq, Vdq)
OPCODE(vmovdqa, VEX(256, 66, 0F, IG, 0x6f), AVX, WR, Vqq, Wqq)
OPCODE(vmovdqa, VEX(256, 66, 0F, IG, 0x7f), AVX, WR, Wqq, Vqq)
OPCODE(movups, LEG(NP, 0F, 0, 0x10), SSE, WR, Vdq, Wdq)
OPCODE(movups, LEG(NP, 0F, 0, 0x11), SSE, WR, Wdq, Vdq)
OPCODE(vmovups, VEX(128, NP, 0F, IG, 0x10), AVX, WR, Vdq, Wdq)
OPCODE(vmovups, VEX(128, NP, 0F, IG, 0x11), AVX, WR, Wdq, Vdq)
OPCODE(vmovups, VEX(256, NP, 0F, IG, 0x10), AVX, WR, Vqq, Wqq)
OPCODE(vmovups, VEX(256, NP, 0F, IG, 0x11), AVX, WR, Wqq, Vqq)
OPCODE(movupd, LEG(66, 0F, 0, 0x10), SSE2, WR, Vdq, Wdq)
OPCODE(movupd, LEG(66, 0F, 0, 0x11), SSE2, WR, Wdq, Vdq)
OPCODE(vmovupd, VEX(128, 66, 0F, IG, 0x10), AVX, WR, Vdq, Wdq)
OPCODE(vmovupd, VEX(128, 66, 0F, IG, 0x11), AVX, WR, Wdq, Vdq)
OPCODE(vmovupd, VEX(256, 66, 0F, IG, 0x10), AVX, WR, Vqq, Wqq)
OPCODE(vmovupd, VEX(256, 66, 0F, IG, 0x11), AVX, WR, Wqq, Vqq)
OPCODE(movdqu, LEG(F3, 0F, 0, 0x6f), SSE2, WR, Vdq, Wdq)
OPCODE(movdqu, LEG(F3, 0F, 0, 0x7f), SSE2, WR, Wdq, Vdq)
OPCODE(vmovdqu, VEX(128, F3, 0F, IG, 0x6f), AVX, WR, Vdq, Wdq)
OPCODE(vmovdqu, VEX(128, F3, 0F, IG, 0x7f), AVX, WR, Wdq, Vdq)
OPCODE(vmovdqu, VEX(256, F3, 0F, IG, 0x6f), AVX, WR, Vqq, Wqq)
OPCODE(vmovdqu, VEX(256, F3, 0F, IG, 0x7f), AVX, WR, Wqq, Vqq)
OPCODE(movss, LEG(F3, 0F, 0, 0x10), SSE, WRRR, Vdq, Vdq, Wd, modrm_mod)
OPCODE(movss, LEG(F3, 0F, 0, 0x11), SSE, WR, Wd, Vd)
OPCODE(vmovss, VEX(IG, F3, 0F, IG, 0x10), AVX, WRRRR, Vdq, Hdq, Wd, modrm_mod, vex_v)
OPCODE(vmovss, VEX(IG, F3, 0F, IG, 0x11), AVX, WRRRR, Wdq, Hdq, Vd, modrm_mod, vex_v)
OPCODE(movsd, LEG(F2, 0F, 0, 0x10), SSE2, WRRR, Vdq, Vdq, Wq, modrm_mod)
OPCODE(movsd, LEG(F2, 0F, 0, 0x11), SSE2, WR, Wq, Vq)
OPCODE(vmovsd, VEX(IG, F2, 0F, IG, 0x10), AVX, WRRRR, Vdq, Hdq, Wq, modrm_mod, vex_v)
OPCODE(vmovsd, VEX(IG, F2, 0F, IG, 0x11), AVX, WRRRR, Wdq, Hdq, Vq, modrm_mod, vex_v)
OPCODE(movq2dq, LEG(F3, 0F, 0, 0xd6), SSE2, WR, Vdq, Nq)
OPCODE(movdq2q, LEG(F2, 0F, 0, 0xd6), SSE2, WR, Pq, Uq)
OPCODE(movhlps, LEG(NP, 0F, 0, 0x12), SSE, WRR, Vdq, Vdq, UdqMhq)
OPCODE(vmovhlps, VEX(128, NP, 0F, IG, 0x12), AVX, WRR, Vdq, Hdq, UdqMhq)
OPCODE(movlps, LEG(NP, 0F, 0, 0x13), SSE, WR, Mq, Vq)
OPCODE(vmovlps, VEX(128, NP, 0F, IG, 0x13), AVX, WR, Mq, Vq)
OPCODE(movlpd, LEG(66, 0F, 0, 0x12), SSE2, WRR, Vdq, Vdq, Mq)
OPCODE(movlpd, LEG(66, 0F, 0, 0x13), SSE2, WR, Mq, Vq)
OPCODE(vmovlpd, VEX(128, 66, 0F, IG, 0x12), AVX, WRR, Vdq, Hdq, Mq)
OPCODE(vmovlpd, VEX(128, 66, 0F, IG, 0x13), AVX, WR, Mq, Vq)
OPCODE(movlhps, LEG(NP, 0F, 0, 0x16), SSE, WRR, Vdq, Vq, Wq)
OPCODE(vmovlhps, VEX(128, NP, 0F, IG, 0x16), AVX, WRR, Vdq, Hq, Wq)
OPCODE(movhps, LEG(NP, 0F, 0, 0x17), SSE, WR, Mq, Vdq)
OPCODE(vmovhps, VEX(128, NP, 0F, IG, 0x17), AVX, WR, Mq, Vdq)
OPCODE(movhpd, LEG(66, 0F, 0, 0x16), SSE2, WRR, Vdq, Vq, Mq)
OPCODE(movhpd, LEG(66, 0F, 0, 0x17), SSE2, WR, Mq, Vdq)
OPCODE(vmovhpd, VEX(128, 66, 0F, IG, 0x16), AVX, WRR, Vdq, Hq, Mq)
OPCODE(vmovhpd, VEX(128, 66, 0F, IG, 0x17), AVX, WR, Mq, Vdq)
OPCODE(pmovmskb, LEG(NP, 0F, 0, 0xd7), SSE, WR, Gd, Nq)
OPCODE(pmovmskb, LEG(NP, 0F, 1, 0xd7), SSE, WR, Gq, Nq)
OPCODE(pmovmskb, LEG(66, 0F, 0, 0xd7), SSE2, WR, Gd, Udq)
OPCODE(pmovmskb, LEG(66, 0F, 1, 0xd7), SSE2, WR, Gq, Udq)
OPCODE(vpmovmskb, VEX(128, 66, 0F, 0, 0xd7), AVX, WR, Gd, Udq)
OPCODE(vpmovmskb, VEX(128, 66, 0F, 1, 0xd7), AVX, WR, Gq, Udq)
OPCODE(vpmovmskb, VEX(256, 66, 0F, 0, 0xd7), AVX2, WR, Gd, Uqq)
OPCODE(vpmovmskb, VEX(256, 66, 0F, 1, 0xd7), AVX2, WR, Gq, Uqq)
OPCODE(movmskps, LEG(NP, 0F, 0, 0x50), SSE, WR, Gd, Udq)
OPCODE(movmskps, LEG(NP, 0F, 1, 0x50), SSE, WR, Gq, Udq)
OPCODE(vmovmskps, VEX(128, NP, 0F, 0, 0x50), AVX, WR, Gd, Udq)
OPCODE(vmovmskps, VEX(128, NP, 0F, 1, 0x50), AVX, WR, Gq, Udq)
OPCODE(vmovmskps, VEX(256, NP, 0F, 0, 0x50), AVX, WR, Gd, Uqq)
OPCODE(vmovmskps, VEX(256, NP, 0F, 1, 0x50), AVX, WR, Gq, Uqq)
OPCODE(movmskpd, LEG(66, 0F, 0, 0x50), SSE2, WR, Gd, Udq)
OPCODE(movmskpd, LEG(66, 0F, 1, 0x50), SSE2, WR, Gq, Udq)
OPCODE(vmovmskpd, VEX(128, 66, 0F, 0, 0x50), AVX, WR, Gd, Udq)
OPCODE(vmovmskpd, VEX(128, 66, 0F, 1, 0x50), AVX, WR, Gq, Udq)
OPCODE(vmovmskpd, VEX(256, 66, 0F, 0, 0x50), AVX, WR, Gd, Uqq)
OPCODE(vmovmskpd, VEX(256, 66, 0F, 1, 0x50), AVX, WR, Gq, Uqq)
OPCODE(lddqu, LEG(F2, 0F, 0, 0xf0), SSE3, WR, Vdq, Mdq)
OPCODE(vlddqu, VEX(128, F2, 0F, IG, 0xf0), AVX, WR, Vdq, Mdq)
OPCODE(vlddqu, VEX(256, F2, 0F, IG, 0xf0), AVX, WR, Vqq, Mqq)
OPCODE(movshdup, LEG(F3, 0F, 0, 0x16), SSE3, WR, Vdq, Wdq)
OPCODE(vmovshdup, VEX(128, F3, 0F, IG, 0x16), AVX, WR, Vdq, Wdq)
OPCODE(vmovshdup, VEX(256, F3, 0F, IG, 0x16), AVX, WR, Vqq, Wqq)
OPCODE(movsldup, LEG(F3, 0F, 0, 0x12), SSE3, WR, Vdq, Wdq)
OPCODE(vmovsldup, VEX(128, F3, 0F, IG, 0x12), AVX, WR, Vdq, Wdq)
OPCODE(vmovsldup, VEX(256, F3, 0F, IG, 0x12), AVX, WR, Vqq, Wqq)
OPCODE(movddup, LEG(F2, 0F, 0, 0x12), SSE3, WR, Vdq, Wq)
OPCODE(vmovddup, VEX(128, F2, 0F, IG, 0x12), AVX, WR, Vdq, Wq)
OPCODE(vmovddup, VEX(256, F2, 0F, IG, 0x12), AVX, WR, Vqq, Wqq)
OPCODE(paddb, LEG(NP, 0F, 0, 0xfc), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddb, LEG(66, 0F, 0, 0xfc), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpaddb, VEX(128, 66, 0F, IG, 0xfc), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpaddb, VEX(256, 66, 0F, IG, 0xfc), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(paddw, LEG(NP, 0F, 0, 0xfd), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddw, LEG(66, 0F, 0, 0xfd), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpaddw, VEX(128, 66, 0F, IG, 0xfd), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpaddw, VEX(256, 66, 0F, IG, 0xfd), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(paddd, LEG(NP, 0F, 0, 0xfe), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddd, LEG(66, 0F, 0, 0xfe), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpaddd, VEX(128, 66, 0F, IG, 0xfe), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpaddd, VEX(256, 66, 0F, IG, 0xfe), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(paddq, LEG(NP, 0F, 0, 0xd4), SSE2, WRR, Pq, Pq, Qq)
OPCODE(paddq, LEG(66, 0F, 0, 0xd4), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpaddq, VEX(128, 66, 0F, IG, 0xd4), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpaddq, VEX(256, 66, 0F, IG, 0xd4), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(paddsb, LEG(NP, 0F, 0, 0xec), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddsb, LEG(66, 0F, 0, 0xec), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpaddsb, VEX(128, 66, 0F, IG, 0xec), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpaddsb, VEX(256, 66, 0F, IG, 0xec), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(paddsw, LEG(NP, 0F, 0, 0xed), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddsw, LEG(66, 0F, 0, 0xed), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpaddsw, VEX(128, 66, 0F, IG, 0xed), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpaddsw, VEX(256, 66, 0F, IG, 0xed), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(paddusb, LEG(NP, 0F, 0, 0xdc), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddusb, LEG(66, 0F, 0, 0xdc), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpaddusb, VEX(128, 66, 0F, IG, 0xdc), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpaddusb, VEX(256, 66, 0F, IG, 0xdc), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(paddusw, LEG(NP, 0F, 0, 0xdd), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddusw, LEG(66, 0F, 0, 0xdd), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpaddusw, VEX(128, 66, 0F, IG, 0xdd), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpaddusw, VEX(256, 66, 0F, IG, 0xdd), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(addps, LEG(NP, 0F, 0, 0x58), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(vaddps, VEX(128, NP, 0F, IG, 0x58), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vaddps, VEX(256, NP, 0F, IG, 0x58), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(addpd, LEG(66, 0F, 0, 0x58), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vaddpd, VEX(128, 66, 0F, IG, 0x58), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vaddpd, VEX(256, 66, 0F, IG, 0x58), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(addss, LEG(F3, 0F, 0, 0x58), SSE, WRR, Vd, Vd, Wd)
OPCODE(vaddss, VEX(IG, F3, 0F, IG, 0x58), AVX, WRR, Vd, Hd, Wd)
OPCODE(addsd, LEG(F2, 0F, 0, 0x58), SSE2, WRR, Vq, Vq, Wq)
OPCODE(vaddsd, VEX(IG, F2, 0F, IG, 0x58), AVX, WRR, Vq, Hq, Wq)
OPCODE(phaddw, LEG(NP, 0F38, 0, 0x01), SSSE3, WRR, Pq, Pq, Qq)
OPCODE(phaddw, LEG(66, 0F38, 0, 0x01), SSSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vphaddw, VEX(128, 66, 0F38, IG, 0x01), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vphaddw, VEX(256, 66, 0F38, IG, 0x01), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(phaddd, LEG(NP, 0F38, 0, 0x02), SSSE3, WRR, Pq, Pq, Qq)
OPCODE(phaddd, LEG(66, 0F38, 0, 0x02), SSSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vphaddd, VEX(128, 66, 0F38, IG, 0x02), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vphaddd, VEX(256, 66, 0F38, IG, 0x02), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(phaddsw, LEG(NP, 0F38, 0, 0x03), SSSE3, WRR, Pq, Pq, Qq)
OPCODE(phaddsw, LEG(66, 0F38, 0, 0x03), SSSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vphaddsw, VEX(128, 66, 0F38, IG, 0x03), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vphaddsw, VEX(256, 66, 0F38, IG, 0x03), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(haddps, LEG(F2, 0F, 0, 0x7c), SSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vhaddps, VEX(128, F2, 0F, IG, 0x7c), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vhaddps, VEX(256, F2, 0F, IG, 0x7c), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(haddpd, LEG(66, 0F, 0, 0x7c), SSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vhaddpd, VEX(128, 66, 0F, IG, 0x7c), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vhaddpd, VEX(256, 66, 0F, IG, 0x7c), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(psubb, LEG(NP, 0F, 0, 0xf8), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubb, LEG(66, 0F, 0, 0xf8), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsubb, VEX(128, 66, 0F, IG, 0xf8), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsubb, VEX(256, 66, 0F, IG, 0xf8), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(psubw, LEG(NP, 0F, 0, 0xf9), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubw, LEG(66, 0F, 0, 0xf9), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsubw, VEX(128, 66, 0F, IG, 0xf9), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsubw, VEX(256, 66, 0F, IG, 0xf9), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(psubd, LEG(NP, 0F, 0, 0xfa), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubd, LEG(66, 0F, 0, 0xfa), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsubd, VEX(128, 66, 0F, IG, 0xfa), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsubd, VEX(256, 66, 0F, IG, 0xfa), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(psubq, LEG(NP, 0F, 0, 0xfb), SSE2, WRR, Pq, Pq, Qq)
OPCODE(psubq, LEG(66, 0F, 0, 0xfb), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsubq, VEX(128, 66, 0F, IG, 0xfb), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsubq, VEX(256, 66, 0F, IG, 0xfb), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(psubsb, LEG(NP, 0F, 0, 0xe8), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubsb, LEG(66, 0F, 0, 0xe8), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsubsb, VEX(128, 66, 0F, IG, 0xe8), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsubsb, VEX(256, 66, 0F, IG, 0xe8), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(psubsw, LEG(NP, 0F, 0, 0xe9), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubsw, LEG(66, 0F, 0, 0xe9), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsubsw, VEX(128, 66, 0F, IG, 0xe9), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsubsw, VEX(256, 66, 0F, IG, 0xe9), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(psubusb, LEG(NP, 0F, 0, 0xd8), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubusb, LEG(66, 0F, 0, 0xd8), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsubusb, VEX(128, 66, 0F, IG, 0xd8), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsubusb, VEX(256, 66, 0F, IG, 0xd8), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(psubusw, LEG(NP, 0F, 0, 0xd9), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubusw, LEG(66, 0F, 0, 0xd9), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsubusw, VEX(128, 66, 0F, IG, 0xd9), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsubusw, VEX(256, 66, 0F, IG, 0xd9), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(subps, LEG(NP, 0F, 0, 0x5c), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(vsubps, VEX(128, NP, 0F, IG, 0x5c), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vsubps, VEX(256, NP, 0F, IG, 0x5c), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(subpd, LEG(66, 0F, 0, 0x5c), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vsubpd, VEX(128, 66, 0F, IG, 0x5c), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vsubpd, VEX(256, 66, 0F, IG, 0x5c), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(subss, LEG(F3, 0F, 0, 0x5c), SSE, WRR, Vd, Vd, Wd)
OPCODE(vsubss, VEX(IG, F3, 0F, IG, 0x5c), AVX, WRR, Vd, Hd, Wd)
OPCODE(subsd, LEG(F2, 0F, 0, 0x5c), SSE2, WRR, Vq, Vq, Wq)
OPCODE(vsubsd, VEX(IG, F2, 0F, IG, 0x5c), AVX, WRR, Vq, Hq, Wq)
OPCODE(phsubw, LEG(NP, 0F38, 0, 0x05), SSSE3, WRR, Pq, Pq, Qq)
OPCODE(phsubw, LEG(66, 0F38, 0, 0x05), SSSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vphsubw, VEX(128, 66, 0F38, IG, 0x05), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vphsubw, VEX(256, 66, 0F38, IG, 0x05), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(phsubd, LEG(NP, 0F38, 0, 0x06), SSSE3, WRR, Pq, Pq, Qq)
OPCODE(phsubd, LEG(66, 0F38, 0, 0x06), SSSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vphsubd, VEX(128, 66, 0F38, IG, 0x06), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vphsubd, VEX(256, 66, 0F38, IG, 0x06), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(phsubsw, LEG(NP, 0F38, 0, 0x07), SSSE3, WRR, Pq, Pq, Qq)
OPCODE(phsubsw, LEG(66, 0F38, 0, 0x07), SSSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vphsubsw, VEX(128, 66, 0F38, IG, 0x07), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vphsubsw, VEX(256, 66, 0F38, IG, 0x07), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(hsubps, LEG(F2, 0F, 0, 0x7d), SSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vhsubps, VEX(128, F2, 0F, IG, 0x7d), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vhsubps, VEX(256, F2, 0F, IG, 0x7d), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(hsubpd, LEG(66, 0F, 0, 0x7d), SSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vhsubpd, VEX(128, 66, 0F, IG, 0x7d), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vhsubpd, VEX(256, 66, 0F, IG, 0x7d), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(addsubps, LEG(F2, 0F, 0, 0xd0), SSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vaddsubps, VEX(128, F2, 0F, IG, 0xd0), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vaddsubps, VEX(256, F2, 0F, IG, 0xd0), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(addsubpd, LEG(66, 0F, 0, 0xd0), SSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vaddsubpd, VEX(128, 66, 0F, IG, 0xd0), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vaddsubpd, VEX(256, 66, 0F, IG, 0xd0), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(pmullw, LEG(NP, 0F, 0, 0xd5), MMX, WRR, Pq, Pq, Qq)
OPCODE(pmullw, LEG(66, 0F, 0, 0xd5), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmullw, VEX(128, 66, 0F, IG, 0xd5), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmullw, VEX(256, 66, 0F, IG, 0xd5), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pmulld, LEG(66, 0F38, 0, 0x40), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmulld, VEX(128, 66, 0F38, IG, 0x40), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmulld, VEX(256, 66, 0F38, IG, 0x40), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pmulhw, LEG(NP, 0F, 0, 0xe5), MMX, WRR, Pq, Pq, Qq)
OPCODE(pmulhw, LEG(66, 0F, 0, 0xe5), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmulhw, VEX(128, 66, 0F, IG, 0xe5), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmulhw, VEX(256, 66, 0F, IG, 0xe5), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pmulhuw, LEG(NP, 0F, 0, 0xe4), SSE, WRR, Pq, Pq, Qq)
OPCODE(pmulhuw, LEG(66, 0F, 0, 0xe4), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmulhuw, VEX(128, 66, 0F, IG, 0xe4), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmulhuw, VEX(256, 66, 0F, IG, 0xe4), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pmuldq, LEG(66, 0F38, 0, 0x28), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmuldq, VEX(128, 66, 0F38, IG, 0x28), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmuldq, VEX(256, 66, 0F38, IG, 0x28), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pmuludq, LEG(NP, 0F, 0, 0xf4), SSE2, WRR, Pq, Pq, Qq)
OPCODE(pmuludq, LEG(66, 0F, 0, 0xf4), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmuludq, VEX(128, 66, 0F, IG, 0xf4), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmuludq, VEX(256, 66, 0F, IG, 0xf4), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pmulhrsw, LEG(NP, 0F38, 0, 0x0b), SSSE3, WRR, Pq, Pq, Qq)
OPCODE(pmulhrsw, LEG(66, 0F38, 0, 0x0b), SSSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmulhrsw, VEX(128, 66, 0F38, IG, 0x0b), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmulhrsw, VEX(256, 66, 0F38, IG, 0x0b), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(mulps, LEG(NP, 0F, 0, 0x59), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(vmulps, VEX(128, NP, 0F, IG, 0x59), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vmulps, VEX(256, NP, 0F, IG, 0x59), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(mulpd, LEG(66, 0F, 0, 0x59), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vmulpd, VEX(128, 66, 0F, IG, 0x59), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vmulpd, VEX(256, 66, 0F, IG, 0x59), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(mulss, LEG(F3, 0F, 0, 0x59), SSE, WRR, Vd, Vd, Wd)
OPCODE(vmulss, VEX(IG, F3, 0F, IG, 0x59), AVX, WRR, Vd, Hd, Wd)
OPCODE(mulsd, LEG(F2, 0F, 0, 0x59), SSE2, WRR, Vq, Vq, Wq)
OPCODE(vmulsd, VEX(IG, F2, 0F, IG, 0x59), AVX, WRR, Vq, Hq, Wq)
OPCODE(pmaddwd, LEG(NP, 0F, 0, 0xf5), MMX, WRR, Pq, Pq, Qq)
OPCODE(pmaddwd, LEG(66, 0F, 0, 0xf5), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmaddwd, VEX(128, 66, 0F, IG, 0xf5), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmaddwd, VEX(256, 66, 0F, IG, 0xf5), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pmaddubsw, LEG(NP, 0F38, 0, 0x04), SSSE3, WRR, Pq, Pq, Qq)
OPCODE(pmaddubsw, LEG(66, 0F38, 0, 0x04), SSSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmaddubsw, VEX(128, 66, 0F38, IG, 0x04), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmaddubsw, VEX(256, 66, 0F38, IG, 0x04), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(divps, LEG(NP, 0F, 0, 0x5e), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(vdivps, VEX(128, NP, 0F, IG, 0x5e), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vdivps, VEX(256, NP, 0F, IG, 0x5e), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(divpd, LEG(66, 0F, 0, 0x5e), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vdivpd, VEX(128, 66, 0F, IG, 0x5e), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vdivpd, VEX(256, 66, 0F, IG, 0x5e), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(divss, LEG(F3, 0F, 0, 0x5e), SSE, WRR, Vd, Vd, Wd)
OPCODE(vdivss, VEX(IG, F3, 0F, IG, 0x5e), AVX, WRR, Vd, Hd, Wd)
OPCODE(divsd, LEG(F2, 0F, 0, 0x5e), SSE2, WRR, Vq, Vq, Wq)
OPCODE(vdivsd, VEX(IG, F2, 0F, IG, 0x5e), AVX, WRR, Vq, Hq, Wq)
OPCODE(rcpps, LEG(NP, 0F, 0, 0x53), SSE, WR, Vdq, Wdq)
OPCODE(vrcpps, VEX(128, NP, 0F, IG, 0x53), AVX, WR, Vdq, Wdq)
OPCODE(vrcpps, VEX(256, NP, 0F, IG, 0x53), AVX, WR, Vqq, Wqq)
OPCODE(rcpss, LEG(F3, 0F, 0, 0x53), SSE, WR, Vd, Wd)
OPCODE(vrcpss, VEX(IG, F3, 0F, IG, 0x53), AVX, WRR, Vd, Hd, Wd)
OPCODE(sqrtps, LEG(NP, 0F, 0, 0x51), SSE, WR, Vdq, Wdq)
OPCODE(vsqrtps, VEX(128, NP, 0F, IG, 0x51), AVX, WR, Vdq, Wdq)
OPCODE(vsqrtps, VEX(256, NP, 0F, IG, 0x51), AVX, WR, Vqq, Wqq)
OPCODE(sqrtpd, LEG(66, 0F, 0, 0x51), SSE2, WR, Vdq, Wdq)
OPCODE(vsqrtpd, VEX(128, 66, 0F, IG, 0x51), AVX, WR, Vdq, Wdq)
OPCODE(vsqrtpd, VEX(256, 66, 0F, IG, 0x51), AVX, WR, Vqq, Wqq)
OPCODE(sqrtss, LEG(F3, 0F, 0, 0x51), SSE, WR, Vd, Wd)
OPCODE(vsqrtss, VEX(IG, F3, 0F, IG, 0x51), AVX, WRR, Vd, Hd, Wd)
OPCODE(sqrtsd, LEG(F2, 0F, 0, 0x51), SSE2, WR, Vq, Wq)
OPCODE(vsqrtsd, VEX(IG, F2, 0F, IG, 0x51), AVX, WRR, Vq, Hq, Wq)
OPCODE(rsqrtps, LEG(NP, 0F, 0, 0x52), SSE, WR, Vdq, Wdq)
OPCODE(vrsqrtps, VEX(128, NP, 0F, IG, 0x52), AVX, WR, Vdq, Wdq)
OPCODE(vrsqrtps, VEX(256, NP, 0F, IG, 0x52), AVX, WR, Vqq, Wqq)
OPCODE(rsqrtss, LEG(F3, 0F, 0, 0x52), SSE, WR, Vd, Wd)
OPCODE(vrsqrtss, VEX(IG, F3, 0F, IG, 0x52), AVX, WRR, Vd, Hd, Wd)
OPCODE(pminub, LEG(NP, 0F, 0, 0xda), SSE, WRR, Pq, Pq, Qq)
OPCODE(pminub, LEG(66, 0F, 0, 0xda), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpminub, VEX(128, 66, 0F, IG, 0xda), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpminub, VEX(256, 66, 0F, IG, 0xda), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pminuw, LEG(66, 0F38, 0, 0x3a), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpminuw, VEX(128, 66, 0F38, IG, 0x3a), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpminuw, VEX(256, 66, 0F38, IG, 0x3a), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pminud, LEG(66, 0F38, 0, 0x3b), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpminud, VEX(128, 66, 0F38, IG, 0x3b), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpminud, VEX(256, 66, 0F38, IG, 0x3b), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pminsb, LEG(66, 0F38, 0, 0x38), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpminsb, VEX(128, 66, 0F38, IG, 0x38), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpminsb, VEX(256, 66, 0F38, IG, 0x38), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pminsw, LEG(NP, 0F, 0, 0xea), SSE, WRR, Pq, Pq, Qq)
OPCODE(pminsw, LEG(66, 0F, 0, 0xea), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpminsw, VEX(128, 66, 0F, IG, 0xea), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpminsw, VEX(256, 66, 0F, IG, 0xea), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pminsd, LEG(66, 0F38, 0, 0x39), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpminsd, VEX(128, 66, 0F38, IG, 0x39), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpminsd, VEX(256, 66, 0F38, IG, 0x39), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(minps, LEG(NP, 0F, 0, 0x5d), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(vminps, VEX(128, NP, 0F, IG, 0x5d), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vminps, VEX(256, NP, 0F, IG, 0x5d), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(minpd, LEG(66, 0F, 0, 0x5d), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vminpd, VEX(128, 66, 0F, IG, 0x5d), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vminpd, VEX(256, 66, 0F, IG, 0x5d), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(minss, LEG(F3, 0F, 0, 0x5d), SSE, WRR, Vd, Vd, Wd)
OPCODE(vminss, VEX(IG, F3, 0F, IG, 0x5d), AVX, WRR, Vd, Hd, Wd)
OPCODE(minsd, LEG(F2, 0F, 0, 0x5d), SSE2, WRR, Vq, Vq, Wq)
OPCODE(vminsd, VEX(IG, F2, 0F, IG, 0x5d), AVX, WRR, Vq, Hq, Wq)
OPCODE(phminposuw, LEG(66, 0F38, 0, 0x41), SSE4_1, WR, Vdq, Wdq)
OPCODE(vphminposuw, VEX(128, 66, 0F38, IG, 0x41), AVX, WR, Vdq, Wdq)
OPCODE(pmaxub, LEG(NP, 0F, 0, 0xde), SSE, WRR, Pq, Pq, Qq)
OPCODE(pmaxub, LEG(66, 0F, 0, 0xde), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmaxub, VEX(128, 66, 0F, IG, 0xde), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmaxub, VEX(256, 66, 0F, IG, 0xde), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pmaxuw, LEG(66, 0F38, 0, 0x3e), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmaxuw, VEX(128, 66, 0F38, IG, 0x3e), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmaxuw, VEX(256, 66, 0F38, IG, 0x3e), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pmaxud, LEG(66, 0F38, 0, 0x3f), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmaxud, VEX(128, 66, 0F38, IG, 0x3f), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmaxud, VEX(256, 66, 0F38, IG, 0x3f), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pmaxsb, LEG(66, 0F38, 0, 0x3c), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmaxsb, VEX(128, 66, 0F38, IG, 0x3c), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmaxsb, VEX(256, 66, 0F38, IG, 0x3c), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pmaxsw, LEG(NP, 0F, 0, 0xee), SSE, WRR, Pq, Pq, Qq)
OPCODE(pmaxsw, LEG(66, 0F, 0, 0xee), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmaxsw, VEX(128, 66, 0F, IG, 0xee), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmaxsw, VEX(256, 66, 0F, IG, 0xee), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pmaxsd, LEG(66, 0F38, 0, 0x3d), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpmaxsd, VEX(128, 66, 0F38, IG, 0x3d), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpmaxsd, VEX(256, 66, 0F38, IG, 0x3d), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(maxps, LEG(NP, 0F, 0, 0x5f), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(vmaxps, VEX(128, NP, 0F, IG, 0x5f), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vmaxps, VEX(256, NP, 0F, IG, 0x5f), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(maxpd, LEG(66, 0F, 0, 0x5f), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vmaxpd, VEX(128, 66, 0F, IG, 0x5f), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vmaxpd, VEX(256, 66, 0F, IG, 0x5f), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(maxss, LEG(F3, 0F, 0, 0x5f), SSE, WRR, Vd, Vd, Wd)
OPCODE(vmaxss, VEX(IG, F3, 0F, IG, 0x5f), AVX, WRR, Vd, Hd, Wd)
OPCODE(maxsd, LEG(F2, 0F, 0, 0x5f), SSE2, WRR, Vq, Vq, Wq)
OPCODE(vmaxsd, VEX(IG, F2, 0F, IG, 0x5f), AVX, WRR, Vq, Hq, Wq)
OPCODE(pavgb, LEG(NP, 0F, 0, 0xe0), SSE, WRR, Pq, Pq, Qq)
OPCODE(pavgb, LEG(66, 0F, 0, 0xe0), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpavgb, VEX(128, 66, 0F, IG, 0xe0), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpavgb, VEX(256, 66, 0F, IG, 0xe0), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pavgw, LEG(NP, 0F, 0, 0xe3), SSE, WRR, Pq, Pq, Qq)
OPCODE(pavgw, LEG(66, 0F, 0, 0xe3), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpavgw, VEX(128, 66, 0F, IG, 0xe3), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpavgw, VEX(256, 66, 0F, IG, 0xe3), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(psadbw, LEG(NP, 0F, 0, 0xf6), SSE, WRR, Pq, Pq, Qq)
OPCODE(psadbw, LEG(66, 0F, 0, 0xf6), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsadbw, VEX(128, 66, 0F, IG, 0xf6), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsadbw, VEX(256, 66, 0F, IG, 0xf6), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(mpsadbw, LEG(66, 0F3A, 0, 0x42), SSE4_1, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(vmpsadbw, VEX(128, 66, 0F3A, IG, 0x42), AVX, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(vmpsadbw, VEX(256, 66, 0F3A, IG, 0x42), AVX2, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(pabsb, LEG(NP, 0F38, 0, 0x1c), SSSE3, WR, Pq, Qq)
OPCODE(pabsb, LEG(66, 0F38, 0, 0x1c), SSSE3, WR, Vdq, Wdq)
OPCODE(vpabsb, VEX(128, 66, 0F38, IG, 0x1c), AVX, WR, Vdq, Wdq)
OPCODE(vpabsb, VEX(256, 66, 0F38, IG, 0x1c), AVX2, WR, Vqq, Wqq)
OPCODE(pabsw, LEG(NP, 0F38, 0, 0x1d), SSSE3, WR, Pq, Qq)
OPCODE(pabsw, LEG(66, 0F38, 0, 0x1d), SSSE3, WR, Vdq, Wdq)
OPCODE(vpabsw, VEX(128, 66, 0F38, IG, 0x1d), AVX, WR, Vdq, Wdq)
OPCODE(vpabsw, VEX(256, 66, 0F38, IG, 0x1d), AVX2, WR, Vqq, Wqq)
OPCODE(pabsd, LEG(NP, 0F38, 0, 0x1e), SSSE3, WR, Pq, Qq)
OPCODE(pabsd, LEG(66, 0F38, 0, 0x1e), SSSE3, WR, Vdq, Wdq)
OPCODE(vpabsd, VEX(128, 66, 0F38, IG, 0x1e), AVX, WR, Vdq, Wdq)
OPCODE(vpabsd, VEX(256, 66, 0F38, IG, 0x1e), AVX2, WR, Vqq, Wqq)
OPCODE(psignb, LEG(NP, 0F38, 0, 0x08), SSSE3, WRR, Pq, Pq, Qq)
OPCODE(psignb, LEG(66, 0F38, 0, 0x08), SSSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsignb, VEX(128, 66, 0F38, IG, 0x08), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsignb, VEX(256, 66, 0F38, IG, 0x08), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(psignw, LEG(NP, 0F38, 0, 0x09), SSSE3, WRR, Pq, Pq, Qq)
OPCODE(psignw, LEG(66, 0F38, 0, 0x09), SSSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsignw, VEX(128, 66, 0F38, IG, 0x09), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsignw, VEX(256, 66, 0F38, IG, 0x09), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(psignd, LEG(NP, 0F38, 0, 0x0a), SSSE3, WRR, Pq, Pq, Qq)
OPCODE(psignd, LEG(66, 0F38, 0, 0x0a), SSSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsignd, VEX(128, 66, 0F38, IG, 0x0a), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsignd, VEX(256, 66, 0F38, IG, 0x0a), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(dpps, LEG(66, 0F3A, 0, 0x40), SSE4_1, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(vdpps, VEX(128, 66, 0F3A, IG, 0x40), AVX, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(vdpps, VEX(256, 66, 0F3A, IG, 0x40), AVX, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(dppd, LEG(66, 0F3A, 0, 0x41), SSE4_1, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(vdppd, VEX(128, 66, 0F3A, IG, 0x41), AVX, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(roundps, LEG(66, 0F3A, 0, 0x08), SSE4_1, WRR, Vdq, Wdq, Ib)
OPCODE(vroundps, VEX(128, 66, 0F3A, IG, 0x08), AVX, WRR, Vdq, Wdq, Ib)
OPCODE(vroundps, VEX(256, 66, 0F3A, IG, 0x08), AVX, WRR, Vqq, Wqq, Ib)
OPCODE(roundpd, LEG(66, 0F3A, 0, 0x09), SSE4_1, WRR, Vdq, Wdq, Ib)
OPCODE(vroundpd, VEX(128, 66, 0F3A, IG, 0x09), AVX, WRR, Vdq, Wdq, Ib)
OPCODE(vroundpd, VEX(256, 66, 0F3A, IG, 0x09), AVX, WRR, Vqq, Wqq, Ib)
OPCODE(roundss, LEG(66, 0F3A, 0, 0x0a), SSE4_1, WRR, Vd, Wd, Ib)
OPCODE(vroundss, VEX(IG, 66, 0F3A, IG, 0x0a), AVX, WRRR, Vd, Hd, Wd, Ib)
OPCODE(roundsd, LEG(66, 0F3A, 0, 0x0b), SSE4_1, WRR, Vq, Wq, Ib)
OPCODE(vroundsd, VEX(IG, 66, 0F3A, IG, 0x0b), AVX, WRRR, Vq, Hq, Wq, Ib)
OPCODE(aesdec, LEG(66, 0F38, 0, 0xde), AES, WRR, Vdq, Vdq, Wdq)
OPCODE(vaesdec, VEX(128, 66, 0F38, IG, 0xde), AES_AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(aesdeclast, LEG(66, 0F38, 0, 0xdf), AES, WRR, Vdq, Vdq, Wdq)
OPCODE(vaesdeclast, VEX(128, 66, 0F38, IG, 0xdf), AES_AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(aesenc, LEG(66, 0F38, 0, 0xdc), AES, WRR, Vdq, Vdq, Wdq)
OPCODE(vaesenc, VEX(128, 66, 0F38, IG, 0xdc), AES_AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(aesenclast, LEG(66, 0F38, 0, 0xdd), AES, WRR, Vdq, Vdq, Wdq)
OPCODE(vaesenclast, VEX(128, 66, 0F38, IG, 0xdd), AES_AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(aesimc, LEG(66, 0F38, 0, 0xdb), AES, WR, Vdq, Wdq)
OPCODE(vaesimc, VEX(128, 66, 0F38, IG, 0xdb), AES_AVX, WR, Vdq, Wdq)
OPCODE(aeskeygenassist, LEG(66, 0F3A, 0, 0xdf), AES, WRR, Vdq, Wdq, Ib)
OPCODE(vaeskeygenassist, VEX(128, 66, 0F3A, IG, 0xdf), AES_AVX, WRR, Vdq, Wdq, Ib)
OPCODE(pclmulqdq, LEG(66, 0F3A, 0, 0x44), PCLMULQDQ, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(vpclmulqdq, VEX(128, 66, 0F3A, IG, 0x44), PCLMULQDQ_AVX, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(pcmpeqb, LEG(NP, 0F, 0, 0x74), MMX, WRR, Pq, Pq, Qq)
OPCODE(pcmpeqb, LEG(66, 0F, 0, 0x74), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpcmpeqb, VEX(128, 66, 0F, IG, 0x74), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpcmpeqb, VEX(256, 66, 0F, IG, 0x74), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pcmpeqw, LEG(NP, 0F, 0, 0x75), MMX, WRR, Pq, Pq, Qq)
OPCODE(pcmpeqw, LEG(66, 0F, 0, 0x75), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpcmpeqw, VEX(128, 66, 0F, IG, 0x75), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpcmpeqw, VEX(256, 66, 0F, IG, 0x75), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pcmpeqd, LEG(NP, 0F, 0, 0x76), MMX, WRR, Pq, Pq, Qq)
OPCODE(pcmpeqd, LEG(66, 0F, 0, 0x76), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpcmpeqd, VEX(128, 66, 0F, IG, 0x76), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpcmpeqd, VEX(256, 66, 0F, IG, 0x76), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pcmpeqq, LEG(66, 0F38, 0, 0x29), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpcmpeqq, VEX(128, 66, 0F38, IG, 0x29), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpcmpeqq, VEX(256, 66, 0F38, IG, 0x29), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pcmpgtb, LEG(NP, 0F, 0, 0x64), MMX, WRR, Pq, Pq, Qq)
OPCODE(pcmpgtb, LEG(66, 0F, 0, 0x64), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpcmpgtb, VEX(128, 66, 0F, IG, 0x64), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpcmpgtb, VEX(256, 66, 0F, IG, 0x64), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pcmpgtw, LEG(NP, 0F, 0, 0x65), MMX, WRR, Pq, Pq, Qq)
OPCODE(pcmpgtw, LEG(66, 0F, 0, 0x65), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpcmpgtw, VEX(128, 66, 0F, IG, 0x65), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpcmpgtw, VEX(256, 66, 0F, IG, 0x65), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pcmpgtd, LEG(NP, 0F, 0, 0x66), MMX, WRR, Pq, Pq, Qq)
OPCODE(pcmpgtd, LEG(66, 0F, 0, 0x66), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpcmpgtd, VEX(128, 66, 0F, IG, 0x66), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpcmpgtd, VEX(256, 66, 0F, IG, 0x66), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pcmpgtq, LEG(66, 0F38, 0, 0x37), SSE4_2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpcmpgtq, VEX(128, 66, 0F38, IG, 0x37), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpcmpgtq, VEX(256, 66, 0F38, IG, 0x37), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pcmpestrm, LEG(66, 0F3A, 0, 0x60), SSE4_2, RRR, Vdq, Wdq, Ib)
OPCODE(vpcmpestrm, VEX(128, 66, 0F3A, IG, 0x60), AVX, RRR, Vdq, Wdq, Ib)
OPCODE(pcmpestri, LEG(66, 0F3A, 0, 0x61), SSE4_2, RRR, Vdq, Wdq, Ib)
OPCODE(vpcmpestri, VEX(128, 66, 0F3A, IG, 0x61), AVX, RRR, Vdq, Wdq, Ib)
OPCODE(pcmpistrm, LEG(66, 0F3A, 0, 0x62), SSE4_2, RRR, Vdq, Wdq, Ib)
OPCODE(vpcmpistrm, VEX(128, 66, 0F3A, IG, 0x62), AVX, RRR, Vdq, Wdq, Ib)
OPCODE(pcmpistri, LEG(66, 0F3A, 0, 0x63), SSE4_2, RRR, Vdq, Wdq, Ib)
OPCODE(vpcmpistri, VEX(128, 66, 0F3A, IG, 0x63), AVX, RRR, Vdq, Wdq, Ib)
OPCODE(ptest, LEG(66, 0F38, 0, 0x17), SSE4_1, RR, Vdq, Wdq)
OPCODE(vptest, VEX(128, 66, 0F38, IG, 0x17), AVX, RR, Vdq, Wdq)
OPCODE(vptest, VEX(256, 66, 0F38, IG, 0x17), AVX, RR, Vqq, Wqq)
OPCODE(vtestps, VEX(128, 66, 0F38, 0, 0x0e), AVX, RR, Vdq, Wdq)
OPCODE(vtestps, VEX(256, 66, 0F38, 0, 0x0e), AVX, RR, Vqq, Wqq)
OPCODE(vtestpd, VEX(128, 66, 0F38, 0, 0x0f), AVX, RR, Vdq, Wdq)
OPCODE(vtestpd, VEX(256, 66, 0F38, 0, 0x0f), AVX, RR, Vqq, Wqq)
OPCODE(cmpps, LEG(NP, 0F, 0, 0xc2), SSE, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(vcmpps, VEX(128, NP, 0F, IG, 0xc2), AVX, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(vcmpps, VEX(256, NP, 0F, IG, 0xc2), AVX, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(cmppd, LEG(66, 0F, 0, 0xc2), SSE2, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(vcmppd, VEX(128, 66, 0F, IG, 0xc2), AVX, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(vcmppd, VEX(256, 66, 0F, IG, 0xc2), AVX, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(cmpss, LEG(F3, 0F, 0, 0xc2), SSE, WRRR, Vd, Vd, Wd, Ib)
OPCODE(vcmpss, VEX(IG, F3, 0F, IG, 0xc2), AVX, WRRR, Vd, Hd, Wd, Ib)
OPCODE(cmpsd, LEG(F2, 0F, 0, 0xc2), SSE2, WRRR, Vq, Vq, Wq, Ib)
OPCODE(vcmpsd, VEX(IG, F2, 0F, IG, 0xc2), AVX, WRRR, Vq, Hq, Wq, Ib)
OPCODE(ucomiss, LEG(NP, 0F, 0, 0x2e), SSE, RR, Vd, Wd)
OPCODE(vucomiss, VEX(IG, NP, 0F, IG, 0x2e), AVX, RR, Vd, Wd)
OPCODE(ucomisd, LEG(66, 0F, 0, 0x2e), SSE2, RR, Vq, Wq)
OPCODE(vucomisd, VEX(IG, 66, 0F, IG, 0x2e), AVX, RR, Vq, Wq)
OPCODE(comiss, LEG(NP, 0F, 0, 0x2f), SSE, RR, Vd, Wd)
OPCODE(vcomiss, VEX(IG, NP, 0F, IG, 0x2f), AVX, RR, Vd, Wd)
OPCODE(comisd, LEG(66, 0F, 0, 0x2f), SSE2, RR, Vq, Wq)
OPCODE(vcomisd, VEX(IG, 66, 0F, IG, 0x2f), AVX, RR, Vq, Wq)
OPCODE(pand, LEG(NP, 0F, 0, 0xdb), MMX, WRR, Pq, Pq, Qq)
OPCODE(pand, LEG(66, 0F, 0, 0xdb), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpand, VEX(128, 66, 0F, IG, 0xdb), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpand, VEX(256, 66, 0F, IG, 0xdb), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(andps, LEG(NP, 0F, 0, 0x54), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(vandps, VEX(128, NP, 0F, IG, 0x54), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vandps, VEX(256, NP, 0F, IG, 0x54), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(andpd, LEG(66, 0F, 0, 0x54), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vandpd, VEX(128, 66, 0F, IG, 0x54), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vandpd, VEX(256, 66, 0F, IG, 0x54), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(pandn, LEG(NP, 0F, 0, 0xdf), MMX, WRR, Pq, Pq, Qq)
OPCODE(pandn, LEG(66, 0F, 0, 0xdf), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpandn, VEX(128, 66, 0F, IG, 0xdf), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpandn, VEX(256, 66, 0F, IG, 0xdf), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(andnps, LEG(NP, 0F, 0, 0x55), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(vandnps, VEX(128, NP, 0F, IG, 0x55), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vandnps, VEX(256, NP, 0F, IG, 0x55), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(andnpd, LEG(66, 0F, 0, 0x55), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vandnpd, VEX(128, 66, 0F, IG, 0x55), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vandnpd, VEX(256, 66, 0F, IG, 0x55), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(por, LEG(NP, 0F, 0, 0xeb), MMX, WRR, Pq, Pq, Qq)
OPCODE(por, LEG(66, 0F, 0, 0xeb), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpor, VEX(128, 66, 0F, IG, 0xeb), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpor, VEX(256, 66, 0F, IG, 0xeb), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(orps, LEG(NP, 0F, 0, 0x56), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(vorps, VEX(128, NP, 0F, IG, 0x56), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vorps, VEX(256, NP, 0F, IG, 0x56), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(orpd, LEG(66, 0F, 0, 0x56), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vorpd, VEX(128, 66, 0F, IG, 0x56), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vorpd, VEX(256, 66, 0F, IG, 0x56), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(pxor, LEG(NP, 0F, 0, 0xef), MMX, WRR, Pq, Pq, Qq)
OPCODE(pxor, LEG(66, 0F, 0, 0xef), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpxor, VEX(128, 66, 0F, IG, 0xef), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpxor, VEX(256, 66, 0F, IG, 0xef), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(xorps, LEG(NP, 0F, 0, 0x57), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(vxorps, VEX(128, NP, 0F, IG, 0x57), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vxorps, VEX(256, NP, 0F, IG, 0x57), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(xorpd, LEG(66, 0F, 0, 0x57), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vxorpd, VEX(128, 66, 0F, IG, 0x57), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vxorpd, VEX(256, 66, 0F, IG, 0x57), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(psllw, LEG(NP, 0F, 0, 0xf1), MMX, WRR, Pq, Pq, Qq)
OPCODE(psllw, LEG(66, 0F, 0, 0xf1), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsllw, VEX(128, 66, 0F, IG, 0xf1), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsllw, VEX(256, 66, 0F, IG, 0xf1), AVX2, WRR, Vqq, Hqq, Wdq)
OPCODE(pslld, LEG(NP, 0F, 0, 0xf2), MMX, WRR, Pq, Pq, Qq)
OPCODE(pslld, LEG(66, 0F, 0, 0xf2), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpslld, VEX(128, 66, 0F, IG, 0xf2), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpslld, VEX(256, 66, 0F, IG, 0xf2), AVX2, WRR, Vqq, Hqq, Wdq)
OPCODE(psllq, LEG(NP, 0F, 0, 0xf3), MMX, WRR, Pq, Pq, Qq)
OPCODE(psllq, LEG(66, 0F, 0, 0xf3), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsllq, VEX(128, 66, 0F, IG, 0xf3), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsllq, VEX(256, 66, 0F, IG, 0xf3), AVX2, WRR, Vqq, Hqq, Wdq)
OPCODE(vpsllvd, VEX(128, 66, 0F38, 0, 0x47), AVX2, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsllvd, VEX(256, 66, 0F38, 0, 0x47), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(vpsllvq, VEX(128, 66, 0F38, 1, 0x47), AVX2, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsllvq, VEX(256, 66, 0F38, 1, 0x47), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(psrlw, LEG(NP, 0F, 0, 0xd1), MMX, WRR, Pq, Pq, Qq)
OPCODE(psrlw, LEG(66, 0F, 0, 0xd1), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsrlw, VEX(128, 66, 0F, IG, 0xd1), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsrlw, VEX(256, 66, 0F, IG, 0xd1), AVX2, WRR, Vqq, Hqq, Wdq)
OPCODE(psrld, LEG(NP, 0F, 0, 0xd2), MMX, WRR, Pq, Pq, Qq)
OPCODE(psrld, LEG(66, 0F, 0, 0xd2), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsrld, VEX(128, 66, 0F, IG, 0xd2), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsrld, VEX(256, 66, 0F, IG, 0xd2), AVX2, WRR, Vqq, Hqq, Wdq)
OPCODE(psrlq, LEG(NP, 0F, 0, 0xd3), MMX, WRR, Pq, Pq, Qq)
OPCODE(psrlq, LEG(66, 0F, 0, 0xd3), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsrlq, VEX(128, 66, 0F, IG, 0xd3), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsrlq, VEX(256, 66, 0F, IG, 0xd3), AVX2, WRR, Vqq, Hqq, Wdq)
OPCODE(vpsrlvd, VEX(128, 66, 0F38, 0, 0x45), AVX2, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsrlvd, VEX(256, 66, 0F38, 0, 0x45), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(vpsrlvq, VEX(128, 66, 0F38, 1, 0x45), AVX2, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsrlvq, VEX(256, 66, 0F38, 1, 0x45), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(psraw, LEG(NP, 0F, 0, 0xe1), MMX, WRR, Pq, Pq, Qq)
OPCODE(psraw, LEG(66, 0F, 0, 0xe1), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsraw, VEX(128, 66, 0F, IG, 0xe1), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsraw, VEX(256, 66, 0F, IG, 0xe1), AVX2, WRR, Vqq, Hqq, Wdq)
OPCODE(psrad, LEG(NP, 0F, 0, 0xe2), MMX, WRR, Pq, Pq, Qq)
OPCODE(psrad, LEG(66, 0F, 0, 0xe2), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpsrad, VEX(128, 66, 0F, IG, 0xe2), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsrad, VEX(256, 66, 0F, IG, 0xe2), AVX2, WRR, Vqq, Hqq, Wdq)
OPCODE(vpsravd, VEX(128, 66, 0F38, 0, 0x46), AVX2, WRR, Vdq, Hdq, Wdq)
OPCODE(vpsravd, VEX(256, 66, 0F38, 0, 0x46), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(palignr, LEG(NP, 0F3A, 0, 0x0f), SSSE3, WRRR, Pq, Pq, Qq, Ib)
OPCODE(palignr, LEG(66, 0F3A, 0, 0x0f), SSSE3, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(vpalignr, VEX(128, 66, 0F3A, IG, 0x0f), AVX, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(vpalignr, VEX(256, 66, 0F3A, IG, 0x0f), AVX2, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(packsswb, LEG(NP, 0F, 0, 0x63), MMX, WRR, Pq, Pq, Qq)
OPCODE(packsswb, LEG(66, 0F, 0, 0x63), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpacksswb, VEX(128, 66, 0F, IG, 0x63), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpacksswb, VEX(256, 66, 0F, IG, 0x63), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(packssdw, LEG(NP, 0F, 0, 0x6b), MMX, WRR, Pq, Pq, Qq)
OPCODE(packssdw, LEG(66, 0F, 0, 0x6b), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpackssdw, VEX(128, 66, 0F, IG, 0x6b), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpackssdw, VEX(256, 66, 0F, IG, 0x6b), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(packuswb, LEG(NP, 0F, 0, 0x67), MMX, WRR, Pq, Pq, Qq)
OPCODE(packuswb, LEG(66, 0F, 0, 0x67), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpackuswb, VEX(128, 66, 0F, IG, 0x67), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpackuswb, VEX(256, 66, 0F, IG, 0x67), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(packusdw, LEG(66, 0F38, 0, 0x2b), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpackusdw, VEX(128, 66, 0F38, IG, 0x2b), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpackusdw, VEX(256, 66, 0F38, IG, 0x2b), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(punpckhbw, LEG(NP, 0F, 0, 0x68), MMX, WRR, Pq, Pq, Qq)
OPCODE(punpckhbw, LEG(66, 0F, 0, 0x68), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpunpckhbw, VEX(128, 66, 0F, IG, 0x68), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpunpckhbw, VEX(256, 66, 0F, IG, 0x68), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(punpckhwd, LEG(NP, 0F, 0, 0x69), MMX, WRR, Pq, Pq, Qq)
OPCODE(punpckhwd, LEG(66, 0F, 0, 0x69), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpunpckhwd, VEX(128, 66, 0F, IG, 0x69), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpunpckhwd, VEX(256, 66, 0F, IG, 0x69), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(punpckhdq, LEG(NP, 0F, 0, 0x6a), MMX, WRR, Pq, Pq, Qq)
OPCODE(punpckhdq, LEG(66, 0F, 0, 0x6a), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpunpckhdq, VEX(128, 66, 0F, IG, 0x6a), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpunpckhdq, VEX(256, 66, 0F, IG, 0x6a), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(punpckhqdq, LEG(66, 0F, 0, 0x6d), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpunpckhqdq, VEX(128, 66, 0F, IG, 0x6d), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpunpckhqdq, VEX(256, 66, 0F, IG, 0x6d), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(punpcklbw, LEG(NP, 0F, 0, 0x60), MMX, WRR, Pq, Pq, Qd)
OPCODE(punpcklbw, LEG(66, 0F, 0, 0x60), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpunpcklbw, VEX(128, 66, 0F, IG, 0x60), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpunpcklbw, VEX(256, 66, 0F, IG, 0x60), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(punpcklwd, LEG(NP, 0F, 0, 0x61), MMX, WRR, Pq, Pq, Qd)
OPCODE(punpcklwd, LEG(66, 0F, 0, 0x61), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpunpcklwd, VEX(128, 66, 0F, IG, 0x61), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpunpcklwd, VEX(256, 66, 0F, IG, 0x61), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(punpckldq, LEG(NP, 0F, 0, 0x62), MMX, WRR, Pq, Pq, Qd)
OPCODE(punpckldq, LEG(66, 0F, 0, 0x62), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpunpckldq, VEX(128, 66, 0F, IG, 0x62), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpunpckldq, VEX(256, 66, 0F, IG, 0x62), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(punpcklqdq, LEG(66, 0F, 0, 0x6c), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vpunpcklqdq, VEX(128, 66, 0F, IG, 0x6c), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpunpcklqdq, VEX(256, 66, 0F, IG, 0x6c), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(unpcklps, LEG(NP, 0F, 0, 0x14), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(vunpcklps, VEX(128, NP, 0F, IG, 0x14), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vunpcklps, VEX(256, NP, 0F, IG, 0x14), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(unpcklpd, LEG(66, 0F, 0, 0x14), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vunpcklpd, VEX(128, 66, 0F, IG, 0x14), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vunpcklpd, VEX(256, 66, 0F, IG, 0x14), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(unpckhps, LEG(NP, 0F, 0, 0x15), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(vunpckhps, VEX(128, NP, 0F, IG, 0x15), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vunpckhps, VEX(256, NP, 0F, IG, 0x15), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(unpckhpd, LEG(66, 0F, 0, 0x15), SSE2, WRR, Vdq, Vdq, Wdq)
OPCODE(vunpckhpd, VEX(128, 66, 0F, IG, 0x15), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vunpckhpd, VEX(256, 66, 0F, IG, 0x15), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(pshufb, LEG(NP, 0F38, 0, 0x00), SSSE3, WRR, Pq, Pq, Qq)
OPCODE(pshufb, LEG(66, 0F38, 0, 0x00), SSSE3, WRR, Vdq, Vdq, Wdq)
OPCODE(vpshufb, VEX(128, 66, 0F38, IG, 0x00), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpshufb, VEX(256, 66, 0F38, IG, 0x00), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(pshufw, LEG(NP, 0F, 0, 0x70), SSE, WRR, Pq, Qq, Ib)
OPCODE(pshuflw, LEG(F2, 0F, 0, 0x70), SSE2, WRR, Vdq, Wdq, Ib)
OPCODE(vpshuflw, VEX(128, F2, 0F, IG, 0x70), AVX, WRR, Vdq, Wdq, Ib)
OPCODE(vpshuflw, VEX(256, F2, 0F, IG, 0x70), AVX2, WRR, Vqq, Wqq, Ib)
OPCODE(pshufhw, LEG(F3, 0F, 0, 0x70), SSE2, WRR, Vdq, Wdq, Ib)
OPCODE(vpshufhw, VEX(128, F3, 0F, IG, 0x70), AVX, WRR, Vdq, Wdq, Ib)
OPCODE(vpshufhw, VEX(256, F3, 0F, IG, 0x70), AVX2, WRR, Vqq, Wqq, Ib)
OPCODE(pshufd, LEG(66, 0F, 0, 0x70), SSE2, WRR, Vdq, Wdq, Ib)
OPCODE(vpshufd, VEX(128, 66, 0F, IG, 0x70), AVX, WRR, Vdq, Wdq, Ib)
OPCODE(vpshufd, VEX(256, 66, 0F, IG, 0x70), AVX2, WRR, Vqq, Wqq, Ib)
OPCODE(shufps, LEG(NP, 0F, 0, 0xc6), SSE, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(vshufps, VEX(128, NP, 0F, IG, 0xc6), AVX, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(vshufps, VEX(256, NP, 0F, IG, 0xc6), AVX, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(shufpd, LEG(66, 0F, 0, 0xc6), SSE2, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(vshufpd, VEX(128, 66, 0F, IG, 0xc6), AVX, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(vshufpd, VEX(256, 66, 0F, IG, 0xc6), AVX, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(blendps, LEG(66, 0F3A, 0, 0x0c), SSE4_1, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(vblendps, VEX(128, 66, 0F3A, IG, 0x0c), AVX, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(vblendps, VEX(256, 66, 0F3A, IG, 0x0c), AVX, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(blendpd, LEG(66, 0F3A, 0, 0x0d), SSE4_1, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(vblendpd, VEX(128, 66, 0F3A, IG, 0x0d), AVX, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(vblendpd, VEX(256, 66, 0F3A, IG, 0x0d), AVX, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(blendvps, LEG(66, 0F38, 0, 0x14), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vblendvps, VEX(128, 66, 0F3A, 0, 0x4a), AVX, WRRR, Vdq, Hdq, Wdq, Ldq)
OPCODE(vblendvps, VEX(256, 66, 0F3A, 0, 0x4a), AVX, WRRR, Vqq, Hqq, Wqq, Lqq)
OPCODE(blendvpd, LEG(66, 0F38, 0, 0x15), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vblendvpd, VEX(128, 66, 0F3A, 0, 0x4b), AVX, WRRR, Vdq, Hdq, Wdq, Ldq)
OPCODE(vblendvpd, VEX(256, 66, 0F3A, 0, 0x4b), AVX, WRRR, Vqq, Hqq, Wqq, Lqq)
OPCODE(pblendvb, LEG(66, 0F38, 0, 0x10), SSE4_1, WRR, Vdq, Vdq, Wdq)
OPCODE(vpblendvb, VEX(128, 66, 0F3A, 0, 0x4c), AVX, WRRR, Vdq, Hdq, Wdq, Ldq)
OPCODE(vpblendvb, VEX(256, 66, 0F3A, 0, 0x4c), AVX2, WRRR, Vqq, Hqq, Wqq, Lqq)
OPCODE(pblendw, LEG(66, 0F3A, 0, 0x0e), SSE4_1, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(vpblendw, VEX(128, 66, 0F3A, IG, 0x0e), AVX, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(vpblendw, VEX(256, 66, 0F3A, IG, 0x0e), AVX2, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(vpblendd, VEX(128, 66, 0F3A, 0, 0x02), AVX2, WRRR, Vdq, Hdq, Wdq, Ib)
OPCODE(vpblendd, VEX(256, 66, 0F3A, 0, 0x02), AVX2, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(insertps, LEG(66, 0F3A, 0, 0x21), SSE4_1, WRRR, Vdq, Vdq, Wd, Ib)
OPCODE(vinsertps, VEX(128, 66, 0F3A, IG, 0x21), AVX, WRRR, Vdq, Hdq, Wd, Ib)
OPCODE(pinsrb, LEG(66, 0F3A, 0, 0x20), SSE4_1, WRRR, Vdq, Vdq, RdMb, Ib)
OPCODE(vpinsrb, VEX(128, 66, 0F3A, 0, 0x20), AVX, WRRR, Vdq, Hdq, RdMb, Ib)
OPCODE(pinsrw, LEG(NP, 0F, 0, 0xc4), SSE, WRRR, Pq, Pq, RdMw, Ib)
OPCODE(pinsrw, LEG(66, 0F, 0, 0xc4), SSE2, WRRR, Vdq, Vdq, RdMw, Ib)
OPCODE(vpinsrw, VEX(128, 66, 0F, 0, 0xc4), AVX, WRRR, Vdq, Hdq, RdMw, Ib)
OPCODE(pinsrd, LEG(66, 0F3A, 0, 0x22), SSE4_1, WRRR, Vdq, Vdq, Ed, Ib)
OPCODE(vpinsrd, VEX(128, 66, 0F3A, 0, 0x22), AVX, WRRR, Vdq, Hdq, Ed, Ib)
OPCODE(pinsrq, LEG(66, 0F3A, 1, 0x22), SSE4_1, WRRR, Vdq, Vdq, Eq, Ib)
OPCODE(vpinsrq, VEX(128, 66, 0F3A, 1, 0x22), AVX, WRRR, Vdq, Hdq, Eq, Ib)
OPCODE(vinsertf128, VEX(256, 66, 0F3A, 0, 0x18), AVX, WRRR, Vqq, Hqq, Wdq, Ib)
OPCODE(vinserti128, VEX(256, 66, 0F3A, 0, 0x38), AVX2, WRRR, Vqq, Hqq, Wdq, Ib)
OPCODE(extractps, LEG(66, 0F3A, 0, 0x17), SSE4_1, WRR, Ed, Vdq, Ib)
OPCODE(vextractps, VEX(128, 66, 0F3A, IG, 0x17), AVX, WRR, Ed, Vdq, Ib)
OPCODE(pextrb, LEG(66, 0F3A, 0, 0x14), SSE4_1, WRR, RdMb, Vdq, Ib)
OPCODE(vpextrb, VEX(128, 66, 0F3A, 0, 0x14), AVX, WRR, RdMb, Vdq, Ib)
OPCODE(pextrw, LEG(66, 0F3A, 0, 0x15), SSE4_1, WRR, RdMw, Vdq, Ib)
OPCODE(vpextrw, VEX(128, 66, 0F3A, 0, 0x15), AVX, WRR, RdMw, Vdq, Ib)
OPCODE(pextrd, LEG(66, 0F3A, 0, 0x16), SSE4_1, WRR, Ed, Vdq, Ib)
OPCODE(vpextrd, VEX(128, 66, 0F3A, 0, 0x16), AVX, WRR, Ed, Vdq, Ib)
OPCODE(pextrq, LEG(66, 0F3A, 1, 0x16), SSE4_1, WRR, Eq, Vdq, Ib)
OPCODE(vpextrq, VEX(128, 66, 0F3A, 1, 0x16), AVX, WRR, Eq, Vdq, Ib)
OPCODE(pextrw, LEG(NP, 0F, 0, 0xc5), SSE, WRR, Gd, Nq, Ib)
OPCODE(pextrw, LEG(NP, 0F, 1, 0xc5), SSE, WRR, Gq, Nq, Ib)
OPCODE(pextrw, LEG(66, 0F, 0, 0xc5), SSE2, WRR, Gd, Udq, Ib)
OPCODE(pextrw, LEG(66, 0F, 1, 0xc5), SSE2, WRR, Gq, Udq, Ib)
OPCODE(vpextrw, VEX(128, 66, 0F, 0, 0xc5), AVX, WRR, Gd, Udq, Ib)
OPCODE(vpextrw, VEX(128, 66, 0F, 1, 0xc5), AVX, WRR, Gq, Udq, Ib)
OPCODE(vextractf128, VEX(256, 66, 0F3A, 0, 0x19), AVX, WRR, Wdq, Vqq, Ib)
OPCODE(vextracti128, VEX(256, 66, 0F3A, 0, 0x39), AVX2, WRR, Wdq, Vqq, Ib)
OPCODE(vpbroadcastb, VEX(128, 66, 0F38, 0, 0x78), AVX2, WR, Vdq, Wb)
OPCODE(vpbroadcastb, VEX(256, 66, 0F38, 0, 0x78), AVX2, WR, Vqq, Wb)
OPCODE(vpbroadcastw, VEX(128, 66, 0F38, 0, 0x79), AVX2, WR, Vdq, Ww)
OPCODE(vpbroadcastw, VEX(256, 66, 0F38, 0, 0x79), AVX2, WR, Vqq, Ww)
OPCODE(vpbroadcastd, VEX(128, 66, 0F38, 0, 0x58), AVX2, WR, Vdq, Wd)
OPCODE(vpbroadcastd, VEX(256, 66, 0F38, 0, 0x58), AVX2, WR, Vqq, Wd)
OPCODE(vpbroadcastq, VEX(128, 66, 0F38, 0, 0x59), AVX2, WR, Vdq, Wq)
OPCODE(vpbroadcastq, VEX(256, 66, 0F38, 0, 0x59), AVX2, WR, Vqq, Wq)
OPCODE(vbroadcastss, VEX(128, 66, 0F38, 0, 0x18), AVX, WRR, Vdq, Wd, modrm_mod)
OPCODE(vbroadcastss, VEX(256, 66, 0F38, 0, 0x18), AVX, WRR, Vqq, Wd, modrm_mod)
OPCODE(vbroadcastsd, VEX(256, 66, 0F38, 0, 0x19), AVX, WRR, Vqq, Wq, modrm_mod)
OPCODE(vbroadcastf128, VEX(256, 66, 0F38, 0, 0x1a), AVX, WR, Vqq, Mdq)
OPCODE(vbroadcasti128, VEX(256, 66, 0F38, 0, 0x5a), AVX2, WR, Vqq, Mdq)
OPCODE(vperm2f128, VEX(256, 66, 0F3A, 0, 0x06), AVX, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(vperm2i128, VEX(256, 66, 0F3A, 0, 0x46), AVX2, WRRR, Vqq, Hqq, Wqq, Ib)
OPCODE(vpermd, VEX(256, 66, 0F38, 0, 0x36), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(vpermps, VEX(256, 66, 0F38, 0, 0x16), AVX2, WRR, Vqq, Hqq, Wqq)
OPCODE(vpermilps, VEX(128, 66, 0F38, 0, 0x0c), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpermilps, VEX(256, 66, 0F38, 0, 0x0c), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(vpermilps, VEX(128, 66, 0F3A, 0, 0x04), AVX, WRR, Vdq, Wdq, Ib)
OPCODE(vpermilps, VEX(256, 66, 0F3A, 0, 0x04), AVX, WRR, Vqq, Wqq, Ib)
OPCODE(vpermilpd, VEX(128, 66, 0F38, 0, 0x0d), AVX, WRR, Vdq, Hdq, Wdq)
OPCODE(vpermilpd, VEX(256, 66, 0F38, 0, 0x0d), AVX, WRR, Vqq, Hqq, Wqq)
OPCODE(vpermilpd, VEX(128, 66, 0F3A, 0, 0x05), AVX, WRR, Vdq, Wdq, Ib)
OPCODE(vpermilpd, VEX(256, 66, 0F3A, 0, 0x05), AVX, WRR, Vqq, Wqq, Ib)
OPCODE(vpermq, VEX(256, 66, 0F3A, 1, 0x00), AVX2, WRR, Vqq, Wqq, Ib)
OPCODE(vpermpd, VEX(256, 66, 0F3A, 1, 0x01), AVX2, WRR, Vqq, Wqq, Ib)
OPCODE(vgatherdps, VEX(128, 66, 0F38, 0, 0x92), AVX2, WWRRR, Vdq, Hdq, Vdq, MDdq, Hdq)
OPCODE(vgatherdps, VEX(256, 66, 0F38, 0, 0x92), AVX2, WWRRR, Vqq, Hqq, Vqq, MDqq, Hqq)
OPCODE(vgatherdpd, VEX(128, 66, 0F38, 1, 0x92), AVX2, WWRRR, Vdq, Hdq, Vdq, MDdq, Hdq)
OPCODE(vgatherdpd, VEX(256, 66, 0F38, 1, 0x92), AVX2, WWRRR, Vqq, Hqq, Vqq, MDdq, Hqq)
OPCODE(vgatherqps, VEX(128, 66, 0F38, 0, 0x93), AVX2, WWRRR, Vdq, Hdq, Vdq, MQdq, Hdq)
OPCODE(vgatherqps, VEX(256, 66, 0F38, 0, 0x93), AVX2, WWRRR, Vdq, Hdq, Vdq, MQqq, Hdq)
OPCODE(vgatherqpd, VEX(128, 66, 0F38, 1, 0x93), AVX2, WWRRR, Vdq, Hdq, Vdq, MQdq, Hdq)
OPCODE(vgatherqpd, VEX(256, 66, 0F38, 1, 0x93), AVX2, WWRRR, Vqq, Hqq, Vqq, MQqq, Hqq)
OPCODE(vpgatherdd, VEX(128, 66, 0F38, 0, 0x90), AVX2, WWRRR, Vdq, Hdq, Vdq, MDdq, Hdq)
OPCODE(vpgatherdd, VEX(256, 66, 0F38, 0, 0x90), AVX2, WWRRR, Vqq, Hqq, Vqq, MDqq, Hqq)
OPCODE(vpgatherdq, VEX(128, 66, 0F38, 1, 0x90), AVX2, WWRRR, Vdq, Hdq, Vdq, MDdq, Hdq)
OPCODE(vpgatherdq, VEX(256, 66, 0F38, 1, 0x90), AVX2, WWRRR, Vqq, Hqq, Vqq, MDdq, Hqq)
OPCODE(vpgatherqd, VEX(128, 66, 0F38, 0, 0x91), AVX2, WWRRR, Vdq, Hdq, Vdq, MQdq, Hdq)
OPCODE(vpgatherqd, VEX(256, 66, 0F38, 0, 0x91), AVX2, WWRRR, Vdq, Hdq, Vdq, MQqq, Hdq)
OPCODE(vpgatherqq, VEX(128, 66, 0F38, 1, 0x91), AVX2, WWRRR, Vdq, Hdq, Vdq, MQdq, Hdq)
OPCODE(vpgatherqq, VEX(256, 66, 0F38, 1, 0x91), AVX2, WWRRR, Vqq, Hqq, Vqq, MQqq, Hqq)
OPCODE(pmovsxbw, LEG(66, 0F38, 0, 0x20), SSE4_1, WR, Vdq, Wq)
OPCODE(vpmovsxbw, VEX(128, 66, 0F38, IG, 0x20), AVX, WR, Vdq, Wq)
OPCODE(vpmovsxbw, VEX(256, 66, 0F38, IG, 0x20), AVX2, WR, Vqq, Wdq)
OPCODE(pmovsxbd, LEG(66, 0F38, 0, 0x21), SSE4_1, WR, Vdq, Wd)
OPCODE(vpmovsxbd, VEX(128, 66, 0F38, IG, 0x21), AVX, WR, Vdq, Wd)
OPCODE(vpmovsxbd, VEX(256, 66, 0F38, IG, 0x21), AVX2, WR, Vqq, Wq)
OPCODE(pmovsxbq, LEG(66, 0F38, 0, 0x22), SSE4_1, WR, Vdq, Ww)
OPCODE(vpmovsxbq, VEX(128, 66, 0F38, IG, 0x22), AVX, WR, Vdq, Ww)
OPCODE(vpmovsxbq, VEX(256, 66, 0F38, IG, 0x22), AVX2, WR, Vqq, Wd)
OPCODE(pmovsxwd, LEG(66, 0F38, 0, 0x23), SSE4_1, WR, Vdq, Wq)
OPCODE(vpmovsxwd, VEX(128, 66, 0F38, IG, 0x23), AVX, WR, Vdq, Wq)
OPCODE(vpmovsxwd, VEX(256, 66, 0F38, IG, 0x23), AVX2, WR, Vqq, Wdq)
OPCODE(pmovsxwq, LEG(66, 0F38, 0, 0x24), SSE4_1, WR, Vdq, Wd)
OPCODE(vpmovsxwq, VEX(128, 66, 0F38, IG, 0x24), AVX, WR, Vdq, Wd)
OPCODE(vpmovsxwq, VEX(256, 66, 0F38, IG, 0x24), AVX2, WR, Vqq, Wq)
OPCODE(pmovsxdq, LEG(66, 0F38, 0, 0x25), SSE4_1, WR, Vdq, Wq)
OPCODE(vpmovsxdq, VEX(128, 66, 0F38, IG, 0x25), AVX, WR, Vdq, Wq)
OPCODE(vpmovsxdq, VEX(256, 66, 0F38, IG, 0x25), AVX2, WR, Vqq, Wdq)
OPCODE(pmovzxbw, LEG(66, 0F38, 0, 0x30), SSE4_1, WR, Vdq, Wq)
OPCODE(vpmovzxbw, VEX(128, 66, 0F38, IG, 0x30), AVX, WR, Vdq, Wq)
OPCODE(vpmovzxbw, VEX(256, 66, 0F38, IG, 0x30), AVX2, WR, Vqq, Wdq)
OPCODE(pmovzxbd, LEG(66, 0F38, 0, 0x31), SSE4_1, WR, Vdq, Wd)
OPCODE(vpmovzxbd, VEX(128, 66, 0F38, IG, 0x31), AVX, WR, Vdq, Wd)
OPCODE(vpmovzxbd, VEX(256, 66, 0F38, IG, 0x31), AVX2, WR, Vqq, Wq)
OPCODE(pmovzxbq, LEG(66, 0F38, 0, 0x32), SSE4_1, WR, Vdq, Ww)
OPCODE(vpmovzxbq, VEX(128, 66, 0F38, IG, 0x32), AVX, WR, Vdq, Ww)
OPCODE(vpmovzxbq, VEX(256, 66, 0F38, IG, 0x32), AVX2, WR, Vqq, Wd)
OPCODE(pmovzxwd, LEG(66, 0F38, 0, 0x33), SSE4_1, WR, Vdq, Wq)
OPCODE(vpmovzxwd, VEX(128, 66, 0F38, IG, 0x33), AVX, WR, Vdq, Wq)
OPCODE(vpmovzxwd, VEX(256, 66, 0F38, IG, 0x33), AVX2, WR, Vqq, Wdq)
OPCODE(pmovzxwq, LEG(66, 0F38, 0, 0x34), SSE4_1, WR, Vdq, Wd)
OPCODE(vpmovzxwq, VEX(128, 66, 0F38, IG, 0x34), AVX, WR, Vdq, Wd)
OPCODE(vpmovzxwq, VEX(256, 66, 0F38, IG, 0x34), AVX2, WR, Vqq, Wq)
OPCODE(pmovzxdq, LEG(66, 0F38, 0, 0x35), SSE4_1, WR, Vdq, Wq)
OPCODE(vpmovzxdq, VEX(128, 66, 0F38, IG, 0x35), AVX, WR, Vdq, Wq)
OPCODE(vpmovzxdq, VEX(256, 66, 0F38, IG, 0x35), AVX2, WR, Vqq, Wdq)
OPCODE(cvtpi2ps, LEG(NP, 0F, 0, 0x2a), SSE, WR, Vdq, Qq)
OPCODE(cvtsi2ss, LEG(F3, 0F, 0, 0x2a), SSE, WR, Vd, Ed)
OPCODE(cvtsi2ss, LEG(F3, 0F, 1, 0x2a), SSE, WR, Vd, Eq)
OPCODE(vcvtsi2ss, VEX(IG, F3, 0F, 0, 0x2a), AVX, WRR, Vd, Hd, Ed)
OPCODE(vcvtsi2ss, VEX(IG, F3, 0F, 1, 0x2a), AVX, WRR, Vd, Hd, Eq)
OPCODE(cvtpi2pd, LEG(66, 0F, 0, 0x2a), SSE2, WR, Vdq, Qq)
OPCODE(cvtsi2sd, LEG(F2, 0F, 0, 0x2a), SSE2, WR, Vq, Ed)
OPCODE(cvtsi2sd, LEG(F2, 0F, 1, 0x2a), SSE2, WR, Vq, Eq)
OPCODE(vcvtsi2sd, VEX(IG, F2, 0F, 0, 0x2a), AVX, WRR, Vq, Hq, Ed)
OPCODE(vcvtsi2sd, VEX(IG, F2, 0F, 1, 0x2a), AVX, WRR, Vq, Hq, Eq)
OPCODE(cvtps2pi, LEG(NP, 0F, 0, 0x2d), SSE, WR, Pq, Wq)
OPCODE(cvtss2si, LEG(F3, 0F, 0, 0x2d), SSE, WR, Gd, Wd)
OPCODE(cvtss2si, LEG(F3, 0F, 1, 0x2d), SSE, WR, Gq, Wd)
OPCODE(vcvtss2si, VEX(IG, F3, 0F, 0, 0x2d), AVX, WR, Gd, Wd)
OPCODE(vcvtss2si, VEX(IG, F3, 0F, 1, 0x2d), AVX, WR, Gq, Wd)
OPCODE(cvtpd2pi, LEG(66, 0F, 0, 0x2d), SSE2, WR, Pq, Wdq)
OPCODE(cvtsd2si, LEG(F2, 0F, 0, 0x2d), SSE2, WR, Gd, Wq)
OPCODE(cvtsd2si, LEG(F2, 0F, 1, 0x2d), SSE2, WR, Gq, Wq)
OPCODE(vcvtsd2si, VEX(IG, F2, 0F, 0, 0x2d), AVX, WR, Gd, Wq)
OPCODE(vcvtsd2si, VEX(IG, F2, 0F, 1, 0x2d), AVX, WR, Gq, Wq)
OPCODE(cvttps2pi, LEG(NP, 0F, 0, 0x2c), SSE, WR, Pq, Wq)
OPCODE(cvttss2si, LEG(F3, 0F, 0, 0x2c), SSE, WR, Gd, Wd)
OPCODE(cvttss2si, LEG(F3, 0F, 1, 0x2c), SSE, WR, Gq, Wd)
OPCODE(vcvttss2si, VEX(IG, F3, 0F, 0, 0x2c), AVX, WR, Gd, Wd)
OPCODE(vcvttss2si, VEX(IG, F3, 0F, 1, 0x2c), AVX, WR, Gq, Wd)
OPCODE(cvttpd2pi, LEG(66, 0F, 0, 0x2c), SSE2, WR, Pq, Wdq)
OPCODE(cvttsd2si, LEG(F2, 0F, 0, 0x2c), SSE2, WR, Gd, Wq)
OPCODE(cvttsd2si, LEG(F2, 0F, 1, 0x2c), SSE2, WR, Gq, Wq)
OPCODE(vcvttsd2si, VEX(IG, F2, 0F, 0, 0x2c), AVX, WR, Gd, Wq)
OPCODE(vcvttsd2si, VEX(IG, F2, 0F, 1, 0x2c), AVX, WR, Gq, Wq)
OPCODE(cvtpd2dq, LEG(F2, 0F, 0, 0xe6), SSE2, WR, Vdq, Wdq)
OPCODE(vcvtpd2dq, VEX(128, F2, 0F, IG, 0xe6), AVX, WR, Vdq, Wdq)
OPCODE(vcvtpd2dq, VEX(256, F2, 0F, IG, 0xe6), AVX, WR, Vdq, Wqq)
OPCODE(cvttpd2dq, LEG(66, 0F, 0, 0xe6), SSE2, WR, Vdq, Wdq)
OPCODE(vcvttpd2dq, VEX(128, 66, 0F, IG, 0xe6), AVX, WR, Vdq, Wdq)
OPCODE(vcvttpd2dq, VEX(256, 66, 0F, IG, 0xe6), AVX, WR, Vdq, Wqq)
OPCODE(cvtdq2pd, LEG(F3, 0F, 0, 0xe6), SSE2, WR, Vdq, Wq)
OPCODE(vcvtdq2pd, VEX(128, F3, 0F, IG, 0xe6), AVX, WR, Vdq, Wq)
OPCODE(vcvtdq2pd, VEX(256, F3, 0F, IG, 0xe6), AVX, WR, Vqq, Wdq)
OPCODE(cvtps2pd, LEG(NP, 0F, 0, 0x5a), SSE2, WR, Vdq, Wq)
OPCODE(vcvtps2pd, VEX(128, NP, 0F, IG, 0x5a), AVX, WR, Vdq, Wq)
OPCODE(vcvtps2pd, VEX(256, NP, 0F, IG, 0x5a), AVX, WR, Vqq, Wdq)
OPCODE(cvtpd2ps, LEG(66, 0F, 0, 0x5a), SSE2, WR, Vdq, Wdq)
OPCODE(vcvtpd2ps, VEX(128, 66, 0F, IG, 0x5a), AVX, WR, Vdq, Wdq)
OPCODE(vcvtpd2ps, VEX(256, 66, 0F, IG, 0x5a), AVX, WR, Vdq, Wqq)
OPCODE(cvtss2sd, LEG(F3, 0F, 0, 0x5a), SSE2, WR, Vq, Wd)
OPCODE(vcvtss2sd, VEX(IG, F3, 0F, IG, 0x5a), AVX, WRR, Vq, Hq, Wd)
OPCODE(cvtsd2ss, LEG(F2, 0F, 0, 0x5a), SSE2, WR, Vd, Wq)
OPCODE(vcvtsd2ss, VEX(IG, F2, 0F, IG, 0x5a), AVX, WRR, Vd, Hd, Wq)
OPCODE(cvtdq2ps, LEG(NP, 0F, 0, 0x5b), SSE2, WR, Vdq, Wdq)
OPCODE(vcvtdq2ps, VEX(128, NP, 0F, IG, 0x5b), AVX, WR, Vdq, Wdq)
OPCODE(vcvtdq2ps, VEX(256, NP, 0F, IG, 0x5b), AVX, WR, Vqq, Wqq)
OPCODE(cvtps2dq, LEG(66, 0F, 0, 0x5b), SSE2, WR, Vdq, Wdq)
OPCODE(vcvtps2dq, VEX(128, 66, 0F, IG, 0x5b), AVX, WR, Vdq, Wdq)
OPCODE(vcvtps2dq, VEX(256, 66, 0F, IG, 0x5b), AVX, WR, Vqq, Wqq)
OPCODE(cvttps2dq, LEG(F3, 0F, 0, 0x5b), SSE2, WR, Vdq, Wdq)
OPCODE(vcvttps2dq, VEX(128, F3, 0F, IG, 0x5b), AVX, WR, Vdq, Wdq)
OPCODE(vcvttps2dq, VEX(256, F3, 0F, IG, 0x5b), AVX, WR, Vqq, Wqq)
OPCODE(maskmovq, LEG(NP, 0F, 0, 0xf7), SSE, RR, Pq, Nq)
OPCODE(maskmovdqu, LEG(66, 0F, 0, 0xf7), SSE2, RR, Vdq, Udq)
OPCODE(vmaskmovdqu, VEX(128, 66, 0F, IG, 0xf7), AVX, RR, Vdq, Udq)
OPCODE(vmaskmovps, VEX(128, 66, 0F38, 0, 0x2c), AVX, WRR, Vdq, Hdq, Mdq)
OPCODE(vmaskmovps, VEX(128, 66, 0F38, 0, 0x2e), AVX, WRR, Mdq, Hdq, Vdq)
OPCODE(vmaskmovps, VEX(256, 66, 0F38, 0, 0x2c), AVX, WRR, Vqq, Hqq, Mqq)
OPCODE(vmaskmovps, VEX(256, 66, 0F38, 0, 0x2e), AVX, WRR, Mqq, Hqq, Vqq)
OPCODE(vmaskmovpd, VEX(128, 66, 0F38, 0, 0x2d), AVX, WRR, Vdq, Hdq, Mdq)
OPCODE(vmaskmovpd, VEX(128, 66, 0F38, 0, 0x2f), AVX, WRR, Mdq, Hdq, Vdq)
OPCODE(vmaskmovpd, VEX(256, 66, 0F38, 0, 0x2d), AVX, WRR, Vqq, Hqq, Mqq)
OPCODE(vmaskmovpd, VEX(256, 66, 0F38, 0, 0x2f), AVX, WRR, Mqq, Hqq, Vqq)
OPCODE(vpmaskmovd, VEX(128, 66, 0F38, 0, 0x8c), AVX2, WRR, Vdq, Hdq, Mdq)
OPCODE(vpmaskmovd, VEX(128, 66, 0F38, 0, 0x8e), AVX2, WRR, Mdq, Hdq, Vdq)
OPCODE(vpmaskmovd, VEX(256, 66, 0F38, 0, 0x8c), AVX2, WRR, Vqq, Hqq, Mqq)
OPCODE(vpmaskmovd, VEX(256, 66, 0F38, 0, 0x8e), AVX2, WRR, Mqq, Hqq, Vqq)
OPCODE(vpmaskmovq, VEX(128, 66, 0F38, 1, 0x8c), AVX2, WRR, Vdq, Hdq, Mdq)
OPCODE(vpmaskmovq, VEX(128, 66, 0F38, 1, 0x8e), AVX2, WRR, Mdq, Hdq, Vdq)
OPCODE(vpmaskmovq, VEX(256, 66, 0F38, 1, 0x8c), AVX2, WRR, Vqq, Hqq, Mqq)
OPCODE(vpmaskmovq, VEX(256, 66, 0F38, 1, 0x8e), AVX2, WRR, Mqq, Hqq, Vqq)
OPCODE(movntps, LEG(NP, 0F, 0, 0x2b), SSE, WR, Mdq, Vdq)
OPCODE(vmovntps, VEX(128, NP, 0F, IG, 0x2b), AVX, WR, Mdq, Vdq)
OPCODE(vmovntps, VEX(256, NP, 0F, IG, 0x2b), AVX, WR, Mqq, Vqq)
OPCODE(movntpd, LEG(66, 0F, 0, 0x2b), SSE2, WR, Mdq, Vdq)
OPCODE(vmovntpd, VEX(128, 66, 0F, IG, 0x2b), AVX, WR, Mdq, Vdq)
OPCODE(vmovntpd, VEX(256, 66, 0F, IG, 0x2b), AVX, WR, Mqq, Vqq)
OPCODE(movnti, LEG(NP, 0F, 0, 0xc3), SSE2, WR, Md, Gd)
OPCODE(movnti, LEG(NP, 0F, 1, 0xc3), SSE2, WR, Mq, Gq)
OPCODE(movntq, LEG(NP, 0F, 0, 0xe7), SSE, WR, Mq, Pq)
OPCODE(movntdq, LEG(66, 0F, 0, 0xe7), SSE2, WR, Mdq, Vdq)
OPCODE(vmovntdq, VEX(128, 66, 0F, IG, 0xe7), AVX, WR, Mdq, Vdq)
OPCODE(vmovntdq, VEX(256, 66, 0F, IG, 0xe7), AVX, WR, Mqq, Vqq)
OPCODE(movntdqa, LEG(66, 0F38, 0, 0x2a), SSE4_1, WR, Vdq, Mdq)
OPCODE(vmovntdqa, VEX(128, 66, 0F38, IG, 0x2a), AVX, WR, Vdq, Mdq)
OPCODE(vmovntdqa, VEX(256, 66, 0F38, IG, 0x2a), AVX2, WR, Vqq, Mqq)
OPCODE(pause, LEG(F3, NA, 0, 0x90), SSE2, )
OPCODE(emms, LEG(NP, 0F, 0, 0x77), MMX, )
OPCODE(vzeroupper, VEX(128, NP, 0F, IG, 0x77), AVX, )
OPCODE(vzeroall, VEX(256, NP, 0F, IG, 0x77), AVX, )

OPCODE_GRP(grp12_LEG_66, LEG(66, 0F, 0, 0x71))
OPCODE_GRP_BEGIN(grp12_LEG_66)
    OPCODE_GRPMEMB(grp12_LEG_66, psllw, 6, SSE2, WRR, Udq, Udq, Ib)
    OPCODE_GRPMEMB(grp12_LEG_66, psrlw, 2, SSE2, WRR, Udq, Udq, Ib)
    OPCODE_GRPMEMB(grp12_LEG_66, psraw, 4, SSE2, WRR, Udq, Udq, Ib)
OPCODE_GRP_END(grp12_LEG_66)

OPCODE_GRP(grp12_LEG_NP, LEG(NP, 0F, 0, 0x71))
OPCODE_GRP_BEGIN(grp12_LEG_NP)
    OPCODE_GRPMEMB(grp12_LEG_NP, psllw, 6, MMX, WRR, Nq, Nq, Ib)
    OPCODE_GRPMEMB(grp12_LEG_NP, psrlw, 2, MMX, WRR, Nq, Nq, Ib)
    OPCODE_GRPMEMB(grp12_LEG_NP, psraw, 4, MMX, WRR, Nq, Nq, Ib)
OPCODE_GRP_END(grp12_LEG_NP)

OPCODE_GRP(grp12_VEX_128_66, VEX(128, 66, 0F, IG, 0x71))
OPCODE_GRP_BEGIN(grp12_VEX_128_66)
    OPCODE_GRPMEMB(grp12_VEX_128_66, vpsllw, 6, AVX, WRR, Hdq, Udq, Ib)
    OPCODE_GRPMEMB(grp12_VEX_128_66, vpsrlw, 2, AVX, WRR, Hdq, Udq, Ib)
    OPCODE_GRPMEMB(grp12_VEX_128_66, vpsraw, 4, AVX, WRR, Hdq, Udq, Ib)
OPCODE_GRP_END(grp12_VEX_128_66)

OPCODE_GRP(grp12_VEX_256_66, VEX(256, 66, 0F, IG, 0x71))
OPCODE_GRP_BEGIN(grp12_VEX_256_66)
    OPCODE_GRPMEMB(grp12_VEX_256_66, vpsllw, 6, AVX2, WRR, Hqq, Uqq, Ib)
    OPCODE_GRPMEMB(grp12_VEX_256_66, vpsrlw, 2, AVX2, WRR, Hqq, Uqq, Ib)
    OPCODE_GRPMEMB(grp12_VEX_256_66, vpsraw, 4, AVX2, WRR, Hqq, Uqq, Ib)
OPCODE_GRP_END(grp12_VEX_256_66)

OPCODE_GRP(grp13_LEG_66, LEG(66, 0F, 0, 0x72))
OPCODE_GRP_BEGIN(grp13_LEG_66)
    OPCODE_GRPMEMB(grp13_LEG_66, pslld, 6, SSE2, WRR, Udq, Udq, Ib)
    OPCODE_GRPMEMB(grp13_LEG_66, psrld, 2, SSE2, WRR, Udq, Udq, Ib)
    OPCODE_GRPMEMB(grp13_LEG_66, psrad, 4, SSE2, WRR, Udq, Udq, Ib)
OPCODE_GRP_END(grp13_LEG_66)

OPCODE_GRP(grp13_LEG_NP, LEG(NP, 0F, 0, 0x72))
OPCODE_GRP_BEGIN(grp13_LEG_NP)
    OPCODE_GRPMEMB(grp13_LEG_NP, pslld, 6, MMX, WRR, Nq, Nq, Ib)
    OPCODE_GRPMEMB(grp13_LEG_NP, psrld, 2, MMX, WRR, Nq, Nq, Ib)
    OPCODE_GRPMEMB(grp13_LEG_NP, psrad, 4, MMX, WRR, Nq, Nq, Ib)
OPCODE_GRP_END(grp13_LEG_NP)

OPCODE_GRP(grp13_VEX_128_66, VEX(128, 66, 0F, IG, 0x72))
OPCODE_GRP_BEGIN(grp13_VEX_128_66)
    OPCODE_GRPMEMB(grp13_VEX_128_66, vpslld, 6, AVX, WRR, Hdq, Udq, Ib)
    OPCODE_GRPMEMB(grp13_VEX_128_66, vpsrld, 2, AVX, WRR, Hdq, Udq, Ib)
    OPCODE_GRPMEMB(grp13_VEX_128_66, vpsrad, 4, AVX, WRR, Hdq, Udq, Ib)
OPCODE_GRP_END(grp13_VEX_128_66)

OPCODE_GRP(grp13_VEX_256_66, VEX(256, 66, 0F, IG, 0x72))
OPCODE_GRP_BEGIN(grp13_VEX_256_66)
    OPCODE_GRPMEMB(grp13_VEX_256_66, vpslld, 6, AVX2, WRR, Hqq, Uqq, Ib)
    OPCODE_GRPMEMB(grp13_VEX_256_66, vpsrld, 2, AVX2, WRR, Hqq, Uqq, Ib)
    OPCODE_GRPMEMB(grp13_VEX_256_66, vpsrad, 4, AVX2, WRR, Hqq, Uqq, Ib)
OPCODE_GRP_END(grp13_VEX_256_66)

OPCODE_GRP(grp14_LEG_66, LEG(66, 0F, 0, 0x73))
OPCODE_GRP_BEGIN(grp14_LEG_66)
    OPCODE_GRPMEMB(grp14_LEG_66, psllq, 6, SSE2, WRR, Udq, Udq, Ib)
    OPCODE_GRPMEMB(grp14_LEG_66, pslldq, 7, SSE2, WRR, Udq, Udq, Ib)
    OPCODE_GRPMEMB(grp14_LEG_66, psrlq, 2, SSE2, WRR, Udq, Udq, Ib)
    OPCODE_GRPMEMB(grp14_LEG_66, psrldq, 3, SSE2, WRR, Udq, Udq, Ib)
OPCODE_GRP_END(grp14_LEG_66)

OPCODE_GRP(grp14_LEG_NP, LEG(NP, 0F, 0, 0x73))
OPCODE_GRP_BEGIN(grp14_LEG_NP)
    OPCODE_GRPMEMB(grp14_LEG_NP, psllq, 6, MMX, WRR, Nq, Nq, Ib)
    OPCODE_GRPMEMB(grp14_LEG_NP, psrlq, 2, MMX, WRR, Nq, Nq, Ib)
OPCODE_GRP_END(grp14_LEG_NP)

OPCODE_GRP(grp14_VEX_128_66, VEX(128, 66, 0F, IG, 0x73))
OPCODE_GRP_BEGIN(grp14_VEX_128_66)
    OPCODE_GRPMEMB(grp14_VEX_128_66, vpsllq, 6, AVX, WRR, Hdq, Udq, Ib)
    OPCODE_GRPMEMB(grp14_VEX_128_66, vpslldq, 7, AVX, WRR, Hdq, Udq, Ib)
    OPCODE_GRPMEMB(grp14_VEX_128_66, vpsrlq, 2, AVX, WRR, Hdq, Udq, Ib)
    OPCODE_GRPMEMB(grp14_VEX_128_66, vpsrldq, 3, AVX, WRR, Hdq, Udq, Ib)
OPCODE_GRP_END(grp14_VEX_128_66)

OPCODE_GRP(grp14_VEX_256_66, VEX(256, 66, 0F, IG, 0x73))
OPCODE_GRP_BEGIN(grp14_VEX_256_66)
    OPCODE_GRPMEMB(grp14_VEX_256_66, vpsllq, 6, AVX2, WRR, Hqq, Uqq, Ib)
    OPCODE_GRPMEMB(grp14_VEX_256_66, vpslldq, 7, AVX2, WRR, Hqq, Uqq, Ib)
    OPCODE_GRPMEMB(grp14_VEX_256_66, vpsrlq, 2, AVX2, WRR, Hqq, Uqq, Ib)
    OPCODE_GRPMEMB(grp14_VEX_256_66, vpsrldq, 3, AVX2, WRR, Hqq, Uqq, Ib)
OPCODE_GRP_END(grp14_VEX_256_66)

OPCODE_GRP(grp15_LEG_NP, LEG(NP, 0F, 0, 0xae))
OPCODE_GRP_BEGIN(grp15_LEG_NP)
    OPCODE_GRPMEMB(grp15_LEG_NP, sfence_clflush, 7, SSE, RR, modrm_mod, modrm)
    OPCODE_GRPMEMB(grp15_LEG_NP, lfence, 5, SSE2, )
    OPCODE_GRPMEMB(grp15_LEG_NP, mfence, 6, SSE2, )
    OPCODE_GRPMEMB(grp15_LEG_NP, ldmxcsr, 2, SSE, R, Md)
    OPCODE_GRPMEMB(grp15_LEG_NP, stmxcsr, 3, SSE, W, Md)
OPCODE_GRP_END(grp15_LEG_NP)

OPCODE_GRP(grp15_VEX_Z_NP, VEX(Z, NP, 0F, IG, 0xae))
OPCODE_GRP_BEGIN(grp15_VEX_Z_NP)
    OPCODE_GRPMEMB(grp15_VEX_Z_NP, vldmxcsr, 2, AVX, R, Md)
    OPCODE_GRPMEMB(grp15_VEX_Z_NP, vstmxcsr, 3, AVX, W, Md)
OPCODE_GRP_END(grp15_VEX_Z_NP)

OPCODE_GRP(grp16_LEG_NP, LEG(NP, 0F, 0, 0x18))
OPCODE_GRP_BEGIN(grp16_LEG_NP)
    OPCODE_GRPMEMB(grp16_LEG_NP, prefetcht0, 1, SSE, R, Mb)
    OPCODE_GRPMEMB(grp16_LEG_NP, prefetcht1, 2, SSE, R, Mb)
    OPCODE_GRPMEMB(grp16_LEG_NP, prefetcht2, 3, SSE, R, Mb)
    OPCODE_GRPMEMB(grp16_LEG_NP, prefetchnta, 0, SSE, R, Mb)
OPCODE_GRP_END(grp16_LEG_NP)

#undef FMTI____
#undef FMTI__R__
#undef FMTI__RR__
#undef FMTI__RRR__
#undef FMTI__W__
#undef FMTI__WR__
#undef FMTI__WRR__
#undef FMTI__WRRR__
#undef FMTI__WRRRR__
#undef FMTI__WWRRR__

#undef FMTI__

#undef FMTI_ARGC__
#undef FMTI_ARGC_RD__
#undef FMTI_ARGC_WR__
#undef FMTI_LOWER__

#undef FMT_ARGC
#undef FMT_ARGC_RD
#undef FMT_ARGC_WR
#undef FMT_LOWER
#undef FMT_UPPER

#undef LEG
#undef VEX
#undef OPCODE
#undef OPCODE_GRP
#undef OPCODE_GRP_BEGIN
#undef OPCODE_GRPMEMB
#undef OPCODE_GRP_END
