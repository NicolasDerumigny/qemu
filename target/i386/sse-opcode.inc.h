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
 */

OPCODE(movd, LEG(NP, 0F, 0, 0x6e), MMX, WR, Pq, Ed)
OPCODE(movd, LEG(NP, 0F, 0, 0x7e), MMX, WR, Ed, Pq)
OPCODE(movq, LEG(NP, 0F, 1, 0x6e), MMX, WR, Pq, Eq)
OPCODE(movq, LEG(NP, 0F, 1, 0x7e), MMX, WR, Eq, Pq)
OPCODE(movq, LEG(NP, 0F, 0, 0x6f), MMX, WR, Pq, Qq)
OPCODE(movq, LEG(NP, 0F, 0, 0x7f), MMX, WR, Qq, Pq)
OPCODE(movaps, LEG(NP, 0F, 0, 0x28), SSE, WR, Vdq, Wdq)
OPCODE(movaps, LEG(NP, 0F, 0, 0x29), SSE, WR, Wdq, Vdq)
OPCODE(movups, LEG(NP, 0F, 0, 0x10), SSE, WR, Vdq, Wdq)
OPCODE(movups, LEG(NP, 0F, 0, 0x11), SSE, WR, Wdq, Vdq)
OPCODE(movss, LEG(F3, 0F, 0, 0x10), SSE, WRRR, Vdq, Vdq, Wd, modrm_mod)
OPCODE(movss, LEG(F3, 0F, 0, 0x11), SSE, WR, Wd, Vd)
OPCODE(movhlps, LEG(NP, 0F, 0, 0x12), SSE, WRR, Vdq, Vdq, UdqMhq)
OPCODE(movlps, LEG(NP, 0F, 0, 0x13), SSE, WR, Mq, Vq)
OPCODE(movlhps, LEG(NP, 0F, 0, 0x16), SSE, WRR, Vdq, Vq, Wq)
OPCODE(movhps, LEG(NP, 0F, 0, 0x17), SSE, WR, Mq, Vdq)
OPCODE(pmovmskb, LEG(NP, 0F, 0, 0xd7), SSE, WR, Gd, Nq)
OPCODE(pmovmskb, LEG(NP, 0F, 1, 0xd7), SSE, WR, Gq, Nq)
OPCODE(movmskps, LEG(NP, 0F, 0, 0x50), SSE, WR, Gd, Udq)
OPCODE(movmskps, LEG(NP, 0F, 1, 0x50), SSE, WR, Gq, Udq)
OPCODE(paddb, LEG(NP, 0F, 0, 0xfc), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddw, LEG(NP, 0F, 0, 0xfd), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddd, LEG(NP, 0F, 0, 0xfe), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddsb, LEG(NP, 0F, 0, 0xec), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddsw, LEG(NP, 0F, 0, 0xed), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddusb, LEG(NP, 0F, 0, 0xdc), MMX, WRR, Pq, Pq, Qq)
OPCODE(paddusw, LEG(NP, 0F, 0, 0xdd), MMX, WRR, Pq, Pq, Qq)
OPCODE(addps, LEG(NP, 0F, 0, 0x58), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(addss, LEG(F3, 0F, 0, 0x58), SSE, WRR, Vd, Vd, Wd)
OPCODE(psubb, LEG(NP, 0F, 0, 0xf8), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubw, LEG(NP, 0F, 0, 0xf9), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubd, LEG(NP, 0F, 0, 0xfa), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubsb, LEG(NP, 0F, 0, 0xe8), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubsw, LEG(NP, 0F, 0, 0xe9), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubusb, LEG(NP, 0F, 0, 0xd8), MMX, WRR, Pq, Pq, Qq)
OPCODE(psubusw, LEG(NP, 0F, 0, 0xd9), MMX, WRR, Pq, Pq, Qq)
OPCODE(subps, LEG(NP, 0F, 0, 0x5c), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(subss, LEG(F3, 0F, 0, 0x5c), SSE, WRR, Vd, Vd, Wd)
OPCODE(pmullw, LEG(NP, 0F, 0, 0xd5), MMX, WRR, Pq, Pq, Qq)
OPCODE(pmulhw, LEG(NP, 0F, 0, 0xe5), MMX, WRR, Pq, Pq, Qq)
OPCODE(pmulhuw, LEG(NP, 0F, 0, 0xe4), SSE, WRR, Pq, Pq, Qq)
OPCODE(mulps, LEG(NP, 0F, 0, 0x59), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(mulss, LEG(F3, 0F, 0, 0x59), SSE, WRR, Vd, Vd, Wd)
OPCODE(pmaddwd, LEG(NP, 0F, 0, 0xf5), MMX, WRR, Pq, Pq, Qq)
OPCODE(divps, LEG(NP, 0F, 0, 0x5e), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(divss, LEG(F3, 0F, 0, 0x5e), SSE, WRR, Vd, Vd, Wd)
OPCODE(rcpps, LEG(NP, 0F, 0, 0x53), SSE, WR, Vdq, Wdq)
OPCODE(rcpss, LEG(F3, 0F, 0, 0x53), SSE, WR, Vd, Wd)
OPCODE(sqrtps, LEG(NP, 0F, 0, 0x51), SSE, WR, Vdq, Wdq)
OPCODE(sqrtss, LEG(F3, 0F, 0, 0x51), SSE, WR, Vd, Wd)
OPCODE(rsqrtps, LEG(NP, 0F, 0, 0x52), SSE, WR, Vdq, Wdq)
OPCODE(rsqrtss, LEG(F3, 0F, 0, 0x52), SSE, WR, Vd, Wd)
OPCODE(pminub, LEG(NP, 0F, 0, 0xda), SSE, WRR, Pq, Pq, Qq)
OPCODE(pminsw, LEG(NP, 0F, 0, 0xea), SSE, WRR, Pq, Pq, Qq)
OPCODE(minps, LEG(NP, 0F, 0, 0x5d), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(minss, LEG(F3, 0F, 0, 0x5d), SSE, WRR, Vd, Vd, Wd)
OPCODE(pmaxub, LEG(NP, 0F, 0, 0xde), SSE, WRR, Pq, Pq, Qq)
OPCODE(pmaxsw, LEG(NP, 0F, 0, 0xee), SSE, WRR, Pq, Pq, Qq)
OPCODE(maxps, LEG(NP, 0F, 0, 0x5f), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(maxss, LEG(F3, 0F, 0, 0x5f), SSE, WRR, Vd, Vd, Wd)
OPCODE(pavgb, LEG(NP, 0F, 0, 0xe0), SSE, WRR, Pq, Pq, Qq)
OPCODE(pavgw, LEG(NP, 0F, 0, 0xe3), SSE, WRR, Pq, Pq, Qq)
OPCODE(psadbw, LEG(NP, 0F, 0, 0xf6), SSE, WRR, Pq, Pq, Qq)
OPCODE(pcmpeqb, LEG(NP, 0F, 0, 0x74), MMX, WRR, Pq, Pq, Qq)
OPCODE(pcmpeqw, LEG(NP, 0F, 0, 0x75), MMX, WRR, Pq, Pq, Qq)
OPCODE(pcmpeqd, LEG(NP, 0F, 0, 0x76), MMX, WRR, Pq, Pq, Qq)
OPCODE(pcmpgtb, LEG(NP, 0F, 0, 0x64), MMX, WRR, Pq, Pq, Qq)
OPCODE(pcmpgtw, LEG(NP, 0F, 0, 0x65), MMX, WRR, Pq, Pq, Qq)
OPCODE(pcmpgtd, LEG(NP, 0F, 0, 0x66), MMX, WRR, Pq, Pq, Qq)
OPCODE(cmpps, LEG(NP, 0F, 0, 0xc2), SSE, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(cmpss, LEG(F3, 0F, 0, 0xc2), SSE, WRRR, Vd, Vd, Wd, Ib)
OPCODE(ucomiss, LEG(NP, 0F, 0, 0x2e), SSE, RR, Vd, Wd)
OPCODE(comiss, LEG(NP, 0F, 0, 0x2f), SSE, RR, Vd, Wd)
OPCODE(pand, LEG(NP, 0F, 0, 0xdb), MMX, WRR, Pq, Pq, Qq)
OPCODE(andps, LEG(NP, 0F, 0, 0x54), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(pandn, LEG(NP, 0F, 0, 0xdf), MMX, WRR, Pq, Pq, Qq)
OPCODE(andnps, LEG(NP, 0F, 0, 0x55), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(por, LEG(NP, 0F, 0, 0xeb), MMX, WRR, Pq, Pq, Qq)
OPCODE(orps, LEG(NP, 0F, 0, 0x56), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(pxor, LEG(NP, 0F, 0, 0xef), MMX, WRR, Pq, Pq, Qq)
OPCODE(xorps, LEG(NP, 0F, 0, 0x57), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(psllw, LEG(NP, 0F, 0, 0xf1), MMX, WRR, Pq, Pq, Qq)
OPCODE(pslld, LEG(NP, 0F, 0, 0xf2), MMX, WRR, Pq, Pq, Qq)
OPCODE(psllq, LEG(NP, 0F, 0, 0xf3), MMX, WRR, Pq, Pq, Qq)
OPCODE(psrlw, LEG(NP, 0F, 0, 0xd1), MMX, WRR, Pq, Pq, Qq)
OPCODE(psrld, LEG(NP, 0F, 0, 0xd2), MMX, WRR, Pq, Pq, Qq)
OPCODE(psrlq, LEG(NP, 0F, 0, 0xd3), MMX, WRR, Pq, Pq, Qq)
OPCODE(psraw, LEG(NP, 0F, 0, 0xe1), MMX, WRR, Pq, Pq, Qq)
OPCODE(psrad, LEG(NP, 0F, 0, 0xe2), MMX, WRR, Pq, Pq, Qq)
OPCODE(packsswb, LEG(NP, 0F, 0, 0x63), MMX, WRR, Pq, Pq, Qq)
OPCODE(packssdw, LEG(NP, 0F, 0, 0x6b), MMX, WRR, Pq, Pq, Qq)
OPCODE(packuswb, LEG(NP, 0F, 0, 0x67), MMX, WRR, Pq, Pq, Qq)
OPCODE(punpckhbw, LEG(NP, 0F, 0, 0x68), MMX, WRR, Pq, Pq, Qq)
OPCODE(punpckhwd, LEG(NP, 0F, 0, 0x69), MMX, WRR, Pq, Pq, Qq)
OPCODE(punpckhdq, LEG(NP, 0F, 0, 0x6a), MMX, WRR, Pq, Pq, Qq)
OPCODE(punpcklbw, LEG(NP, 0F, 0, 0x60), MMX, WRR, Pq, Pq, Qd)
OPCODE(punpcklwd, LEG(NP, 0F, 0, 0x61), MMX, WRR, Pq, Pq, Qd)
OPCODE(punpckldq, LEG(NP, 0F, 0, 0x62), MMX, WRR, Pq, Pq, Qd)
OPCODE(unpcklps, LEG(NP, 0F, 0, 0x14), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(unpckhps, LEG(NP, 0F, 0, 0x15), SSE, WRR, Vdq, Vdq, Wdq)
OPCODE(pshufw, LEG(NP, 0F, 0, 0x70), SSE, WRR, Pq, Qq, Ib)
OPCODE(shufps, LEG(NP, 0F, 0, 0xc6), SSE, WRRR, Vdq, Vdq, Wdq, Ib)
OPCODE(pinsrw, LEG(NP, 0F, 0, 0xc4), SSE, WRRR, Pq, Pq, RdMw, Ib)
OPCODE(pextrw, LEG(NP, 0F, 0, 0xc5), SSE, WRR, Gd, Nq, Ib)
OPCODE(pextrw, LEG(NP, 0F, 1, 0xc5), SSE, WRR, Gq, Nq, Ib)
OPCODE(cvtpi2ps, LEG(NP, 0F, 0, 0x2a), SSE, WR, Vdq, Qq)
OPCODE(cvtsi2ss, LEG(F3, 0F, 0, 0x2a), SSE, WR, Vd, Ed)
OPCODE(cvtsi2ss, LEG(F3, 0F, 1, 0x2a), SSE, WR, Vd, Eq)
OPCODE(cvtps2pi, LEG(NP, 0F, 0, 0x2d), SSE, WR, Pq, Wq)
OPCODE(cvtss2si, LEG(F3, 0F, 0, 0x2d), SSE, WR, Gd, Wd)
OPCODE(cvtss2si, LEG(F3, 0F, 1, 0x2d), SSE, WR, Gq, Wd)
OPCODE(cvttps2pi, LEG(NP, 0F, 0, 0x2c), SSE, WR, Pq, Wq)
OPCODE(cvttss2si, LEG(F3, 0F, 0, 0x2c), SSE, WR, Gd, Wd)
OPCODE(cvttss2si, LEG(F3, 0F, 1, 0x2c), SSE, WR, Gq, Wd)
OPCODE(maskmovq, LEG(NP, 0F, 0, 0xf7), SSE, RR, Pq, Nq)
OPCODE(movntps, LEG(NP, 0F, 0, 0x2b), SSE, WR, Mdq, Vdq)
OPCODE(movntq, LEG(NP, 0F, 0, 0xe7), SSE, WR, Mq, Pq)
OPCODE(emms, LEG(NP, 0F, 0, 0x77), MMX, )

OPCODE_GRP(grp12_LEG_NP, LEG(NP, 0F, 0, 0x71))
OPCODE_GRP_BEGIN(grp12_LEG_NP)
    OPCODE_GRPMEMB(grp12_LEG_NP, psllw, 6, MMX, WRR, Nq, Nq, Ib)
    OPCODE_GRPMEMB(grp12_LEG_NP, psrlw, 2, MMX, WRR, Nq, Nq, Ib)
    OPCODE_GRPMEMB(grp12_LEG_NP, psraw, 4, MMX, WRR, Nq, Nq, Ib)
OPCODE_GRP_END(grp12_LEG_NP)

OPCODE_GRP(grp13_LEG_NP, LEG(NP, 0F, 0, 0x72))
OPCODE_GRP_BEGIN(grp13_LEG_NP)
    OPCODE_GRPMEMB(grp13_LEG_NP, pslld, 6, MMX, WRR, Nq, Nq, Ib)
    OPCODE_GRPMEMB(grp13_LEG_NP, psrld, 2, MMX, WRR, Nq, Nq, Ib)
    OPCODE_GRPMEMB(grp13_LEG_NP, psrad, 4, MMX, WRR, Nq, Nq, Ib)
OPCODE_GRP_END(grp13_LEG_NP)

OPCODE_GRP(grp14_LEG_NP, LEG(NP, 0F, 0, 0x73))
OPCODE_GRP_BEGIN(grp14_LEG_NP)
    OPCODE_GRPMEMB(grp14_LEG_NP, psllq, 6, MMX, WRR, Nq, Nq, Ib)
    OPCODE_GRPMEMB(grp14_LEG_NP, psrlq, 2, MMX, WRR, Nq, Nq, Ib)
OPCODE_GRP_END(grp14_LEG_NP)

OPCODE_GRP(grp15_LEG_NP, LEG(NP, 0F, 0, 0xae))
OPCODE_GRP_BEGIN(grp15_LEG_NP)
    OPCODE_GRPMEMB(grp15_LEG_NP, sfence, 7, SSE, )
    OPCODE_GRPMEMB(grp15_LEG_NP, ldmxcsr, 2, SSE, R, Md)
    OPCODE_GRPMEMB(grp15_LEG_NP, stmxcsr, 3, SSE, W, Md)
OPCODE_GRP_END(grp15_LEG_NP)

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
