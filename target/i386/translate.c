/*
 *  i386 translation
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "qemu/osdep.h"

#include "qemu/host-utils.h"
#include "cpu.h"
#include "disas/disas.h"
#include "exec/exec-all.h"
#include "tcg-gvec-desc.h"
#include "tcg-op.h"
#include "tcg-op-gvec.h"
#include "exec/cpu_ldst.h"
#include "exec/translator.h"

#include "exec/helper-proto.h"
#include "exec/helper-gen.h"

#include "trace-tcg.h"
#include "exec/log.h"

#define PREFIX_REPZ   0x01
#define PREFIX_REPNZ  0x02
#define PREFIX_LOCK   0x04
#define PREFIX_DATA   0x08
#define PREFIX_ADR    0x10
#define PREFIX_VEX    0x20

#ifdef TARGET_X86_64
#define CODE64(s) ((s)->code64)
#define REX_X(s) ((s)->rex_x)
#define REX_B(s) ((s)->rex_b)
#define REX_R(s) ((s)->rex_r)
#define REX_W(s) ((s)->rex_w)
#else
#define CODE64(s) 0
#define REX_X(s) 0
#define REX_B(s) 0
#define REX_R(s) 0
#define REX_W(s) -1
#endif

#ifdef TARGET_X86_64
# define ctztl  ctz64
# define clztl  clz64
#else
# define ctztl  ctz32
# define clztl  clz32
#endif

/* For a switch indexed by MODRM, match all memory operands for a given OP.  */
#define CASE_MODRM_MEM_OP(OP) \
    case (0 << 6) | (OP << 3) | 0 ... (0 << 6) | (OP << 3) | 7: \
    case (1 << 6) | (OP << 3) | 0 ... (1 << 6) | (OP << 3) | 7: \
    case (2 << 6) | (OP << 3) | 0 ... (2 << 6) | (OP << 3) | 7

#define CASE_MODRM_OP(OP) \
    case (0 << 6) | (OP << 3) | 0 ... (0 << 6) | (OP << 3) | 7: \
    case (1 << 6) | (OP << 3) | 0 ... (1 << 6) | (OP << 3) | 7: \
    case (2 << 6) | (OP << 3) | 0 ... (2 << 6) | (OP << 3) | 7: \
    case (3 << 6) | (OP << 3) | 0 ... (3 << 6) | (OP << 3) | 7

//#define MACRO_TEST   1

/* global register indexes */
static TCGv cpu_cc_dst, cpu_cc_src, cpu_cc_src2;
static TCGv_i32 cpu_cc_op;
static TCGv cpu_regs[CPU_NB_REGS];
static TCGv cpu_seg_base[6];
static TCGv_i64 cpu_bndl[4];
static TCGv_i64 cpu_bndu[4];

#include "exec/gen-icount.h"

typedef struct DisasContext {
    DisasContextBase base;

    /* current insn context */
    int override; /* -1 if no override */
    int prefix;
    TCGMemOp aflag;
    TCGMemOp dflag;
    target_ulong pc_start;
    target_ulong pc; /* pc = eip + cs_base */
    /* current block context */
    target_ulong cs_base; /* base of CS segment */
    int pe;     /* protected mode */
    int code32; /* 32 bit code segment */
#ifdef TARGET_X86_64
    int lma;    /* long mode active */
    int code64; /* 64 bit code segment */
    int rex_x, rex_b, rex_r, rex_w;
#endif
    int vex_l;  /* vex vector length */
    int vex_v;  /* vex vvvv register, without 1's complement.  */
    int ss32;   /* 32 bit stack segment */
    CCOp cc_op;  /* current CC operation */
    bool cc_op_dirty;
#ifdef TARGET_X86_64
    bool x86_64_hregs;
#endif
    int addseg; /* non zero if either DS/ES/SS have a non zero base */
    int f_st;   /* currently unused */
    int vm86;   /* vm86 mode */
    int cpl;
    int iopl;
    int tf;     /* TF cpu flag */
    int jmp_opt; /* use direct block chaining for direct jumps */
    int repz_opt; /* optimize jumps within repz instructions */
    int mem_index; /* select memory access functions */
    uint64_t flags; /* all execution flags */
    int popl_esp_hack; /* for correct popl with esp base handling */
    int rip_offset; /* only used in x86_64, but left for simplicity */
    int cpuid_features;
    int cpuid_ext_features;
    int cpuid_ext2_features;
    int cpuid_ext3_features;
    int cpuid_7_0_ebx_features;
    int cpuid_xsave_features;

    /* TCG local temps */
    TCGv cc_srcT;
    TCGv A0;
    TCGv T0;
    TCGv T1;

    /* TCG local register indexes (only used inside old micro ops) */
    TCGv tmp0;
    TCGv tmp4;
    TCGv_ptr ptr0;
    TCGv_ptr ptr1;
    TCGv_i32 tmp2_i32;
    TCGv_i32 tmp3_i32;
    TCGv_i64 tmp1_i64;

    sigjmp_buf jmpbuf;
} DisasContext;

static void gen_eob(DisasContext *s);
static void gen_jr(DisasContext *s, TCGv dest);
static void gen_jmp(DisasContext *s, target_ulong eip);
static void gen_jmp_tb(DisasContext *s, target_ulong eip, int tb_num);
static void gen_op(DisasContext *s1, int op, TCGMemOp ot, int d);

/* i386 arith/logic operations */
enum {
    OP_ADDL,
    OP_ORL,
    OP_ADCL,
    OP_SBBL,
    OP_ANDL,
    OP_SUBL,
    OP_XORL,
    OP_CMPL,
};

/* i386 shift ops */
enum {
    OP_ROL,
    OP_ROR,
    OP_RCL,
    OP_RCR,
    OP_SHL,
    OP_SHR,
    OP_SHL1, /* undocumented */
    OP_SAR = 7,
};

enum {
    JCC_O,
    JCC_B,
    JCC_Z,
    JCC_BE,
    JCC_S,
    JCC_P,
    JCC_L,
    JCC_LE,
};

enum {
    /* I386 int registers */
    OR_EAX,   /* MUST be even numbered */
    OR_ECX,
    OR_EDX,
    OR_EBX,
    OR_ESP,
    OR_EBP,
    OR_ESI,
    OR_EDI,

    OR_TMP0 = 16,    /* temporary operand register */
    OR_TMP1,
    OR_A0, /* temporary register used when doing address evaluation */
};

enum {
    USES_CC_DST  = 1,
    USES_CC_SRC  = 2,
    USES_CC_SRC2 = 4,
    USES_CC_SRCT = 8,
};

/* Bit set if the global variable is live after setting CC_OP to X.  */
static const uint8_t cc_op_live[CC_OP_NB] = {
    [CC_OP_DYNAMIC] = USES_CC_DST | USES_CC_SRC | USES_CC_SRC2,
    [CC_OP_EFLAGS] = USES_CC_SRC,
    [CC_OP_MULB ... CC_OP_MULQ] = USES_CC_DST | USES_CC_SRC,
    [CC_OP_ADDB ... CC_OP_ADDQ] = USES_CC_DST | USES_CC_SRC,
    [CC_OP_ADCB ... CC_OP_ADCQ] = USES_CC_DST | USES_CC_SRC | USES_CC_SRC2,
    [CC_OP_SUBB ... CC_OP_SUBQ] = USES_CC_DST | USES_CC_SRC | USES_CC_SRCT,
    [CC_OP_SBBB ... CC_OP_SBBQ] = USES_CC_DST | USES_CC_SRC | USES_CC_SRC2,
    [CC_OP_LOGICB ... CC_OP_LOGICQ] = USES_CC_DST,
    [CC_OP_INCB ... CC_OP_INCQ] = USES_CC_DST | USES_CC_SRC,
    [CC_OP_DECB ... CC_OP_DECQ] = USES_CC_DST | USES_CC_SRC,
    [CC_OP_SHLB ... CC_OP_SHLQ] = USES_CC_DST | USES_CC_SRC,
    [CC_OP_SARB ... CC_OP_SARQ] = USES_CC_DST | USES_CC_SRC,
    [CC_OP_BMILGB ... CC_OP_BMILGQ] = USES_CC_DST | USES_CC_SRC,
    [CC_OP_ADCX] = USES_CC_DST | USES_CC_SRC,
    [CC_OP_ADOX] = USES_CC_SRC | USES_CC_SRC2,
    [CC_OP_ADCOX] = USES_CC_DST | USES_CC_SRC | USES_CC_SRC2,
    [CC_OP_CLR] = 0,
    [CC_OP_POPCNT] = USES_CC_SRC,
};

static void set_cc_op(DisasContext *s, CCOp op)
{
    int dead;

    if (s->cc_op == op) {
        return;
    }

    /* Discard CC computation that will no longer be used.  */
    dead = cc_op_live[s->cc_op] & ~cc_op_live[op];
    if (dead & USES_CC_DST) {
        tcg_gen_discard_tl(cpu_cc_dst);
    }
    if (dead & USES_CC_SRC) {
        tcg_gen_discard_tl(cpu_cc_src);
    }
    if (dead & USES_CC_SRC2) {
        tcg_gen_discard_tl(cpu_cc_src2);
    }
    if (dead & USES_CC_SRCT) {
        tcg_gen_discard_tl(s->cc_srcT);
    }

    if (op == CC_OP_DYNAMIC) {
        /* The DYNAMIC setting is translator only, and should never be
           stored.  Thus we always consider it clean.  */
        s->cc_op_dirty = false;
    } else {
        /* Discard any computed CC_OP value (see shifts).  */
        if (s->cc_op == CC_OP_DYNAMIC) {
            tcg_gen_discard_i32(cpu_cc_op);
        }
        s->cc_op_dirty = true;
    }
    s->cc_op = op;
}

static void gen_update_cc_op(DisasContext *s)
{
    if (s->cc_op_dirty) {
        tcg_gen_movi_i32(cpu_cc_op, s->cc_op);
        s->cc_op_dirty = false;
    }
}

#ifdef TARGET_X86_64

#define NB_OP_SIZES 4

#else /* !TARGET_X86_64 */

#define NB_OP_SIZES 3

#endif /* !TARGET_X86_64 */

#if defined(HOST_WORDS_BIGENDIAN)
#define REG_B_OFFSET (sizeof(target_ulong) - 1)
#define REG_H_OFFSET (sizeof(target_ulong) - 2)
#define REG_W_OFFSET (sizeof(target_ulong) - 2)
#define REG_L_OFFSET (sizeof(target_ulong) - 4)
#define REG_LH_OFFSET (sizeof(target_ulong) - 8)
#else
#define REG_B_OFFSET 0
#define REG_H_OFFSET 1
#define REG_W_OFFSET 0
#define REG_L_OFFSET 0
#define REG_LH_OFFSET 4
#endif

/* In instruction encodings for byte register accesses the
 * register number usually indicates "low 8 bits of register N";
 * however there are some special cases where N 4..7 indicates
 * [AH, CH, DH, BH], ie "bits 15..8 of register N-4". Return
 * true for this special case, false otherwise.
 */
static inline bool byte_reg_is_xH(DisasContext *s, int reg)
{
    if (reg < 4) {
        return false;
    }
#ifdef TARGET_X86_64
    if (reg >= 8 || s->x86_64_hregs) {
        return false;
    }
#endif
    return true;
}

/* Select the size of a push/pop operation.  */
static inline TCGMemOp mo_pushpop(DisasContext *s, TCGMemOp ot)
{
    if (CODE64(s)) {
        return ot == MO_16 ? MO_16 : MO_64;
    } else {
        return ot;
    }
}

/* Select the size of the stack pointer.  */
static inline TCGMemOp mo_stacksize(DisasContext *s)
{
    return CODE64(s) ? MO_64 : s->ss32 ? MO_32 : MO_16;
}

/* Select only size 64 else 32.  Used for SSE operand sizes.  */
static inline TCGMemOp mo_64_32(TCGMemOp ot)
{
#ifdef TARGET_X86_64
    return ot == MO_64 ? MO_64 : MO_32;
#else
    return MO_32;
#endif
}

/* Select size 8 if lsb of B is clear, else OT.  Used for decoding
   byte vs word opcodes.  */
static inline TCGMemOp mo_b_d(int b, TCGMemOp ot)
{
    return b & 1 ? ot : MO_8;
}

/* Select size 8 if lsb of B is clear, else OT capped at 32.
   Used for decoding operand size of port opcodes.  */
static inline TCGMemOp mo_b_d32(int b, TCGMemOp ot)
{
    return b & 1 ? (ot == MO_16 ? MO_16 : MO_32) : MO_8;
}

static void gen_op_mov_reg_v(DisasContext *s, TCGMemOp ot, int reg, TCGv t0)
{
    switch(ot) {
    case MO_8:
        if (!byte_reg_is_xH(s, reg)) {
            tcg_gen_deposit_tl(cpu_regs[reg], cpu_regs[reg], t0, 0, 8);
        } else {
            tcg_gen_deposit_tl(cpu_regs[reg - 4], cpu_regs[reg - 4], t0, 8, 8);
        }
        break;
    case MO_16:
        tcg_gen_deposit_tl(cpu_regs[reg], cpu_regs[reg], t0, 0, 16);
        break;
    case MO_32:
        /* For x86_64, this sets the higher half of register to zero.
           For i386, this is equivalent to a mov. */
        tcg_gen_ext32u_tl(cpu_regs[reg], t0);
        break;
#ifdef TARGET_X86_64
    case MO_64:
        tcg_gen_mov_tl(cpu_regs[reg], t0);
        break;
#endif
    default:
        tcg_abort();
    }
}

static inline
void gen_op_mov_v_reg(DisasContext *s, TCGMemOp ot, TCGv t0, int reg)
{
    if (ot == MO_8 && byte_reg_is_xH(s, reg)) {
        tcg_gen_extract_tl(t0, cpu_regs[reg - 4], 8, 8);
    } else {
        tcg_gen_mov_tl(t0, cpu_regs[reg]);
    }
}

static void gen_add_A0_im(DisasContext *s, int val)
{
    tcg_gen_addi_tl(s->A0, s->A0, val);
    if (!CODE64(s)) {
        tcg_gen_ext32u_tl(s->A0, s->A0);
    }
}

static inline void gen_op_jmp_v(TCGv dest)
{
    tcg_gen_st_tl(dest, cpu_env, offsetof(CPUX86State, eip));
}

static inline
void gen_op_add_reg_im(DisasContext *s, TCGMemOp size, int reg, int32_t val)
{
    tcg_gen_addi_tl(s->tmp0, cpu_regs[reg], val);
    gen_op_mov_reg_v(s, size, reg, s->tmp0);
}

static inline void gen_op_add_reg_T0(DisasContext *s, TCGMemOp size, int reg)
{
    tcg_gen_add_tl(s->tmp0, cpu_regs[reg], s->T0);
    gen_op_mov_reg_v(s, size, reg, s->tmp0);
}

static inline void gen_op_ld_v(DisasContext *s, int idx, TCGv t0, TCGv a0)
{
    tcg_gen_qemu_ld_tl(t0, a0, s->mem_index, idx | MO_LE);
}

static inline void gen_op_st_v(DisasContext *s, int idx, TCGv t0, TCGv a0)
{
    tcg_gen_qemu_st_tl(t0, a0, s->mem_index, idx | MO_LE);
}

static inline void gen_op_st_rm_T0_A0(DisasContext *s, int idx, int d)
{
    if (d == OR_TMP0) {
        gen_op_st_v(s, idx, s->T0, s->A0);
    } else {
        gen_op_mov_reg_v(s, idx, d, s->T0);
    }
}

static inline void gen_jmp_im(DisasContext *s, target_ulong pc)
{
    tcg_gen_movi_tl(s->tmp0, pc);
    gen_op_jmp_v(s->tmp0);
}

/* Compute SEG:REG into A0.  SEG is selected from the override segment
   (OVR_SEG) and the default segment (DEF_SEG).  OVR_SEG may be -1 to
   indicate no override.  */
static void gen_lea_v_seg(DisasContext *s, TCGMemOp aflag, TCGv a0,
                          int def_seg, int ovr_seg)
{
    switch (aflag) {
#ifdef TARGET_X86_64
    case MO_64:
        if (ovr_seg < 0) {
            tcg_gen_mov_tl(s->A0, a0);
            return;
        }
        break;
#endif
    case MO_32:
        /* 32 bit address */
        if (ovr_seg < 0 && s->addseg) {
            ovr_seg = def_seg;
        }
        if (ovr_seg < 0) {
            tcg_gen_ext32u_tl(s->A0, a0);
            return;
        }
        break;
    case MO_16:
        /* 16 bit address */
        tcg_gen_ext16u_tl(s->A0, a0);
        a0 = s->A0;
        if (ovr_seg < 0) {
            if (s->addseg) {
                ovr_seg = def_seg;
            } else {
                return;
            }
        }
        break;
    default:
        tcg_abort();
    }

    if (ovr_seg >= 0) {
        TCGv seg = cpu_seg_base[ovr_seg];

        if (aflag == MO_64) {
            tcg_gen_add_tl(s->A0, a0, seg);
        } else if (CODE64(s)) {
            tcg_gen_ext32u_tl(s->A0, a0);
            tcg_gen_add_tl(s->A0, s->A0, seg);
        } else {
            tcg_gen_add_tl(s->A0, a0, seg);
            tcg_gen_ext32u_tl(s->A0, s->A0);
        }
    }
}

static inline void gen_string_movl_A0_ESI(DisasContext *s)
{
    gen_lea_v_seg(s, s->aflag, cpu_regs[R_ESI], R_DS, s->override);
}

static inline void gen_string_movl_A0_EDI(DisasContext *s)
{
    gen_lea_v_seg(s, s->aflag, cpu_regs[R_EDI], R_ES, -1);
}

static inline void gen_op_movl_T0_Dshift(DisasContext *s, TCGMemOp ot)
{
    tcg_gen_ld32s_tl(s->T0, cpu_env, offsetof(CPUX86State, df));
    tcg_gen_shli_tl(s->T0, s->T0, ot);
};

static TCGv gen_ext_tl(TCGv dst, TCGv src, TCGMemOp size, bool sign)
{
    switch (size) {
    case MO_8:
        if (sign) {
            tcg_gen_ext8s_tl(dst, src);
        } else {
            tcg_gen_ext8u_tl(dst, src);
        }
        return dst;
    case MO_16:
        if (sign) {
            tcg_gen_ext16s_tl(dst, src);
        } else {
            tcg_gen_ext16u_tl(dst, src);
        }
        return dst;
#ifdef TARGET_X86_64
    case MO_32:
        if (sign) {
            tcg_gen_ext32s_tl(dst, src);
        } else {
            tcg_gen_ext32u_tl(dst, src);
        }
        return dst;
#endif
    default:
        return src;
    }
}

static void gen_extu(TCGMemOp ot, TCGv reg)
{
    gen_ext_tl(reg, reg, ot, false);
}

static void gen_exts(TCGMemOp ot, TCGv reg)
{
    gen_ext_tl(reg, reg, ot, true);
}

static inline
void gen_op_jnz_ecx(DisasContext *s, TCGMemOp size, TCGLabel *label1)
{
    tcg_gen_mov_tl(s->tmp0, cpu_regs[R_ECX]);
    gen_extu(size, s->tmp0);
    tcg_gen_brcondi_tl(TCG_COND_NE, s->tmp0, 0, label1);
}

static inline
void gen_op_jz_ecx(DisasContext *s, TCGMemOp size, TCGLabel *label1)
{
    tcg_gen_mov_tl(s->tmp0, cpu_regs[R_ECX]);
    gen_extu(size, s->tmp0);
    tcg_gen_brcondi_tl(TCG_COND_EQ, s->tmp0, 0, label1);
}

static void gen_helper_in_func(TCGMemOp ot, TCGv v, TCGv_i32 n)
{
    switch (ot) {
    case MO_8:
        gen_helper_inb(v, cpu_env, n);
        break;
    case MO_16:
        gen_helper_inw(v, cpu_env, n);
        break;
    case MO_32:
        gen_helper_inl(v, cpu_env, n);
        break;
    default:
        tcg_abort();
    }
}

static void gen_helper_out_func(TCGMemOp ot, TCGv_i32 v, TCGv_i32 n)
{
    switch (ot) {
    case MO_8:
        gen_helper_outb(cpu_env, v, n);
        break;
    case MO_16:
        gen_helper_outw(cpu_env, v, n);
        break;
    case MO_32:
        gen_helper_outl(cpu_env, v, n);
        break;
    default:
        tcg_abort();
    }
}

static void gen_check_io(DisasContext *s, TCGMemOp ot, target_ulong cur_eip,
                         uint32_t svm_flags)
{
    target_ulong next_eip;

    if (s->pe && (s->cpl > s->iopl || s->vm86)) {
        tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
        switch (ot) {
        case MO_8:
            gen_helper_check_iob(cpu_env, s->tmp2_i32);
            break;
        case MO_16:
            gen_helper_check_iow(cpu_env, s->tmp2_i32);
            break;
        case MO_32:
            gen_helper_check_iol(cpu_env, s->tmp2_i32);
            break;
        default:
            tcg_abort();
        }
    }
    if(s->flags & HF_GUEST_MASK) {
        gen_update_cc_op(s);
        gen_jmp_im(s, cur_eip);
        svm_flags |= (1 << (4 + ot));
        next_eip = s->pc - s->cs_base;
        tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
        gen_helper_svm_check_io(cpu_env, s->tmp2_i32,
                                tcg_const_i32(svm_flags),
                                tcg_const_i32(next_eip - cur_eip));
    }
}

static inline void gen_movs(DisasContext *s, TCGMemOp ot)
{
    gen_string_movl_A0_ESI(s);
    gen_op_ld_v(s, ot, s->T0, s->A0);
    gen_string_movl_A0_EDI(s);
    gen_op_st_v(s, ot, s->T0, s->A0);
    gen_op_movl_T0_Dshift(s, ot);
    gen_op_add_reg_T0(s, s->aflag, R_ESI);
    gen_op_add_reg_T0(s, s->aflag, R_EDI);
}

static void gen_op_update1_cc(DisasContext *s)
{
    tcg_gen_mov_tl(cpu_cc_dst, s->T0);
}

static void gen_op_update2_cc(DisasContext *s)
{
    tcg_gen_mov_tl(cpu_cc_src, s->T1);
    tcg_gen_mov_tl(cpu_cc_dst, s->T0);
}

static void gen_op_update3_cc(DisasContext *s, TCGv reg)
{
    tcg_gen_mov_tl(cpu_cc_src2, reg);
    tcg_gen_mov_tl(cpu_cc_src, s->T1);
    tcg_gen_mov_tl(cpu_cc_dst, s->T0);
}

static inline void gen_op_testl_T0_T1_cc(DisasContext *s)
{
    tcg_gen_and_tl(cpu_cc_dst, s->T0, s->T1);
}

static void gen_op_update_neg_cc(DisasContext *s)
{
    tcg_gen_mov_tl(cpu_cc_dst, s->T0);
    tcg_gen_neg_tl(cpu_cc_src, s->T0);
    tcg_gen_movi_tl(s->cc_srcT, 0);
}

/* compute all eflags to cc_src */
static void gen_compute_eflags(DisasContext *s)
{
    TCGv zero, dst, src1, src2;
    int live, dead;

    if (s->cc_op == CC_OP_EFLAGS) {
        return;
    }
    if (s->cc_op == CC_OP_CLR) {
        tcg_gen_movi_tl(cpu_cc_src, CC_Z | CC_P);
        set_cc_op(s, CC_OP_EFLAGS);
        return;
    }

    zero = NULL;
    dst = cpu_cc_dst;
    src1 = cpu_cc_src;
    src2 = cpu_cc_src2;

    /* Take care to not read values that are not live.  */
    live = cc_op_live[s->cc_op] & ~USES_CC_SRCT;
    dead = live ^ (USES_CC_DST | USES_CC_SRC | USES_CC_SRC2);
    if (dead) {
        zero = tcg_const_tl(0);
        if (dead & USES_CC_DST) {
            dst = zero;
        }
        if (dead & USES_CC_SRC) {
            src1 = zero;
        }
        if (dead & USES_CC_SRC2) {
            src2 = zero;
        }
    }

    gen_update_cc_op(s);
    gen_helper_cc_compute_all(cpu_cc_src, dst, src1, src2, cpu_cc_op);
    set_cc_op(s, CC_OP_EFLAGS);

    if (dead) {
        tcg_temp_free(zero);
    }
}

typedef struct CCPrepare {
    TCGCond cond;
    TCGv reg;
    TCGv reg2;
    target_ulong imm;
    target_ulong mask;
    bool use_reg2;
    bool no_setcond;
} CCPrepare;

/* compute eflags.C to reg */
static CCPrepare gen_prepare_eflags_c(DisasContext *s, TCGv reg)
{
    TCGv t0, t1;
    int size, shift;

    switch (s->cc_op) {
    case CC_OP_SUBB ... CC_OP_SUBQ:
        /* (DATA_TYPE)CC_SRCT < (DATA_TYPE)CC_SRC */
        size = s->cc_op - CC_OP_SUBB;
        t1 = gen_ext_tl(s->tmp0, cpu_cc_src, size, false);
        /* If no temporary was used, be careful not to alias t1 and t0.  */
        t0 = t1 == cpu_cc_src ? s->tmp0 : reg;
        tcg_gen_mov_tl(t0, s->cc_srcT);
        gen_extu(size, t0);
        goto add_sub;

    case CC_OP_ADDB ... CC_OP_ADDQ:
        /* (DATA_TYPE)CC_DST < (DATA_TYPE)CC_SRC */
        size = s->cc_op - CC_OP_ADDB;
        t1 = gen_ext_tl(s->tmp0, cpu_cc_src, size, false);
        t0 = gen_ext_tl(reg, cpu_cc_dst, size, false);
    add_sub:
        return (CCPrepare) { .cond = TCG_COND_LTU, .reg = t0,
                             .reg2 = t1, .mask = -1, .use_reg2 = true };

    case CC_OP_LOGICB ... CC_OP_LOGICQ:
    case CC_OP_CLR:
    case CC_OP_POPCNT:
        return (CCPrepare) { .cond = TCG_COND_NEVER, .mask = -1 };

    case CC_OP_INCB ... CC_OP_INCQ:
    case CC_OP_DECB ... CC_OP_DECQ:
        return (CCPrepare) { .cond = TCG_COND_NE, .reg = cpu_cc_src,
                             .mask = -1, .no_setcond = true };

    case CC_OP_SHLB ... CC_OP_SHLQ:
        /* (CC_SRC >> (DATA_BITS - 1)) & 1 */
        size = s->cc_op - CC_OP_SHLB;
        shift = (8 << size) - 1;
        return (CCPrepare) { .cond = TCG_COND_NE, .reg = cpu_cc_src,
                             .mask = (target_ulong)1 << shift };

    case CC_OP_MULB ... CC_OP_MULQ:
        return (CCPrepare) { .cond = TCG_COND_NE,
                             .reg = cpu_cc_src, .mask = -1 };

    case CC_OP_BMILGB ... CC_OP_BMILGQ:
        size = s->cc_op - CC_OP_BMILGB;
        t0 = gen_ext_tl(reg, cpu_cc_src, size, false);
        return (CCPrepare) { .cond = TCG_COND_EQ, .reg = t0, .mask = -1 };

    case CC_OP_ADCX:
    case CC_OP_ADCOX:
        return (CCPrepare) { .cond = TCG_COND_NE, .reg = cpu_cc_dst,
                             .mask = -1, .no_setcond = true };

    case CC_OP_EFLAGS:
    case CC_OP_SARB ... CC_OP_SARQ:
        /* CC_SRC & 1 */
        return (CCPrepare) { .cond = TCG_COND_NE,
                             .reg = cpu_cc_src, .mask = CC_C };

    default:
       /* The need to compute only C from CC_OP_DYNAMIC is important
          in efficiently implementing e.g. INC at the start of a TB.  */
       gen_update_cc_op(s);
       gen_helper_cc_compute_c(reg, cpu_cc_dst, cpu_cc_src,
                               cpu_cc_src2, cpu_cc_op);
       return (CCPrepare) { .cond = TCG_COND_NE, .reg = reg,
                            .mask = -1, .no_setcond = true };
    }
}

/* compute eflags.P to reg */
static CCPrepare gen_prepare_eflags_p(DisasContext *s, TCGv reg)
{
    gen_compute_eflags(s);
    return (CCPrepare) { .cond = TCG_COND_NE, .reg = cpu_cc_src,
                         .mask = CC_P };
}

/* compute eflags.S to reg */
static CCPrepare gen_prepare_eflags_s(DisasContext *s, TCGv reg)
{
    switch (s->cc_op) {
    case CC_OP_DYNAMIC:
        gen_compute_eflags(s);
        /* FALLTHRU */
    case CC_OP_EFLAGS:
    case CC_OP_ADCX:
    case CC_OP_ADOX:
    case CC_OP_ADCOX:
        return (CCPrepare) { .cond = TCG_COND_NE, .reg = cpu_cc_src,
                             .mask = CC_S };
    case CC_OP_CLR:
    case CC_OP_POPCNT:
        return (CCPrepare) { .cond = TCG_COND_NEVER, .mask = -1 };
    default:
        {
            TCGMemOp size = (s->cc_op - CC_OP_ADDB) & 3;
            TCGv t0 = gen_ext_tl(reg, cpu_cc_dst, size, true);
            return (CCPrepare) { .cond = TCG_COND_LT, .reg = t0, .mask = -1 };
        }
    }
}

/* compute eflags.O to reg */
static CCPrepare gen_prepare_eflags_o(DisasContext *s, TCGv reg)
{
    switch (s->cc_op) {
    case CC_OP_ADOX:
    case CC_OP_ADCOX:
        return (CCPrepare) { .cond = TCG_COND_NE, .reg = cpu_cc_src2,
                             .mask = -1, .no_setcond = true };
    case CC_OP_CLR:
    case CC_OP_POPCNT:
        return (CCPrepare) { .cond = TCG_COND_NEVER, .mask = -1 };
    default:
        gen_compute_eflags(s);
        return (CCPrepare) { .cond = TCG_COND_NE, .reg = cpu_cc_src,
                             .mask = CC_O };
    }
}

/* compute eflags.Z to reg */
static CCPrepare gen_prepare_eflags_z(DisasContext *s, TCGv reg)
{
    switch (s->cc_op) {
    case CC_OP_DYNAMIC:
        gen_compute_eflags(s);
        /* FALLTHRU */
    case CC_OP_EFLAGS:
    case CC_OP_ADCX:
    case CC_OP_ADOX:
    case CC_OP_ADCOX:
        return (CCPrepare) { .cond = TCG_COND_NE, .reg = cpu_cc_src,
                             .mask = CC_Z };
    case CC_OP_CLR:
        return (CCPrepare) { .cond = TCG_COND_ALWAYS, .mask = -1 };
    case CC_OP_POPCNT:
        return (CCPrepare) { .cond = TCG_COND_EQ, .reg = cpu_cc_src,
                             .mask = -1 };
    default:
        {
            TCGMemOp size = (s->cc_op - CC_OP_ADDB) & 3;
            TCGv t0 = gen_ext_tl(reg, cpu_cc_dst, size, false);
            return (CCPrepare) { .cond = TCG_COND_EQ, .reg = t0, .mask = -1 };
        }
    }
}

/* perform a conditional store into register 'reg' according to jump opcode
   value 'b'. In the fast case, T0 is guaranted not to be used. */
static CCPrepare gen_prepare_cc(DisasContext *s, int b, TCGv reg)
{
    int inv, jcc_op, cond;
    TCGMemOp size;
    CCPrepare cc;
    TCGv t0;

    inv = b & 1;
    jcc_op = (b >> 1) & 7;

    switch (s->cc_op) {
    case CC_OP_SUBB ... CC_OP_SUBQ:
        /* We optimize relational operators for the cmp/jcc case.  */
        size = s->cc_op - CC_OP_SUBB;
        switch (jcc_op) {
        case JCC_BE:
            tcg_gen_mov_tl(s->tmp4, s->cc_srcT);
            gen_extu(size, s->tmp4);
            t0 = gen_ext_tl(s->tmp0, cpu_cc_src, size, false);
            cc = (CCPrepare) { .cond = TCG_COND_LEU, .reg = s->tmp4,
                               .reg2 = t0, .mask = -1, .use_reg2 = true };
            break;

        case JCC_L:
            cond = TCG_COND_LT;
            goto fast_jcc_l;
        case JCC_LE:
            cond = TCG_COND_LE;
        fast_jcc_l:
            tcg_gen_mov_tl(s->tmp4, s->cc_srcT);
            gen_exts(size, s->tmp4);
            t0 = gen_ext_tl(s->tmp0, cpu_cc_src, size, true);
            cc = (CCPrepare) { .cond = cond, .reg = s->tmp4,
                               .reg2 = t0, .mask = -1, .use_reg2 = true };
            break;

        default:
            goto slow_jcc;
        }
        break;

    default:
    slow_jcc:
        /* This actually generates good code for JC, JZ and JS.  */
        switch (jcc_op) {
        case JCC_O:
            cc = gen_prepare_eflags_o(s, reg);
            break;
        case JCC_B:
            cc = gen_prepare_eflags_c(s, reg);
            break;
        case JCC_Z:
            cc = gen_prepare_eflags_z(s, reg);
            break;
        case JCC_BE:
            gen_compute_eflags(s);
            cc = (CCPrepare) { .cond = TCG_COND_NE, .reg = cpu_cc_src,
                               .mask = CC_Z | CC_C };
            break;
        case JCC_S:
            cc = gen_prepare_eflags_s(s, reg);
            break;
        case JCC_P:
            cc = gen_prepare_eflags_p(s, reg);
            break;
        case JCC_L:
            gen_compute_eflags(s);
            if (reg == cpu_cc_src) {
                reg = s->tmp0;
            }
            tcg_gen_shri_tl(reg, cpu_cc_src, 4); /* CC_O -> CC_S */
            tcg_gen_xor_tl(reg, reg, cpu_cc_src);
            cc = (CCPrepare) { .cond = TCG_COND_NE, .reg = reg,
                               .mask = CC_S };
            break;
        default:
        case JCC_LE:
            gen_compute_eflags(s);
            if (reg == cpu_cc_src) {
                reg = s->tmp0;
            }
            tcg_gen_shri_tl(reg, cpu_cc_src, 4); /* CC_O -> CC_S */
            tcg_gen_xor_tl(reg, reg, cpu_cc_src);
            cc = (CCPrepare) { .cond = TCG_COND_NE, .reg = reg,
                               .mask = CC_S | CC_Z };
            break;
        }
        break;
    }

    if (inv) {
        cc.cond = tcg_invert_cond(cc.cond);
    }
    return cc;
}

static void gen_setcc1(DisasContext *s, int b, TCGv reg)
{
    CCPrepare cc = gen_prepare_cc(s, b, reg);

    if (cc.no_setcond) {
        if (cc.cond == TCG_COND_EQ) {
            tcg_gen_xori_tl(reg, cc.reg, 1);
        } else {
            tcg_gen_mov_tl(reg, cc.reg);
        }
        return;
    }

    if (cc.cond == TCG_COND_NE && !cc.use_reg2 && cc.imm == 0 &&
        cc.mask != 0 && (cc.mask & (cc.mask - 1)) == 0) {
        tcg_gen_shri_tl(reg, cc.reg, ctztl(cc.mask));
        tcg_gen_andi_tl(reg, reg, 1);
        return;
    }
    if (cc.mask != -1) {
        tcg_gen_andi_tl(reg, cc.reg, cc.mask);
        cc.reg = reg;
    }
    if (cc.use_reg2) {
        tcg_gen_setcond_tl(cc.cond, reg, cc.reg, cc.reg2);
    } else {
        tcg_gen_setcondi_tl(cc.cond, reg, cc.reg, cc.imm);
    }
}

static inline void gen_compute_eflags_c(DisasContext *s, TCGv reg)
{
    gen_setcc1(s, JCC_B << 1, reg);
}

/* generate a conditional jump to label 'l1' according to jump opcode
   value 'b'. In the fast case, T0 is guaranted not to be used. */
static inline void gen_jcc1_noeob(DisasContext *s, int b, TCGLabel *l1)
{
    CCPrepare cc = gen_prepare_cc(s, b, s->T0);

    if (cc.mask != -1) {
        tcg_gen_andi_tl(s->T0, cc.reg, cc.mask);
        cc.reg = s->T0;
    }
    if (cc.use_reg2) {
        tcg_gen_brcond_tl(cc.cond, cc.reg, cc.reg2, l1);
    } else {
        tcg_gen_brcondi_tl(cc.cond, cc.reg, cc.imm, l1);
    }
}

/* Generate a conditional jump to label 'l1' according to jump opcode
   value 'b'. In the fast case, T0 is guaranted not to be used.
   A translation block must end soon.  */
static inline void gen_jcc1(DisasContext *s, int b, TCGLabel *l1)
{
    CCPrepare cc = gen_prepare_cc(s, b, s->T0);

    gen_update_cc_op(s);
    if (cc.mask != -1) {
        tcg_gen_andi_tl(s->T0, cc.reg, cc.mask);
        cc.reg = s->T0;
    }
    set_cc_op(s, CC_OP_DYNAMIC);
    if (cc.use_reg2) {
        tcg_gen_brcond_tl(cc.cond, cc.reg, cc.reg2, l1);
    } else {
        tcg_gen_brcondi_tl(cc.cond, cc.reg, cc.imm, l1);
    }
}

/* XXX: does not work with gdbstub "ice" single step - not a
   serious problem */
static TCGLabel *gen_jz_ecx_string(DisasContext *s, target_ulong next_eip)
{
    TCGLabel *l1 = gen_new_label();
    TCGLabel *l2 = gen_new_label();
    gen_op_jnz_ecx(s, s->aflag, l1);
    gen_set_label(l2);
    gen_jmp_tb(s, next_eip, 1);
    gen_set_label(l1);
    return l2;
}

static inline void gen_stos(DisasContext *s, TCGMemOp ot)
{
    gen_op_mov_v_reg(s, MO_32, s->T0, R_EAX);
    gen_string_movl_A0_EDI(s);
    gen_op_st_v(s, ot, s->T0, s->A0);
    gen_op_movl_T0_Dshift(s, ot);
    gen_op_add_reg_T0(s, s->aflag, R_EDI);
}

static inline void gen_lods(DisasContext *s, TCGMemOp ot)
{
    gen_string_movl_A0_ESI(s);
    gen_op_ld_v(s, ot, s->T0, s->A0);
    gen_op_mov_reg_v(s, ot, R_EAX, s->T0);
    gen_op_movl_T0_Dshift(s, ot);
    gen_op_add_reg_T0(s, s->aflag, R_ESI);
}

static inline void gen_scas(DisasContext *s, TCGMemOp ot)
{
    gen_string_movl_A0_EDI(s);
    gen_op_ld_v(s, ot, s->T1, s->A0);
    gen_op(s, OP_CMPL, ot, R_EAX);
    gen_op_movl_T0_Dshift(s, ot);
    gen_op_add_reg_T0(s, s->aflag, R_EDI);
}

static inline void gen_cmps(DisasContext *s, TCGMemOp ot)
{
    gen_string_movl_A0_EDI(s);
    gen_op_ld_v(s, ot, s->T1, s->A0);
    gen_string_movl_A0_ESI(s);
    gen_op(s, OP_CMPL, ot, OR_TMP0);
    gen_op_movl_T0_Dshift(s, ot);
    gen_op_add_reg_T0(s, s->aflag, R_ESI);
    gen_op_add_reg_T0(s, s->aflag, R_EDI);
}

static void gen_bpt_io(DisasContext *s, TCGv_i32 t_port, int ot)
{
    if (s->flags & HF_IOBPT_MASK) {
        TCGv_i32 t_size = tcg_const_i32(1 << ot);
        TCGv t_next = tcg_const_tl(s->pc - s->cs_base);

        gen_helper_bpt_io(cpu_env, t_port, t_size, t_next);
        tcg_temp_free_i32(t_size);
        tcg_temp_free(t_next);
    }
}


static inline void gen_ins(DisasContext *s, TCGMemOp ot)
{
    if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
        gen_io_start();
    }
    gen_string_movl_A0_EDI(s);
    /* Note: we must do this dummy write first to be restartable in
       case of page fault. */
    tcg_gen_movi_tl(s->T0, 0);
    gen_op_st_v(s, ot, s->T0, s->A0);
    tcg_gen_trunc_tl_i32(s->tmp2_i32, cpu_regs[R_EDX]);
    tcg_gen_andi_i32(s->tmp2_i32, s->tmp2_i32, 0xffff);
    gen_helper_in_func(ot, s->T0, s->tmp2_i32);
    gen_op_st_v(s, ot, s->T0, s->A0);
    gen_op_movl_T0_Dshift(s, ot);
    gen_op_add_reg_T0(s, s->aflag, R_EDI);
    gen_bpt_io(s, s->tmp2_i32, ot);
    if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
        gen_io_end();
    }
}

static inline void gen_outs(DisasContext *s, TCGMemOp ot)
{
    if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
        gen_io_start();
    }
    gen_string_movl_A0_ESI(s);
    gen_op_ld_v(s, ot, s->T0, s->A0);

    tcg_gen_trunc_tl_i32(s->tmp2_i32, cpu_regs[R_EDX]);
    tcg_gen_andi_i32(s->tmp2_i32, s->tmp2_i32, 0xffff);
    tcg_gen_trunc_tl_i32(s->tmp3_i32, s->T0);
    gen_helper_out_func(ot, s->tmp2_i32, s->tmp3_i32);
    gen_op_movl_T0_Dshift(s, ot);
    gen_op_add_reg_T0(s, s->aflag, R_ESI);
    gen_bpt_io(s, s->tmp2_i32, ot);
    if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
        gen_io_end();
    }
}

/* same method as Valgrind : we generate jumps to current or next
   instruction */
#define GEN_REPZ(op)                                                          \
static inline void gen_repz_ ## op(DisasContext *s, TCGMemOp ot,              \
                                 target_ulong cur_eip, target_ulong next_eip) \
{                                                                             \
    TCGLabel *l2;                                                             \
    gen_update_cc_op(s);                                                      \
    l2 = gen_jz_ecx_string(s, next_eip);                                      \
    gen_ ## op(s, ot);                                                        \
    gen_op_add_reg_im(s, s->aflag, R_ECX, -1);                                \
    /* a loop would cause two single step exceptions if ECX = 1               \
       before rep string_insn */                                              \
    if (s->repz_opt)                                                          \
        gen_op_jz_ecx(s, s->aflag, l2);                                       \
    gen_jmp(s, cur_eip);                                                      \
}

#define GEN_REPZ2(op)                                                         \
static inline void gen_repz_ ## op(DisasContext *s, TCGMemOp ot,              \
                                   target_ulong cur_eip,                      \
                                   target_ulong next_eip,                     \
                                   int nz)                                    \
{                                                                             \
    TCGLabel *l2;                                                             \
    gen_update_cc_op(s);                                                      \
    l2 = gen_jz_ecx_string(s, next_eip);                                      \
    gen_ ## op(s, ot);                                                        \
    gen_op_add_reg_im(s, s->aflag, R_ECX, -1);                                \
    gen_update_cc_op(s);                                                      \
    gen_jcc1(s, (JCC_Z << 1) | (nz ^ 1), l2);                                 \
    if (s->repz_opt)                                                          \
        gen_op_jz_ecx(s, s->aflag, l2);                                       \
    gen_jmp(s, cur_eip);                                                      \
}

GEN_REPZ(movs)
GEN_REPZ(stos)
GEN_REPZ(lods)
GEN_REPZ(ins)
GEN_REPZ(outs)
GEN_REPZ2(scas)
GEN_REPZ2(cmps)

static void gen_helper_fp_arith_ST0_FT0(int op)
{
    switch (op) {
    case 0:
        gen_helper_fadd_ST0_FT0(cpu_env);
        break;
    case 1:
        gen_helper_fmul_ST0_FT0(cpu_env);
        break;
    case 2:
        gen_helper_fcom_ST0_FT0(cpu_env);
        break;
    case 3:
        gen_helper_fcom_ST0_FT0(cpu_env);
        break;
    case 4:
        gen_helper_fsub_ST0_FT0(cpu_env);
        break;
    case 5:
        gen_helper_fsubr_ST0_FT0(cpu_env);
        break;
    case 6:
        gen_helper_fdiv_ST0_FT0(cpu_env);
        break;
    case 7:
        gen_helper_fdivr_ST0_FT0(cpu_env);
        break;
    }
}

/* NOTE the exception in "r" op ordering */
static void gen_helper_fp_arith_STN_ST0(int op, int opreg)
{
    TCGv_i32 tmp = tcg_const_i32(opreg);
    switch (op) {
    case 0:
        gen_helper_fadd_STN_ST0(cpu_env, tmp);
        break;
    case 1:
        gen_helper_fmul_STN_ST0(cpu_env, tmp);
        break;
    case 4:
        gen_helper_fsubr_STN_ST0(cpu_env, tmp);
        break;
    case 5:
        gen_helper_fsub_STN_ST0(cpu_env, tmp);
        break;
    case 6:
        gen_helper_fdivr_STN_ST0(cpu_env, tmp);
        break;
    case 7:
        gen_helper_fdiv_STN_ST0(cpu_env, tmp);
        break;
    }
}

static void gen_exception(DisasContext *s, int trapno)
{
    gen_update_cc_op(s);
    gen_jmp_im(s, s->pc_start - s->cs_base);
    gen_helper_raise_exception(cpu_env, tcg_const_i32(trapno));
    s->base.is_jmp = DISAS_NORETURN;
}

/* Generate #UD for the current instruction.  The assumption here is that
   the instruction is known, but it isn't allowed in the current cpu mode.  */
static void gen_illegal_opcode(DisasContext *s)
{
    gen_exception(s, EXCP06_ILLOP);
}

/* if d == OR_TMP0, it means memory operand (address in A0) */
static void gen_op(DisasContext *s1, int op, TCGMemOp ot, int d)
{
    if (d != OR_TMP0) {
        if (s1->prefix & PREFIX_LOCK) {
            /* Lock prefix when destination is not memory.  */
            gen_illegal_opcode(s1);
            return;
        }
        gen_op_mov_v_reg(s1, ot, s1->T0, d);
    } else if (!(s1->prefix & PREFIX_LOCK)) {
        gen_op_ld_v(s1, ot, s1->T0, s1->A0);
    }
    switch(op) {
    case OP_ADCL:
        gen_compute_eflags_c(s1, s1->tmp4);
        if (s1->prefix & PREFIX_LOCK) {
            tcg_gen_add_tl(s1->T0, s1->tmp4, s1->T1);
            tcg_gen_atomic_add_fetch_tl(s1->T0, s1->A0, s1->T0,
                                        s1->mem_index, ot | MO_LE);
        } else {
            tcg_gen_add_tl(s1->T0, s1->T0, s1->T1);
            tcg_gen_add_tl(s1->T0, s1->T0, s1->tmp4);
            gen_op_st_rm_T0_A0(s1, ot, d);
        }
        gen_op_update3_cc(s1, s1->tmp4);
        set_cc_op(s1, CC_OP_ADCB + ot);
        break;
    case OP_SBBL:
        gen_compute_eflags_c(s1, s1->tmp4);
        if (s1->prefix & PREFIX_LOCK) {
            tcg_gen_add_tl(s1->T0, s1->T1, s1->tmp4);
            tcg_gen_neg_tl(s1->T0, s1->T0);
            tcg_gen_atomic_add_fetch_tl(s1->T0, s1->A0, s1->T0,
                                        s1->mem_index, ot | MO_LE);
        } else {
            tcg_gen_sub_tl(s1->T0, s1->T0, s1->T1);
            tcg_gen_sub_tl(s1->T0, s1->T0, s1->tmp4);
            gen_op_st_rm_T0_A0(s1, ot, d);
        }
        gen_op_update3_cc(s1, s1->tmp4);
        set_cc_op(s1, CC_OP_SBBB + ot);
        break;
    case OP_ADDL:
        if (s1->prefix & PREFIX_LOCK) {
            tcg_gen_atomic_add_fetch_tl(s1->T0, s1->A0, s1->T1,
                                        s1->mem_index, ot | MO_LE);
        } else {
            tcg_gen_add_tl(s1->T0, s1->T0, s1->T1);
            gen_op_st_rm_T0_A0(s1, ot, d);
        }
        gen_op_update2_cc(s1);
        set_cc_op(s1, CC_OP_ADDB + ot);
        break;
    case OP_SUBL:
        if (s1->prefix & PREFIX_LOCK) {
            tcg_gen_neg_tl(s1->T0, s1->T1);
            tcg_gen_atomic_fetch_add_tl(s1->cc_srcT, s1->A0, s1->T0,
                                        s1->mem_index, ot | MO_LE);
            tcg_gen_sub_tl(s1->T0, s1->cc_srcT, s1->T1);
        } else {
            tcg_gen_mov_tl(s1->cc_srcT, s1->T0);
            tcg_gen_sub_tl(s1->T0, s1->T0, s1->T1);
            gen_op_st_rm_T0_A0(s1, ot, d);
        }
        gen_op_update2_cc(s1);
        set_cc_op(s1, CC_OP_SUBB + ot);
        break;
    default:
    case OP_ANDL:
        if (s1->prefix & PREFIX_LOCK) {
            tcg_gen_atomic_and_fetch_tl(s1->T0, s1->A0, s1->T1,
                                        s1->mem_index, ot | MO_LE);
        } else {
            tcg_gen_and_tl(s1->T0, s1->T0, s1->T1);
            gen_op_st_rm_T0_A0(s1, ot, d);
        }
        gen_op_update1_cc(s1);
        set_cc_op(s1, CC_OP_LOGICB + ot);
        break;
    case OP_ORL:
        if (s1->prefix & PREFIX_LOCK) {
            tcg_gen_atomic_or_fetch_tl(s1->T0, s1->A0, s1->T1,
                                       s1->mem_index, ot | MO_LE);
        } else {
            tcg_gen_or_tl(s1->T0, s1->T0, s1->T1);
            gen_op_st_rm_T0_A0(s1, ot, d);
        }
        gen_op_update1_cc(s1);
        set_cc_op(s1, CC_OP_LOGICB + ot);
        break;
    case OP_XORL:
        if (s1->prefix & PREFIX_LOCK) {
            tcg_gen_atomic_xor_fetch_tl(s1->T0, s1->A0, s1->T1,
                                        s1->mem_index, ot | MO_LE);
        } else {
            tcg_gen_xor_tl(s1->T0, s1->T0, s1->T1);
            gen_op_st_rm_T0_A0(s1, ot, d);
        }
        gen_op_update1_cc(s1);
        set_cc_op(s1, CC_OP_LOGICB + ot);
        break;
    case OP_CMPL:
        tcg_gen_mov_tl(cpu_cc_src, s1->T1);
        tcg_gen_mov_tl(s1->cc_srcT, s1->T0);
        tcg_gen_sub_tl(cpu_cc_dst, s1->T0, s1->T1);
        set_cc_op(s1, CC_OP_SUBB + ot);
        break;
    }
}

/* if d == OR_TMP0, it means memory operand (address in A0) */
static void gen_inc(DisasContext *s1, TCGMemOp ot, int d, int c)
{
    if (s1->prefix & PREFIX_LOCK) {
        if (d != OR_TMP0) {
            /* Lock prefix when destination is not memory */
            gen_illegal_opcode(s1);
            return;
        }
        tcg_gen_movi_tl(s1->T0, c > 0 ? 1 : -1);
        tcg_gen_atomic_add_fetch_tl(s1->T0, s1->A0, s1->T0,
                                    s1->mem_index, ot | MO_LE);
    } else {
        if (d != OR_TMP0) {
            gen_op_mov_v_reg(s1, ot, s1->T0, d);
        } else {
            gen_op_ld_v(s1, ot, s1->T0, s1->A0);
        }
        tcg_gen_addi_tl(s1->T0, s1->T0, (c > 0 ? 1 : -1));
        gen_op_st_rm_T0_A0(s1, ot, d);
    }

    gen_compute_eflags_c(s1, cpu_cc_src);
    tcg_gen_mov_tl(cpu_cc_dst, s1->T0);
    set_cc_op(s1, (c > 0 ? CC_OP_INCB : CC_OP_DECB) + ot);
}

static void gen_shift_flags(DisasContext *s, TCGMemOp ot, TCGv result,
                            TCGv shm1, TCGv count, bool is_right)
{
    TCGv_i32 z32, s32, oldop;
    TCGv z_tl;

    /* Store the results into the CC variables.  If we know that the
       variable must be dead, store unconditionally.  Otherwise we'll
       need to not disrupt the current contents.  */
    z_tl = tcg_const_tl(0);
    if (cc_op_live[s->cc_op] & USES_CC_DST) {
        tcg_gen_movcond_tl(TCG_COND_NE, cpu_cc_dst, count, z_tl,
                           result, cpu_cc_dst);
    } else {
        tcg_gen_mov_tl(cpu_cc_dst, result);
    }
    if (cc_op_live[s->cc_op] & USES_CC_SRC) {
        tcg_gen_movcond_tl(TCG_COND_NE, cpu_cc_src, count, z_tl,
                           shm1, cpu_cc_src);
    } else {
        tcg_gen_mov_tl(cpu_cc_src, shm1);
    }
    tcg_temp_free(z_tl);

    /* Get the two potential CC_OP values into temporaries.  */
    tcg_gen_movi_i32(s->tmp2_i32, (is_right ? CC_OP_SARB : CC_OP_SHLB) + ot);
    if (s->cc_op == CC_OP_DYNAMIC) {
        oldop = cpu_cc_op;
    } else {
        tcg_gen_movi_i32(s->tmp3_i32, s->cc_op);
        oldop = s->tmp3_i32;
    }

    /* Conditionally store the CC_OP value.  */
    z32 = tcg_const_i32(0);
    s32 = tcg_temp_new_i32();
    tcg_gen_trunc_tl_i32(s32, count);
    tcg_gen_movcond_i32(TCG_COND_NE, cpu_cc_op, s32, z32, s->tmp2_i32, oldop);
    tcg_temp_free_i32(z32);
    tcg_temp_free_i32(s32);

    /* The CC_OP value is no longer predictable.  */
    set_cc_op(s, CC_OP_DYNAMIC);
}

static void gen_shift_rm_T1(DisasContext *s, TCGMemOp ot, int op1,
                            int is_right, int is_arith)
{
    target_ulong mask = (ot == MO_64 ? 0x3f : 0x1f);

    /* load */
    if (op1 == OR_TMP0) {
        gen_op_ld_v(s, ot, s->T0, s->A0);
    } else {
        gen_op_mov_v_reg(s, ot, s->T0, op1);
    }

    tcg_gen_andi_tl(s->T1, s->T1, mask);
    tcg_gen_subi_tl(s->tmp0, s->T1, 1);

    if (is_right) {
        if (is_arith) {
            gen_exts(ot, s->T0);
            tcg_gen_sar_tl(s->tmp0, s->T0, s->tmp0);
            tcg_gen_sar_tl(s->T0, s->T0, s->T1);
        } else {
            gen_extu(ot, s->T0);
            tcg_gen_shr_tl(s->tmp0, s->T0, s->tmp0);
            tcg_gen_shr_tl(s->T0, s->T0, s->T1);
        }
    } else {
        tcg_gen_shl_tl(s->tmp0, s->T0, s->tmp0);
        tcg_gen_shl_tl(s->T0, s->T0, s->T1);
    }

    /* store */
    gen_op_st_rm_T0_A0(s, ot, op1);

    gen_shift_flags(s, ot, s->T0, s->tmp0, s->T1, is_right);
}

static void gen_shift_rm_im(DisasContext *s, TCGMemOp ot, int op1, int op2,
                            int is_right, int is_arith)
{
    int mask = (ot == MO_64 ? 0x3f : 0x1f);

    /* load */
    if (op1 == OR_TMP0)
        gen_op_ld_v(s, ot, s->T0, s->A0);
    else
        gen_op_mov_v_reg(s, ot, s->T0, op1);

    op2 &= mask;
    if (op2 != 0) {
        if (is_right) {
            if (is_arith) {
                gen_exts(ot, s->T0);
                tcg_gen_sari_tl(s->tmp4, s->T0, op2 - 1);
                tcg_gen_sari_tl(s->T0, s->T0, op2);
            } else {
                gen_extu(ot, s->T0);
                tcg_gen_shri_tl(s->tmp4, s->T0, op2 - 1);
                tcg_gen_shri_tl(s->T0, s->T0, op2);
            }
        } else {
            tcg_gen_shli_tl(s->tmp4, s->T0, op2 - 1);
            tcg_gen_shli_tl(s->T0, s->T0, op2);
        }
    }

    /* store */
    gen_op_st_rm_T0_A0(s, ot, op1);

    /* update eflags if non zero shift */
    if (op2 != 0) {
        tcg_gen_mov_tl(cpu_cc_src, s->tmp4);
        tcg_gen_mov_tl(cpu_cc_dst, s->T0);
        set_cc_op(s, (is_right ? CC_OP_SARB : CC_OP_SHLB) + ot);
    }
}

static void gen_rot_rm_T1(DisasContext *s, TCGMemOp ot, int op1, int is_right)
{
    target_ulong mask = (ot == MO_64 ? 0x3f : 0x1f);
    TCGv_i32 t0, t1;

    /* load */
    if (op1 == OR_TMP0) {
        gen_op_ld_v(s, ot, s->T0, s->A0);
    } else {
        gen_op_mov_v_reg(s, ot, s->T0, op1);
    }

    tcg_gen_andi_tl(s->T1, s->T1, mask);

    switch (ot) {
    case MO_8:
        /* Replicate the 8-bit input so that a 32-bit rotate works.  */
        tcg_gen_ext8u_tl(s->T0, s->T0);
        tcg_gen_muli_tl(s->T0, s->T0, 0x01010101);
        goto do_long;
    case MO_16:
        /* Replicate the 16-bit input so that a 32-bit rotate works.  */
        tcg_gen_deposit_tl(s->T0, s->T0, s->T0, 16, 16);
        goto do_long;
    do_long:
#ifdef TARGET_X86_64
    case MO_32:
        tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
        tcg_gen_trunc_tl_i32(s->tmp3_i32, s->T1);
        if (is_right) {
            tcg_gen_rotr_i32(s->tmp2_i32, s->tmp2_i32, s->tmp3_i32);
        } else {
            tcg_gen_rotl_i32(s->tmp2_i32, s->tmp2_i32, s->tmp3_i32);
        }
        tcg_gen_extu_i32_tl(s->T0, s->tmp2_i32);
        break;
#endif
    default:
        if (is_right) {
            tcg_gen_rotr_tl(s->T0, s->T0, s->T1);
        } else {
            tcg_gen_rotl_tl(s->T0, s->T0, s->T1);
        }
        break;
    }

    /* store */
    gen_op_st_rm_T0_A0(s, ot, op1);

    /* We'll need the flags computed into CC_SRC.  */
    gen_compute_eflags(s);

    /* The value that was "rotated out" is now present at the other end
       of the word.  Compute C into CC_DST and O into CC_SRC2.  Note that
       since we've computed the flags into CC_SRC, these variables are
       currently dead.  */
    if (is_right) {
        tcg_gen_shri_tl(cpu_cc_src2, s->T0, mask - 1);
        tcg_gen_shri_tl(cpu_cc_dst, s->T0, mask);
        tcg_gen_andi_tl(cpu_cc_dst, cpu_cc_dst, 1);
    } else {
        tcg_gen_shri_tl(cpu_cc_src2, s->T0, mask);
        tcg_gen_andi_tl(cpu_cc_dst, s->T0, 1);
    }
    tcg_gen_andi_tl(cpu_cc_src2, cpu_cc_src2, 1);
    tcg_gen_xor_tl(cpu_cc_src2, cpu_cc_src2, cpu_cc_dst);

    /* Now conditionally store the new CC_OP value.  If the shift count
       is 0 we keep the CC_OP_EFLAGS setting so that only CC_SRC is live.
       Otherwise reuse CC_OP_ADCOX which have the C and O flags split out
       exactly as we computed above.  */
    t0 = tcg_const_i32(0);
    t1 = tcg_temp_new_i32();
    tcg_gen_trunc_tl_i32(t1, s->T1);
    tcg_gen_movi_i32(s->tmp2_i32, CC_OP_ADCOX);
    tcg_gen_movi_i32(s->tmp3_i32, CC_OP_EFLAGS);
    tcg_gen_movcond_i32(TCG_COND_NE, cpu_cc_op, t1, t0,
                        s->tmp2_i32, s->tmp3_i32);
    tcg_temp_free_i32(t0);
    tcg_temp_free_i32(t1);

    /* The CC_OP value is no longer predictable.  */ 
    set_cc_op(s, CC_OP_DYNAMIC);
}

static void gen_rot_rm_im(DisasContext *s, TCGMemOp ot, int op1, int op2,
                          int is_right)
{
    int mask = (ot == MO_64 ? 0x3f : 0x1f);
    int shift;

    /* load */
    if (op1 == OR_TMP0) {
        gen_op_ld_v(s, ot, s->T0, s->A0);
    } else {
        gen_op_mov_v_reg(s, ot, s->T0, op1);
    }

    op2 &= mask;
    if (op2 != 0) {
        switch (ot) {
#ifdef TARGET_X86_64
        case MO_32:
            tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
            if (is_right) {
                tcg_gen_rotri_i32(s->tmp2_i32, s->tmp2_i32, op2);
            } else {
                tcg_gen_rotli_i32(s->tmp2_i32, s->tmp2_i32, op2);
            }
            tcg_gen_extu_i32_tl(s->T0, s->tmp2_i32);
            break;
#endif
        default:
            if (is_right) {
                tcg_gen_rotri_tl(s->T0, s->T0, op2);
            } else {
                tcg_gen_rotli_tl(s->T0, s->T0, op2);
            }
            break;
        case MO_8:
            mask = 7;
            goto do_shifts;
        case MO_16:
            mask = 15;
        do_shifts:
            shift = op2 & mask;
            if (is_right) {
                shift = mask + 1 - shift;
            }
            gen_extu(ot, s->T0);
            tcg_gen_shli_tl(s->tmp0, s->T0, shift);
            tcg_gen_shri_tl(s->T0, s->T0, mask + 1 - shift);
            tcg_gen_or_tl(s->T0, s->T0, s->tmp0);
            break;
        }
    }

    /* store */
    gen_op_st_rm_T0_A0(s, ot, op1);

    if (op2 != 0) {
        /* Compute the flags into CC_SRC.  */
        gen_compute_eflags(s);

        /* The value that was "rotated out" is now present at the other end
           of the word.  Compute C into CC_DST and O into CC_SRC2.  Note that
           since we've computed the flags into CC_SRC, these variables are
           currently dead.  */
        if (is_right) {
            tcg_gen_shri_tl(cpu_cc_src2, s->T0, mask - 1);
            tcg_gen_shri_tl(cpu_cc_dst, s->T0, mask);
            tcg_gen_andi_tl(cpu_cc_dst, cpu_cc_dst, 1);
        } else {
            tcg_gen_shri_tl(cpu_cc_src2, s->T0, mask);
            tcg_gen_andi_tl(cpu_cc_dst, s->T0, 1);
        }
        tcg_gen_andi_tl(cpu_cc_src2, cpu_cc_src2, 1);
        tcg_gen_xor_tl(cpu_cc_src2, cpu_cc_src2, cpu_cc_dst);
        set_cc_op(s, CC_OP_ADCOX);
    }
}

/* XXX: add faster immediate = 1 case */
static void gen_rotc_rm_T1(DisasContext *s, TCGMemOp ot, int op1,
                           int is_right)
{
    gen_compute_eflags(s);
    assert(s->cc_op == CC_OP_EFLAGS);

    /* load */
    if (op1 == OR_TMP0)
        gen_op_ld_v(s, ot, s->T0, s->A0);
    else
        gen_op_mov_v_reg(s, ot, s->T0, op1);
    
    if (is_right) {
        switch (ot) {
        case MO_8:
            gen_helper_rcrb(s->T0, cpu_env, s->T0, s->T1);
            break;
        case MO_16:
            gen_helper_rcrw(s->T0, cpu_env, s->T0, s->T1);
            break;
        case MO_32:
            gen_helper_rcrl(s->T0, cpu_env, s->T0, s->T1);
            break;
#ifdef TARGET_X86_64
        case MO_64:
            gen_helper_rcrq(s->T0, cpu_env, s->T0, s->T1);
            break;
#endif
        default:
            tcg_abort();
        }
    } else {
        switch (ot) {
        case MO_8:
            gen_helper_rclb(s->T0, cpu_env, s->T0, s->T1);
            break;
        case MO_16:
            gen_helper_rclw(s->T0, cpu_env, s->T0, s->T1);
            break;
        case MO_32:
            gen_helper_rcll(s->T0, cpu_env, s->T0, s->T1);
            break;
#ifdef TARGET_X86_64
        case MO_64:
            gen_helper_rclq(s->T0, cpu_env, s->T0, s->T1);
            break;
#endif
        default:
            tcg_abort();
        }
    }
    /* store */
    gen_op_st_rm_T0_A0(s, ot, op1);
}

/* XXX: add faster immediate case */
static void gen_shiftd_rm_T1(DisasContext *s, TCGMemOp ot, int op1,
                             bool is_right, TCGv count_in)
{
    target_ulong mask = (ot == MO_64 ? 63 : 31);
    TCGv count;

    /* load */
    if (op1 == OR_TMP0) {
        gen_op_ld_v(s, ot, s->T0, s->A0);
    } else {
        gen_op_mov_v_reg(s, ot, s->T0, op1);
    }

    count = tcg_temp_new();
    tcg_gen_andi_tl(count, count_in, mask);

    switch (ot) {
    case MO_16:
        /* Note: we implement the Intel behaviour for shift count > 16.
           This means "shrdw C, B, A" shifts A:B:A >> C.  Build the B:A
           portion by constructing it as a 32-bit value.  */
        if (is_right) {
            tcg_gen_deposit_tl(s->tmp0, s->T0, s->T1, 16, 16);
            tcg_gen_mov_tl(s->T1, s->T0);
            tcg_gen_mov_tl(s->T0, s->tmp0);
        } else {
            tcg_gen_deposit_tl(s->T1, s->T0, s->T1, 16, 16);
        }
        /* FALLTHRU */
#ifdef TARGET_X86_64
    case MO_32:
        /* Concatenate the two 32-bit values and use a 64-bit shift.  */
        tcg_gen_subi_tl(s->tmp0, count, 1);
        if (is_right) {
            tcg_gen_concat_tl_i64(s->T0, s->T0, s->T1);
            tcg_gen_shr_i64(s->tmp0, s->T0, s->tmp0);
            tcg_gen_shr_i64(s->T0, s->T0, count);
        } else {
            tcg_gen_concat_tl_i64(s->T0, s->T1, s->T0);
            tcg_gen_shl_i64(s->tmp0, s->T0, s->tmp0);
            tcg_gen_shl_i64(s->T0, s->T0, count);
            tcg_gen_shri_i64(s->tmp0, s->tmp0, 32);
            tcg_gen_shri_i64(s->T0, s->T0, 32);
        }
        break;
#endif
    default:
        tcg_gen_subi_tl(s->tmp0, count, 1);
        if (is_right) {
            tcg_gen_shr_tl(s->tmp0, s->T0, s->tmp0);

            tcg_gen_subfi_tl(s->tmp4, mask + 1, count);
            tcg_gen_shr_tl(s->T0, s->T0, count);
            tcg_gen_shl_tl(s->T1, s->T1, s->tmp4);
        } else {
            tcg_gen_shl_tl(s->tmp0, s->T0, s->tmp0);
            if (ot == MO_16) {
                /* Only needed if count > 16, for Intel behaviour.  */
                tcg_gen_subfi_tl(s->tmp4, 33, count);
                tcg_gen_shr_tl(s->tmp4, s->T1, s->tmp4);
                tcg_gen_or_tl(s->tmp0, s->tmp0, s->tmp4);
            }

            tcg_gen_subfi_tl(s->tmp4, mask + 1, count);
            tcg_gen_shl_tl(s->T0, s->T0, count);
            tcg_gen_shr_tl(s->T1, s->T1, s->tmp4);
        }
        tcg_gen_movi_tl(s->tmp4, 0);
        tcg_gen_movcond_tl(TCG_COND_EQ, s->T1, count, s->tmp4,
                           s->tmp4, s->T1);
        tcg_gen_or_tl(s->T0, s->T0, s->T1);
        break;
    }

    /* store */
    gen_op_st_rm_T0_A0(s, ot, op1);

    gen_shift_flags(s, ot, s->T0, s->tmp0, count, is_right);
    tcg_temp_free(count);
}

static void gen_shift(DisasContext *s1, int op, TCGMemOp ot, int d, int s)
{
    if (s != OR_TMP1)
        gen_op_mov_v_reg(s1, ot, s1->T1, s);
    switch(op) {
    case OP_ROL:
        gen_rot_rm_T1(s1, ot, d, 0);
        break;
    case OP_ROR:
        gen_rot_rm_T1(s1, ot, d, 1);
        break;
    case OP_SHL:
    case OP_SHL1:
        gen_shift_rm_T1(s1, ot, d, 0, 0);
        break;
    case OP_SHR:
        gen_shift_rm_T1(s1, ot, d, 1, 0);
        break;
    case OP_SAR:
        gen_shift_rm_T1(s1, ot, d, 1, 1);
        break;
    case OP_RCL:
        gen_rotc_rm_T1(s1, ot, d, 0);
        break;
    case OP_RCR:
        gen_rotc_rm_T1(s1, ot, d, 1);
        break;
    }
}

static void gen_shifti(DisasContext *s1, int op, TCGMemOp ot, int d, int c)
{
    switch(op) {
    case OP_ROL:
        gen_rot_rm_im(s1, ot, d, c, 0);
        break;
    case OP_ROR:
        gen_rot_rm_im(s1, ot, d, c, 1);
        break;
    case OP_SHL:
    case OP_SHL1:
        gen_shift_rm_im(s1, ot, d, c, 0, 0);
        break;
    case OP_SHR:
        gen_shift_rm_im(s1, ot, d, c, 1, 0);
        break;
    case OP_SAR:
        gen_shift_rm_im(s1, ot, d, c, 1, 1);
        break;
    default:
        /* currently not optimized */
        tcg_gen_movi_tl(s1->T1, c);
        gen_shift(s1, op, ot, d, OR_TMP1);
        break;
    }
}

#define X86_MAX_INSN_LENGTH 15

static uint64_t advance_pc(CPUX86State *env, DisasContext *s, int num_bytes)
{
    uint64_t pc = s->pc;

    s->pc += num_bytes;
    if (unlikely(s->pc - s->pc_start > X86_MAX_INSN_LENGTH)) {
        /* If the instruction's 16th byte is on a different page than the 1st, a
         * page fault on the second page wins over the general protection fault
         * caused by the instruction being too long.
         * This can happen even if the operand is only one byte long!
         */
        if (((s->pc - 1) ^ (pc - 1)) & TARGET_PAGE_MASK) {
            volatile uint8_t unused =
                cpu_ldub_code(env, (s->pc - 1) & TARGET_PAGE_MASK);
            (void) unused;
        }
        siglongjmp(s->jmpbuf, 1);
    }

    return pc;
}

static inline uint8_t x86_ldub_code(CPUX86State *env, DisasContext *s)
{
    return cpu_ldub_code(env, advance_pc(env, s, 1));
}

static inline int16_t x86_ldsw_code(CPUX86State *env, DisasContext *s)
{
    return cpu_ldsw_code(env, advance_pc(env, s, 2));
}

static inline uint16_t x86_lduw_code(CPUX86State *env, DisasContext *s)
{
    return cpu_lduw_code(env, advance_pc(env, s, 2));
}

static inline uint32_t x86_ldl_code(CPUX86State *env, DisasContext *s)
{
    return cpu_ldl_code(env, advance_pc(env, s, 4));
}

#ifdef TARGET_X86_64
static inline uint64_t x86_ldq_code(CPUX86State *env, DisasContext *s)
{
    return cpu_ldq_code(env, advance_pc(env, s, 8));
}
#endif

/* Decompose an address.  */

typedef struct AddressParts {
    int def_seg;
    int base;
    int index;
    int scale;
    target_long disp;
} AddressParts;

static AddressParts gen_lea_modrm_0(CPUX86State *env, DisasContext *s,
                                    int modrm)
{
    int def_seg, base, index, scale, mod, rm;
    target_long disp;
    bool havesib;

    def_seg = R_DS;
    index = -1;
    scale = 0;
    disp = 0;

    mod = (modrm >> 6) & 3;
    rm = modrm & 7;
    base = rm | REX_B(s);

    if (mod == 3) {
        /* Normally filtered out earlier, but including this path
           simplifies multi-byte nop, as well as bndcl, bndcu, bndcn.  */
        goto done;
    }

    switch (s->aflag) {
    case MO_64:
    case MO_32:
        havesib = 0;
        if (rm == 4) {
            int code = x86_ldub_code(env, s);
            scale = (code >> 6) & 3;
            index = ((code >> 3) & 7) | REX_X(s);
            if (index == 4) {
                index = -1;  /* no index */
            }
            base = (code & 7) | REX_B(s);
            havesib = 1;
        }

        switch (mod) {
        case 0:
            if ((base & 7) == 5) {
                base = -1;
                disp = (int32_t)x86_ldl_code(env, s);
                if (CODE64(s) && !havesib) {
                    base = -2;
                    disp += s->pc + s->rip_offset;
                }
            }
            break;
        case 1:
            disp = (int8_t)x86_ldub_code(env, s);
            break;
        default:
        case 2:
            disp = (int32_t)x86_ldl_code(env, s);
            break;
        }

        /* For correct popl handling with esp.  */
        if (base == R_ESP && s->popl_esp_hack) {
            disp += s->popl_esp_hack;
        }
        if (base == R_EBP || base == R_ESP) {
            def_seg = R_SS;
        }
        break;

    case MO_16:
        if (mod == 0) {
            if (rm == 6) {
                base = -1;
                disp = x86_lduw_code(env, s);
                break;
            }
        } else if (mod == 1) {
            disp = (int8_t)x86_ldub_code(env, s);
        } else {
            disp = (int16_t)x86_lduw_code(env, s);
        }

        switch (rm) {
        case 0:
            base = R_EBX;
            index = R_ESI;
            break;
        case 1:
            base = R_EBX;
            index = R_EDI;
            break;
        case 2:
            base = R_EBP;
            index = R_ESI;
            def_seg = R_SS;
            break;
        case 3:
            base = R_EBP;
            index = R_EDI;
            def_seg = R_SS;
            break;
        case 4:
            base = R_ESI;
            break;
        case 5:
            base = R_EDI;
            break;
        case 6:
            base = R_EBP;
            def_seg = R_SS;
            break;
        default:
        case 7:
            base = R_EBX;
            break;
        }
        break;

    default:
        tcg_abort();
    }

 done:
    return (AddressParts){ def_seg, base, index, scale, disp };
}

/* Compute the address, with a minimum number of TCG ops.  */
static TCGv gen_lea_modrm_1(DisasContext *s, AddressParts a)
{
    TCGv ea = NULL;

    if (a.index >= 0) {
        if (a.scale == 0) {
            ea = cpu_regs[a.index];
        } else {
            tcg_gen_shli_tl(s->A0, cpu_regs[a.index], a.scale);
            ea = s->A0;
        }
        if (a.base >= 0) {
            tcg_gen_add_tl(s->A0, ea, cpu_regs[a.base]);
            ea = s->A0;
        }
    } else if (a.base >= 0) {
        ea = cpu_regs[a.base];
    }
    if (!ea) {
        tcg_gen_movi_tl(s->A0, a.disp);
        ea = s->A0;
    } else if (a.disp != 0) {
        tcg_gen_addi_tl(s->A0, ea, a.disp);
        ea = s->A0;
    }

    return ea;
}

static void gen_lea_modrm(CPUX86State *env, DisasContext *s, int modrm)
{
    AddressParts a = gen_lea_modrm_0(env, s, modrm);
    TCGv ea = gen_lea_modrm_1(s, a);
    gen_lea_v_seg(s, s->aflag, ea, a.def_seg, s->override);
}

static void gen_nop_modrm(CPUX86State *env, DisasContext *s, int modrm)
{
    (void)gen_lea_modrm_0(env, s, modrm);
}

/* Used for BNDCL, BNDCU, BNDCN.  */
static void gen_bndck(CPUX86State *env, DisasContext *s, int modrm,
                      TCGCond cond, TCGv_i64 bndv)
{
    TCGv ea = gen_lea_modrm_1(s, gen_lea_modrm_0(env, s, modrm));

    tcg_gen_extu_tl_i64(s->tmp1_i64, ea);
    if (!CODE64(s)) {
        tcg_gen_ext32u_i64(s->tmp1_i64, s->tmp1_i64);
    }
    tcg_gen_setcond_i64(cond, s->tmp1_i64, s->tmp1_i64, bndv);
    tcg_gen_extrl_i64_i32(s->tmp2_i32, s->tmp1_i64);
    gen_helper_bndck(cpu_env, s->tmp2_i32);
}

/* used for LEA and MOV AX, mem */
static void gen_add_A0_ds_seg(DisasContext *s)
{
    gen_lea_v_seg(s, s->aflag, s->A0, R_DS, s->override);
}

/* generate modrm memory load or store of 'reg'. TMP0 is used if reg ==
   OR_TMP0 */
static void gen_ldst_modrm(CPUX86State *env, DisasContext *s, int modrm,
                           TCGMemOp ot, int reg, int is_store)
{
    int mod, rm;

    mod = (modrm >> 6) & 3;
    rm = (modrm & 7) | REX_B(s);
    if (mod == 3) {
        if (is_store) {
            if (reg != OR_TMP0)
                gen_op_mov_v_reg(s, ot, s->T0, reg);
            gen_op_mov_reg_v(s, ot, rm, s->T0);
        } else {
            gen_op_mov_v_reg(s, ot, s->T0, rm);
            if (reg != OR_TMP0)
                gen_op_mov_reg_v(s, ot, reg, s->T0);
        }
    } else {
        gen_lea_modrm(env, s, modrm);
        if (is_store) {
            if (reg != OR_TMP0)
                gen_op_mov_v_reg(s, ot, s->T0, reg);
            gen_op_st_v(s, ot, s->T0, s->A0);
        } else {
            gen_op_ld_v(s, ot, s->T0, s->A0);
            if (reg != OR_TMP0)
                gen_op_mov_reg_v(s, ot, reg, s->T0);
        }
    }
}

static inline uint32_t insn_get(CPUX86State *env, DisasContext *s, TCGMemOp ot)
{
    uint32_t ret;

    switch (ot) {
    case MO_8:
        ret = x86_ldub_code(env, s);
        break;
    case MO_16:
        ret = x86_lduw_code(env, s);
        break;
    case MO_32:
#ifdef TARGET_X86_64
    case MO_64:
#endif
        ret = x86_ldl_code(env, s);
        break;
    default:
        tcg_abort();
    }
    return ret;
}

static inline int insn_const_size(TCGMemOp ot)
{
    if (ot <= MO_32) {
        return 1 << ot;
    } else {
        return 4;
    }
}

static inline bool use_goto_tb(DisasContext *s, target_ulong pc)
{
#ifndef CONFIG_USER_ONLY
    return (pc & TARGET_PAGE_MASK) == (s->base.tb->pc & TARGET_PAGE_MASK) ||
           (pc & TARGET_PAGE_MASK) == (s->pc_start & TARGET_PAGE_MASK);
#else
    return true;
#endif
}

static inline void gen_goto_tb(DisasContext *s, int tb_num, target_ulong eip)
{
    target_ulong pc = s->cs_base + eip;

    if (use_goto_tb(s, pc))  {
        /* jump to same page: we can use a direct jump */
        tcg_gen_goto_tb(tb_num);
        gen_jmp_im(s, eip);
        tcg_gen_exit_tb(s->base.tb, tb_num);
        s->base.is_jmp = DISAS_NORETURN;
    } else {
        /* jump to another page */
        gen_jmp_im(s, eip);
        gen_jr(s, s->tmp0);
    }
}

static inline void gen_jcc(DisasContext *s, int b,
                           target_ulong val, target_ulong next_eip)
{
    TCGLabel *l1, *l2;

    if (s->jmp_opt) {
        l1 = gen_new_label();
        gen_jcc1(s, b, l1);

        gen_goto_tb(s, 0, next_eip);

        gen_set_label(l1);
        gen_goto_tb(s, 1, val);
    } else {
        l1 = gen_new_label();
        l2 = gen_new_label();
        gen_jcc1(s, b, l1);

        gen_jmp_im(s, next_eip);
        tcg_gen_br(l2);

        gen_set_label(l1);
        gen_jmp_im(s, val);
        gen_set_label(l2);
        gen_eob(s);
    }
}

static void gen_cmovcc1(CPUX86State *env, DisasContext *s, TCGMemOp ot, int b,
                        int modrm, int reg)
{
    CCPrepare cc;

    gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);

    cc = gen_prepare_cc(s, b, s->T1);
    if (cc.mask != -1) {
        TCGv t0 = tcg_temp_new();
        tcg_gen_andi_tl(t0, cc.reg, cc.mask);
        cc.reg = t0;
    }
    if (!cc.use_reg2) {
        cc.reg2 = tcg_const_tl(cc.imm);
    }

    tcg_gen_movcond_tl(cc.cond, s->T0, cc.reg, cc.reg2,
                       s->T0, cpu_regs[reg]);
    gen_op_mov_reg_v(s, ot, reg, s->T0);

    if (cc.mask != -1) {
        tcg_temp_free(cc.reg);
    }
    if (!cc.use_reg2) {
        tcg_temp_free(cc.reg2);
    }
}

static inline void gen_op_movl_T0_seg(DisasContext *s, int seg_reg)
{
    tcg_gen_ld32u_tl(s->T0, cpu_env,
                     offsetof(CPUX86State,segs[seg_reg].selector));
}

static inline void gen_op_movl_seg_T0_vm(DisasContext *s, int seg_reg)
{
    tcg_gen_ext16u_tl(s->T0, s->T0);
    tcg_gen_st32_tl(s->T0, cpu_env,
                    offsetof(CPUX86State,segs[seg_reg].selector));
    tcg_gen_shli_tl(cpu_seg_base[seg_reg], s->T0, 4);
}

/* move T0 to seg_reg and compute if the CPU state may change. Never
   call this function with seg_reg == R_CS */
static void gen_movl_seg_T0(DisasContext *s, int seg_reg)
{
    if (s->pe && !s->vm86) {
        tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
        gen_helper_load_seg(cpu_env, tcg_const_i32(seg_reg), s->tmp2_i32);
        /* abort translation because the addseg value may change or
           because ss32 may change. For R_SS, translation must always
           stop as a special handling must be done to disable hardware
           interrupts for the next instruction */
        if (seg_reg == R_SS || (s->code32 && seg_reg < R_FS)) {
            s->base.is_jmp = DISAS_TOO_MANY;
        }
    } else {
        gen_op_movl_seg_T0_vm(s, seg_reg);
        if (seg_reg == R_SS) {
            s->base.is_jmp = DISAS_TOO_MANY;
        }
    }
}

static inline int svm_is_rep(int prefixes)
{
    return ((prefixes & (PREFIX_REPZ | PREFIX_REPNZ)) ? 8 : 0);
}

static inline void
gen_svm_check_intercept_param(DisasContext *s, target_ulong pc_start,
                              uint32_t type, uint64_t param)
{
    /* no SVM activated; fast case */
    if (likely(!(s->flags & HF_GUEST_MASK)))
        return;
    gen_update_cc_op(s);
    gen_jmp_im(s, pc_start - s->cs_base);
    gen_helper_svm_check_intercept_param(cpu_env, tcg_const_i32(type),
                                         tcg_const_i64(param));
}

static inline void
gen_svm_check_intercept(DisasContext *s, target_ulong pc_start, uint64_t type)
{
    gen_svm_check_intercept_param(s, pc_start, type, 0);
}

static inline void gen_stack_update(DisasContext *s, int addend)
{
    gen_op_add_reg_im(s, mo_stacksize(s), R_ESP, addend);
}

/* Generate a push. It depends on ss32, addseg and dflag.  */
static void gen_push_v(DisasContext *s, TCGv val)
{
    TCGMemOp d_ot = mo_pushpop(s, s->dflag);
    TCGMemOp a_ot = mo_stacksize(s);
    int size = 1 << d_ot;
    TCGv new_esp = s->A0;

    tcg_gen_subi_tl(s->A0, cpu_regs[R_ESP], size);

    if (!CODE64(s)) {
        if (s->addseg) {
            new_esp = s->tmp4;
            tcg_gen_mov_tl(new_esp, s->A0);
        }
        gen_lea_v_seg(s, a_ot, s->A0, R_SS, -1);
    }

    gen_op_st_v(s, d_ot, val, s->A0);
    gen_op_mov_reg_v(s, a_ot, R_ESP, new_esp);
}

/* two step pop is necessary for precise exceptions */
static TCGMemOp gen_pop_T0(DisasContext *s)
{
    TCGMemOp d_ot = mo_pushpop(s, s->dflag);

    gen_lea_v_seg(s, mo_stacksize(s), cpu_regs[R_ESP], R_SS, -1);
    gen_op_ld_v(s, d_ot, s->T0, s->A0);

    return d_ot;
}

static inline void gen_pop_update(DisasContext *s, TCGMemOp ot)
{
    gen_stack_update(s, 1 << ot);
}

static inline void gen_stack_A0(DisasContext *s)
{
    gen_lea_v_seg(s, s->ss32 ? MO_32 : MO_16, cpu_regs[R_ESP], R_SS, -1);
}

static void gen_pusha(DisasContext *s)
{
    TCGMemOp s_ot = s->ss32 ? MO_32 : MO_16;
    TCGMemOp d_ot = s->dflag;
    int size = 1 << d_ot;
    int i;

    for (i = 0; i < 8; i++) {
        tcg_gen_addi_tl(s->A0, cpu_regs[R_ESP], (i - 8) * size);
        gen_lea_v_seg(s, s_ot, s->A0, R_SS, -1);
        gen_op_st_v(s, d_ot, cpu_regs[7 - i], s->A0);
    }

    gen_stack_update(s, -8 * size);
}

static void gen_popa(DisasContext *s)
{
    TCGMemOp s_ot = s->ss32 ? MO_32 : MO_16;
    TCGMemOp d_ot = s->dflag;
    int size = 1 << d_ot;
    int i;

    for (i = 0; i < 8; i++) {
        /* ESP is not reloaded */
        if (7 - i == R_ESP) {
            continue;
        }
        tcg_gen_addi_tl(s->A0, cpu_regs[R_ESP], i * size);
        gen_lea_v_seg(s, s_ot, s->A0, R_SS, -1);
        gen_op_ld_v(s, d_ot, s->T0, s->A0);
        gen_op_mov_reg_v(s, d_ot, 7 - i, s->T0);
    }

    gen_stack_update(s, 8 * size);
}

static void gen_enter(DisasContext *s, int esp_addend, int level)
{
    TCGMemOp d_ot = mo_pushpop(s, s->dflag);
    TCGMemOp a_ot = CODE64(s) ? MO_64 : s->ss32 ? MO_32 : MO_16;
    int size = 1 << d_ot;

    /* Push BP; compute FrameTemp into T1.  */
    tcg_gen_subi_tl(s->T1, cpu_regs[R_ESP], size);
    gen_lea_v_seg(s, a_ot, s->T1, R_SS, -1);
    gen_op_st_v(s, d_ot, cpu_regs[R_EBP], s->A0);

    level &= 31;
    if (level != 0) {
        int i;

        /* Copy level-1 pointers from the previous frame.  */
        for (i = 1; i < level; ++i) {
            tcg_gen_subi_tl(s->A0, cpu_regs[R_EBP], size * i);
            gen_lea_v_seg(s, a_ot, s->A0, R_SS, -1);
            gen_op_ld_v(s, d_ot, s->tmp0, s->A0);

            tcg_gen_subi_tl(s->A0, s->T1, size * i);
            gen_lea_v_seg(s, a_ot, s->A0, R_SS, -1);
            gen_op_st_v(s, d_ot, s->tmp0, s->A0);
        }

        /* Push the current FrameTemp as the last level.  */
        tcg_gen_subi_tl(s->A0, s->T1, size * level);
        gen_lea_v_seg(s, a_ot, s->A0, R_SS, -1);
        gen_op_st_v(s, d_ot, s->T1, s->A0);
    }

    /* Copy the FrameTemp value to EBP.  */
    gen_op_mov_reg_v(s, a_ot, R_EBP, s->T1);

    /* Compute the final value of ESP.  */
    tcg_gen_subi_tl(s->T1, s->T1, esp_addend + size * level);
    gen_op_mov_reg_v(s, a_ot, R_ESP, s->T1);
}

static void gen_leave(DisasContext *s)
{
    TCGMemOp d_ot = mo_pushpop(s, s->dflag);
    TCGMemOp a_ot = mo_stacksize(s);

    gen_lea_v_seg(s, a_ot, cpu_regs[R_EBP], R_SS, -1);
    gen_op_ld_v(s, d_ot, s->T0, s->A0);

    tcg_gen_addi_tl(s->T1, cpu_regs[R_EBP], 1 << d_ot);

    gen_op_mov_reg_v(s, d_ot, R_EBP, s->T0);
    gen_op_mov_reg_v(s, a_ot, R_ESP, s->T1);
}

/* Similarly, except that the assumption here is that we don't decode
   the instruction at all -- either a missing opcode, an unimplemented
   feature, or just a bogus instruction stream.  */
static void gen_unknown_opcode(CPUX86State *env, DisasContext *s)
{
    gen_illegal_opcode(s);

    if (qemu_loglevel_mask(LOG_UNIMP)) {
        target_ulong pc = s->pc_start, end = s->pc;
        qemu_log_lock();
        qemu_log("ILLOPC: " TARGET_FMT_lx ":", pc);
        for (; pc < end; ++pc) {
            qemu_log(" %02x", cpu_ldub_code(env, pc));
        }
        qemu_log("\n");
        qemu_log_unlock();
    }
}

/* an interrupt is different from an exception because of the
   privilege checks */
static void gen_interrupt(DisasContext *s, int intno,
                          target_ulong cur_eip, target_ulong next_eip)
{
    gen_update_cc_op(s);
    gen_jmp_im(s, cur_eip);
    gen_helper_raise_interrupt(cpu_env, tcg_const_i32(intno),
                               tcg_const_i32(next_eip - cur_eip));
    s->base.is_jmp = DISAS_NORETURN;
}

static void gen_debug(DisasContext *s, target_ulong cur_eip)
{
    gen_update_cc_op(s);
    gen_jmp_im(s, cur_eip);
    gen_helper_debug(cpu_env);
    s->base.is_jmp = DISAS_NORETURN;
}

static void gen_set_hflag(DisasContext *s, uint32_t mask)
{
    if ((s->flags & mask) == 0) {
        TCGv_i32 t = tcg_temp_new_i32();
        tcg_gen_ld_i32(t, cpu_env, offsetof(CPUX86State, hflags));
        tcg_gen_ori_i32(t, t, mask);
        tcg_gen_st_i32(t, cpu_env, offsetof(CPUX86State, hflags));
        tcg_temp_free_i32(t);
        s->flags |= mask;
    }
}

static void gen_reset_hflag(DisasContext *s, uint32_t mask)
{
    if (s->flags & mask) {
        TCGv_i32 t = tcg_temp_new_i32();
        tcg_gen_ld_i32(t, cpu_env, offsetof(CPUX86State, hflags));
        tcg_gen_andi_i32(t, t, ~mask);
        tcg_gen_st_i32(t, cpu_env, offsetof(CPUX86State, hflags));
        tcg_temp_free_i32(t);
        s->flags &= ~mask;
    }
}

/* Clear BND registers during legacy branches.  */
static void gen_bnd_jmp(DisasContext *s)
{
    /* Clear the registers only if BND prefix is missing, MPX is enabled,
       and if the BNDREGs are known to be in use (non-zero) already.
       The helper itself will check BNDPRESERVE at runtime.  */
    if ((s->prefix & PREFIX_REPNZ) == 0
        && (s->flags & HF_MPX_EN_MASK) != 0
        && (s->flags & HF_MPX_IU_MASK) != 0) {
        gen_helper_bnd_jmp(cpu_env);
    }
}

/* Generate an end of block. Trace exception is also generated if needed.
   If INHIBIT, set HF_INHIBIT_IRQ_MASK if it isn't already set.
   If RECHECK_TF, emit a rechecking helper for #DB, ignoring the state of
   S->TF.  This is used by the syscall/sysret insns.  */
static void
do_gen_eob_worker(DisasContext *s, bool inhibit, bool recheck_tf, bool jr)
{
    gen_update_cc_op(s);

    /* If several instructions disable interrupts, only the first does it.  */
    if (inhibit && !(s->flags & HF_INHIBIT_IRQ_MASK)) {
        gen_set_hflag(s, HF_INHIBIT_IRQ_MASK);
    } else {
        gen_reset_hflag(s, HF_INHIBIT_IRQ_MASK);
    }

    if (s->base.tb->flags & HF_RF_MASK) {
        gen_helper_reset_rf(cpu_env);
    }
    if (s->base.singlestep_enabled) {
        gen_helper_debug(cpu_env);
    } else if (recheck_tf) {
        gen_helper_rechecking_single_step(cpu_env);
        tcg_gen_exit_tb(NULL, 0);
    } else if (s->tf) {
        gen_helper_single_step(cpu_env);
    } else if (jr) {
        tcg_gen_lookup_and_goto_ptr();
    } else {
        tcg_gen_exit_tb(NULL, 0);
    }
    s->base.is_jmp = DISAS_NORETURN;
}

static inline void
gen_eob_worker(DisasContext *s, bool inhibit, bool recheck_tf)
{
    do_gen_eob_worker(s, inhibit, recheck_tf, false);
}

/* End of block.
   If INHIBIT, set HF_INHIBIT_IRQ_MASK if it isn't already set.  */
static void gen_eob_inhibit_irq(DisasContext *s, bool inhibit)
{
    gen_eob_worker(s, inhibit, false);
}

/* End of block, resetting the inhibit irq flag.  */
static void gen_eob(DisasContext *s)
{
    gen_eob_worker(s, false, false);
}

/* Jump to register */
static void gen_jr(DisasContext *s, TCGv dest)
{
    do_gen_eob_worker(s, false, false, true);
}

/* generate a jump to eip. No segment change must happen before as a
   direct call to the next block may occur */
static void gen_jmp_tb(DisasContext *s, target_ulong eip, int tb_num)
{
    gen_update_cc_op(s);
    set_cc_op(s, CC_OP_DYNAMIC);
    if (s->jmp_opt) {
        gen_goto_tb(s, tb_num, eip);
    } else {
        gen_jmp_im(s, eip);
        gen_eob(s);
    }
}

static void gen_jmp(DisasContext *s, target_ulong eip)
{
    gen_jmp_tb(s, eip, 0);
}

static inline void gen_ldq_env_A0(DisasContext *s, int offset)
{
    tcg_gen_qemu_ld_i64(s->tmp1_i64, s->A0, s->mem_index, MO_LEQ);
    tcg_gen_st_i64(s->tmp1_i64, cpu_env, offset);
}

static inline void gen_stq_env_A0(DisasContext *s, int offset)
{
    tcg_gen_ld_i64(s->tmp1_i64, cpu_env, offset);
    tcg_gen_qemu_st_i64(s->tmp1_i64, s->A0, s->mem_index, MO_LEQ);
}

static inline void gen_ldo_env_A0(DisasContext *s, int offset)
{
    int mem_index = s->mem_index;
    tcg_gen_qemu_ld_i64(s->tmp1_i64, s->A0, mem_index, MO_LEQ);
    tcg_gen_st_i64(s->tmp1_i64, cpu_env, offset + offsetof(ZMMReg, ZMM_Q(0)));
    tcg_gen_addi_tl(s->tmp0, s->A0, 8);
    tcg_gen_qemu_ld_i64(s->tmp1_i64, s->tmp0, mem_index, MO_LEQ);
    tcg_gen_st_i64(s->tmp1_i64, cpu_env, offset + offsetof(ZMMReg, ZMM_Q(1)));
}

static inline void gen_sto_env_A0(DisasContext *s, int offset)
{
    int mem_index = s->mem_index;
    tcg_gen_ld_i64(s->tmp1_i64, cpu_env, offset + offsetof(ZMMReg, ZMM_Q(0)));
    tcg_gen_qemu_st_i64(s->tmp1_i64, s->A0, mem_index, MO_LEQ);
    tcg_gen_addi_tl(s->tmp0, s->A0, 8);
    tcg_gen_ld_i64(s->tmp1_i64, cpu_env, offset + offsetof(ZMMReg, ZMM_Q(1)));
    tcg_gen_qemu_st_i64(s->tmp1_i64, s->tmp0, mem_index, MO_LEQ);
}

static inline void gen_op_movo(DisasContext *s, int d_offset, int s_offset)
{
    tcg_gen_ld_i64(s->tmp1_i64, cpu_env, s_offset + offsetof(ZMMReg, ZMM_Q(0)));
    tcg_gen_st_i64(s->tmp1_i64, cpu_env, d_offset + offsetof(ZMMReg, ZMM_Q(0)));
    tcg_gen_ld_i64(s->tmp1_i64, cpu_env, s_offset + offsetof(ZMMReg, ZMM_Q(1)));
    tcg_gen_st_i64(s->tmp1_i64, cpu_env, d_offset + offsetof(ZMMReg, ZMM_Q(1)));
}

static inline void gen_op_movq(DisasContext *s, int d_offset, int s_offset)
{
    tcg_gen_ld_i64(s->tmp1_i64, cpu_env, s_offset);
    tcg_gen_st_i64(s->tmp1_i64, cpu_env, d_offset);
}

static inline void gen_op_movl(DisasContext *s, int d_offset, int s_offset)
{
    tcg_gen_ld_i32(s->tmp2_i32, cpu_env, s_offset);
    tcg_gen_st_i32(s->tmp2_i32, cpu_env, d_offset);
}

static inline void gen_op_movq_env_0(DisasContext *s, int d_offset)
{
    tcg_gen_movi_i64(s->tmp1_i64, 0);
    tcg_gen_st_i64(s->tmp1_i64, cpu_env, d_offset);
}

typedef void (*SSEFunc_i_ep)(TCGv_i32 val, TCGv_ptr env, TCGv_ptr reg);
typedef void (*SSEFunc_l_ep)(TCGv_i64 val, TCGv_ptr env, TCGv_ptr reg);
typedef void (*SSEFunc_0_epi)(TCGv_ptr env, TCGv_ptr reg, TCGv_i32 val);
typedef void (*SSEFunc_0_epl)(TCGv_ptr env, TCGv_ptr reg, TCGv_i64 val);
typedef void (*SSEFunc_0_epp)(TCGv_ptr env, TCGv_ptr reg_a, TCGv_ptr reg_b);
typedef void (*SSEFunc_0_eppi)(TCGv_ptr env, TCGv_ptr reg_a, TCGv_ptr reg_b,
                               TCGv_i32 val);
typedef void (*SSEFunc_0_ppi)(TCGv_ptr reg_a, TCGv_ptr reg_b, TCGv_i32 val);
typedef void (*SSEFunc_0_eppt)(TCGv_ptr env, TCGv_ptr reg_a, TCGv_ptr reg_b,
                               TCGv val);

#define SSE_SPECIAL ((void *)1)
#define SSE_DUMMY ((void *)2)

#define MMX_OP2(x) { gen_helper_ ## x ## _mmx, gen_helper_ ## x ## _xmm }
#define SSE_FOP(x) { gen_helper_ ## x ## ps, gen_helper_ ## x ## pd, \
                     gen_helper_ ## x ## ss, gen_helper_ ## x ## sd, }

static const SSEFunc_0_epp sse_op_table1[256][4] = {
    /* 3DNow! extensions */
    [0x0e] = { SSE_DUMMY }, /* femms */
    [0x0f] = { SSE_DUMMY }, /* pf... */
    /* pure SSE operations */
    [0x10] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movups, movupd, movss, movsd */
    [0x11] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movups, movupd, movss, movsd */
    [0x12] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movlps, movlpd, movsldup, movddup */
    [0x13] = { SSE_SPECIAL, SSE_SPECIAL },  /* movlps, movlpd */
    [0x14] = { gen_helper_punpckldq_xmm, gen_helper_punpcklqdq_xmm },
    [0x15] = { gen_helper_punpckhdq_xmm, gen_helper_punpckhqdq_xmm },
    [0x16] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL },  /* movhps, movhpd, movshdup */
    [0x17] = { SSE_SPECIAL, SSE_SPECIAL },  /* movhps, movhpd */

    [0x28] = { SSE_SPECIAL, SSE_SPECIAL },  /* movaps, movapd */
    [0x29] = { SSE_SPECIAL, SSE_SPECIAL },  /* movaps, movapd */
    [0x2a] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* cvtpi2ps, cvtpi2pd, cvtsi2ss, cvtsi2sd */
    [0x2b] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movntps, movntpd, movntss, movntsd */
    [0x2c] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* cvttps2pi, cvttpd2pi, cvttsd2si, cvttss2si */
    [0x2d] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* cvtps2pi, cvtpd2pi, cvtsd2si, cvtss2si */
    [0x2e] = { gen_helper_ucomiss, gen_helper_ucomisd },
    [0x2f] = { gen_helper_comiss, gen_helper_comisd },
    [0x50] = { SSE_SPECIAL, SSE_SPECIAL }, /* movmskps, movmskpd */
    [0x51] = SSE_FOP(sqrt),
    [0x52] = { gen_helper_rsqrtps, NULL, gen_helper_rsqrtss, NULL },
    [0x53] = { gen_helper_rcpps, NULL, gen_helper_rcpss, NULL },
    [0x54] = { gen_helper_pand_xmm, gen_helper_pand_xmm }, /* andps, andpd */
    [0x55] = { gen_helper_pandn_xmm, gen_helper_pandn_xmm }, /* andnps, andnpd */
    [0x56] = { gen_helper_por_xmm, gen_helper_por_xmm }, /* orps, orpd */
    [0x57] = { gen_helper_pxor_xmm, gen_helper_pxor_xmm }, /* xorps, xorpd */
    [0x58] = SSE_FOP(add),
    [0x59] = SSE_FOP(mul),
    [0x5a] = { gen_helper_cvtps2pd, gen_helper_cvtpd2ps,
               gen_helper_cvtss2sd, gen_helper_cvtsd2ss },
    [0x5b] = { gen_helper_cvtdq2ps, gen_helper_cvtps2dq, gen_helper_cvttps2dq },
    [0x5c] = SSE_FOP(sub),
    [0x5d] = SSE_FOP(min),
    [0x5e] = SSE_FOP(div),
    [0x5f] = SSE_FOP(max),

    [0xc2] = SSE_FOP(cmpeq),
    [0xc6] = { (SSEFunc_0_epp)gen_helper_shufps,
               (SSEFunc_0_epp)gen_helper_shufpd }, /* XXX: casts */

    /* SSSE3, SSE4, MOVBE, CRC32, BMI1, BMI2, ADX.  */
    [0x38] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL },
    [0x3a] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL },

    /* MMX ops and their SSE extensions */
    [0x60] = MMX_OP2(punpcklbw),
    [0x61] = MMX_OP2(punpcklwd),
    [0x62] = MMX_OP2(punpckldq),
    [0x63] = MMX_OP2(packsswb),
    [0x64] = MMX_OP2(pcmpgtb),
    [0x65] = MMX_OP2(pcmpgtw),
    [0x66] = MMX_OP2(pcmpgtl),
    [0x67] = MMX_OP2(packuswb),
    [0x68] = MMX_OP2(punpckhbw),
    [0x69] = MMX_OP2(punpckhwd),
    [0x6a] = MMX_OP2(punpckhdq),
    [0x6b] = MMX_OP2(packssdw),
    [0x6c] = { NULL, gen_helper_punpcklqdq_xmm },
    [0x6d] = { NULL, gen_helper_punpckhqdq_xmm },
    [0x6e] = { SSE_SPECIAL, SSE_SPECIAL }, /* movd mm, ea */
    [0x6f] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movq, movdqa, , movqdu */
    [0x70] = { (SSEFunc_0_epp)gen_helper_pshufw_mmx,
               (SSEFunc_0_epp)gen_helper_pshufd_xmm,
               (SSEFunc_0_epp)gen_helper_pshufhw_xmm,
               (SSEFunc_0_epp)gen_helper_pshuflw_xmm }, /* XXX: casts */
    [0x71] = { SSE_SPECIAL, SSE_SPECIAL }, /* shiftw */
    [0x72] = { SSE_SPECIAL, SSE_SPECIAL }, /* shiftd */
    [0x73] = { SSE_SPECIAL, SSE_SPECIAL }, /* shiftq */
    [0x74] = MMX_OP2(pcmpeqb),
    [0x75] = MMX_OP2(pcmpeqw),
    [0x76] = MMX_OP2(pcmpeql),
    [0x77] = { SSE_DUMMY }, /* emms */
    [0x78] = { NULL, SSE_SPECIAL, NULL, SSE_SPECIAL }, /* extrq_i, insertq_i */
    [0x79] = { NULL, gen_helper_extrq_r, NULL, gen_helper_insertq_r },
    [0x7c] = { NULL, gen_helper_haddpd, NULL, gen_helper_haddps },
    [0x7d] = { NULL, gen_helper_hsubpd, NULL, gen_helper_hsubps },
    [0x7e] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movd, movd, , movq */
    [0x7f] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movq, movdqa, movdqu */
    [0xc4] = { SSE_SPECIAL, SSE_SPECIAL }, /* pinsrw */
    [0xc5] = { SSE_SPECIAL, SSE_SPECIAL }, /* pextrw */
    [0xd0] = { NULL, gen_helper_addsubpd, NULL, gen_helper_addsubps },
    [0xd1] = MMX_OP2(psrlw),
    [0xd2] = MMX_OP2(psrld),
    [0xd3] = MMX_OP2(psrlq),
    [0xd4] = MMX_OP2(paddq),
    [0xd5] = MMX_OP2(pmullw),
    [0xd6] = { NULL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL },
    [0xd7] = { SSE_SPECIAL, SSE_SPECIAL }, /* pmovmskb */
    [0xd8] = MMX_OP2(psubusb),
    [0xd9] = MMX_OP2(psubusw),
    [0xda] = MMX_OP2(pminub),
    [0xdb] = MMX_OP2(pand),
    [0xdc] = MMX_OP2(paddusb),
    [0xdd] = MMX_OP2(paddusw),
    [0xde] = MMX_OP2(pmaxub),
    [0xdf] = MMX_OP2(pandn),
    [0xe0] = MMX_OP2(pavgb),
    [0xe1] = MMX_OP2(psraw),
    [0xe2] = MMX_OP2(psrad),
    [0xe3] = MMX_OP2(pavgw),
    [0xe4] = MMX_OP2(pmulhuw),
    [0xe5] = MMX_OP2(pmulhw),
    [0xe6] = { NULL, gen_helper_cvttpd2dq, gen_helper_cvtdq2pd, gen_helper_cvtpd2dq },
    [0xe7] = { SSE_SPECIAL , SSE_SPECIAL },  /* movntq, movntq */
    [0xe8] = MMX_OP2(psubsb),
    [0xe9] = MMX_OP2(psubsw),
    [0xea] = MMX_OP2(pminsw),
    [0xeb] = MMX_OP2(por),
    [0xec] = MMX_OP2(paddsb),
    [0xed] = MMX_OP2(paddsw),
    [0xee] = MMX_OP2(pmaxsw),
    [0xef] = MMX_OP2(pxor),
    [0xf0] = { NULL, NULL, NULL, SSE_SPECIAL }, /* lddqu */
    [0xf1] = MMX_OP2(psllw),
    [0xf2] = MMX_OP2(pslld),
    [0xf3] = MMX_OP2(psllq),
    [0xf4] = MMX_OP2(pmuludq),
    [0xf5] = MMX_OP2(pmaddwd),
    [0xf6] = MMX_OP2(psadbw),
    [0xf7] = { (SSEFunc_0_epp)gen_helper_maskmov_mmx,
               (SSEFunc_0_epp)gen_helper_maskmov_xmm }, /* XXX: casts */
    [0xf8] = MMX_OP2(psubb),
    [0xf9] = MMX_OP2(psubw),
    [0xfa] = MMX_OP2(psubl),
    [0xfb] = MMX_OP2(psubq),
    [0xfc] = MMX_OP2(paddb),
    [0xfd] = MMX_OP2(paddw),
    [0xfe] = MMX_OP2(paddl),
};

static const SSEFunc_0_epp sse_op_table2[3 * 8][2] = {
    [0 + 2] = MMX_OP2(psrlw),
    [0 + 4] = MMX_OP2(psraw),
    [0 + 6] = MMX_OP2(psllw),
    [8 + 2] = MMX_OP2(psrld),
    [8 + 4] = MMX_OP2(psrad),
    [8 + 6] = MMX_OP2(pslld),
    [16 + 2] = MMX_OP2(psrlq),
    [16 + 3] = { NULL, gen_helper_psrldq_xmm },
    [16 + 6] = MMX_OP2(psllq),
    [16 + 7] = { NULL, gen_helper_pslldq_xmm },
};

static const SSEFunc_0_epi sse_op_table3ai[] = {
    gen_helper_cvtsi2ss,
    gen_helper_cvtsi2sd
};

#ifdef TARGET_X86_64
static const SSEFunc_0_epl sse_op_table3aq[] = {
    gen_helper_cvtsq2ss,
    gen_helper_cvtsq2sd
};
#endif

static const SSEFunc_i_ep sse_op_table3bi[] = {
    gen_helper_cvttss2si,
    gen_helper_cvtss2si,
    gen_helper_cvttsd2si,
    gen_helper_cvtsd2si
};

#ifdef TARGET_X86_64
static const SSEFunc_l_ep sse_op_table3bq[] = {
    gen_helper_cvttss2sq,
    gen_helper_cvtss2sq,
    gen_helper_cvttsd2sq,
    gen_helper_cvtsd2sq
};
#endif

static const SSEFunc_0_epp sse_op_table4[8][4] = {
    SSE_FOP(cmpeq),
    SSE_FOP(cmplt),
    SSE_FOP(cmple),
    SSE_FOP(cmpunord),
    SSE_FOP(cmpneq),
    SSE_FOP(cmpnlt),
    SSE_FOP(cmpnle),
    SSE_FOP(cmpord),
};

static const SSEFunc_0_epp sse_op_table5[256] = {
    [0x0c] = gen_helper_pi2fw,
    [0x0d] = gen_helper_pi2fd,
    [0x1c] = gen_helper_pf2iw,
    [0x1d] = gen_helper_pf2id,
    [0x8a] = gen_helper_pfnacc,
    [0x8e] = gen_helper_pfpnacc,
    [0x90] = gen_helper_pfcmpge,
    [0x94] = gen_helper_pfmin,
    [0x96] = gen_helper_pfrcp,
    [0x97] = gen_helper_pfrsqrt,
    [0x9a] = gen_helper_pfsub,
    [0x9e] = gen_helper_pfadd,
    [0xa0] = gen_helper_pfcmpgt,
    [0xa4] = gen_helper_pfmax,
    [0xa6] = gen_helper_movq, /* pfrcpit1; no need to actually increase precision */
    [0xa7] = gen_helper_movq, /* pfrsqit1 */
    [0xaa] = gen_helper_pfsubr,
    [0xae] = gen_helper_pfacc,
    [0xb0] = gen_helper_pfcmpeq,
    [0xb4] = gen_helper_pfmul,
    [0xb6] = gen_helper_movq, /* pfrcpit2 */
    [0xb7] = gen_helper_pmulhrw_mmx,
    [0xbb] = gen_helper_pswapd,
    [0xbf] = gen_helper_pavgb_mmx /* pavgusb */
};

struct SSEOpHelper_epp {
    SSEFunc_0_epp op[2];
    uint32_t ext_mask;
};

struct SSEOpHelper_eppi {
    SSEFunc_0_eppi op[2];
    uint32_t ext_mask;
};

#define SSSE3_OP(x) { MMX_OP2(x), CPUID_EXT_SSSE3 }
#define SSE41_OP(x) { { NULL, gen_helper_ ## x ## _xmm }, CPUID_EXT_SSE41 }
#define SSE42_OP(x) { { NULL, gen_helper_ ## x ## _xmm }, CPUID_EXT_SSE42 }
#define SSE41_SPECIAL { { NULL, SSE_SPECIAL }, CPUID_EXT_SSE41 }
#define PCLMULQDQ_OP(x) { { NULL, gen_helper_ ## x ## _xmm }, \
        CPUID_EXT_PCLMULQDQ }
#define AESNI_OP(x) { { NULL, gen_helper_ ## x ## _xmm }, CPUID_EXT_AES }

static const struct SSEOpHelper_epp sse_op_table6[256] = {
    [0x00] = SSSE3_OP(pshufb),
    [0x01] = SSSE3_OP(phaddw),
    [0x02] = SSSE3_OP(phaddd),
    [0x03] = SSSE3_OP(phaddsw),
    [0x04] = SSSE3_OP(pmaddubsw),
    [0x05] = SSSE3_OP(phsubw),
    [0x06] = SSSE3_OP(phsubd),
    [0x07] = SSSE3_OP(phsubsw),
    [0x08] = SSSE3_OP(psignb),
    [0x09] = SSSE3_OP(psignw),
    [0x0a] = SSSE3_OP(psignd),
    [0x0b] = SSSE3_OP(pmulhrsw),
    [0x10] = SSE41_OP(pblendvb),
    [0x14] = SSE41_OP(blendvps),
    [0x15] = SSE41_OP(blendvpd),
    [0x17] = SSE41_OP(ptest),
    [0x1c] = SSSE3_OP(pabsb),
    [0x1d] = SSSE3_OP(pabsw),
    [0x1e] = SSSE3_OP(pabsd),
    [0x20] = SSE41_OP(pmovsxbw),
    [0x21] = SSE41_OP(pmovsxbd),
    [0x22] = SSE41_OP(pmovsxbq),
    [0x23] = SSE41_OP(pmovsxwd),
    [0x24] = SSE41_OP(pmovsxwq),
    [0x25] = SSE41_OP(pmovsxdq),
    [0x28] = SSE41_OP(pmuldq),
    [0x29] = SSE41_OP(pcmpeqq),
    [0x2a] = SSE41_SPECIAL, /* movntqda */
    [0x2b] = SSE41_OP(packusdw),
    [0x30] = SSE41_OP(pmovzxbw),
    [0x31] = SSE41_OP(pmovzxbd),
    [0x32] = SSE41_OP(pmovzxbq),
    [0x33] = SSE41_OP(pmovzxwd),
    [0x34] = SSE41_OP(pmovzxwq),
    [0x35] = SSE41_OP(pmovzxdq),
    [0x37] = SSE42_OP(pcmpgtq),
    [0x38] = SSE41_OP(pminsb),
    [0x39] = SSE41_OP(pminsd),
    [0x3a] = SSE41_OP(pminuw),
    [0x3b] = SSE41_OP(pminud),
    [0x3c] = SSE41_OP(pmaxsb),
    [0x3d] = SSE41_OP(pmaxsd),
    [0x3e] = SSE41_OP(pmaxuw),
    [0x3f] = SSE41_OP(pmaxud),
    [0x40] = SSE41_OP(pmulld),
    [0x41] = SSE41_OP(phminposuw),
    [0xdb] = AESNI_OP(aesimc),
    [0xdc] = AESNI_OP(aesenc),
    [0xdd] = AESNI_OP(aesenclast),
    [0xde] = AESNI_OP(aesdec),
    [0xdf] = AESNI_OP(aesdeclast),
};

static const struct SSEOpHelper_eppi sse_op_table7[256] = {
    [0x08] = SSE41_OP(roundps),
    [0x09] = SSE41_OP(roundpd),
    [0x0a] = SSE41_OP(roundss),
    [0x0b] = SSE41_OP(roundsd),
    [0x0c] = SSE41_OP(blendps),
    [0x0d] = SSE41_OP(blendpd),
    [0x0e] = SSE41_OP(pblendw),
    [0x0f] = SSSE3_OP(palignr),
    [0x14] = SSE41_SPECIAL, /* pextrb */
    [0x15] = SSE41_SPECIAL, /* pextrw */
    [0x16] = SSE41_SPECIAL, /* pextrd/pextrq */
    [0x17] = SSE41_SPECIAL, /* extractps */
    [0x20] = SSE41_SPECIAL, /* pinsrb */
    [0x21] = SSE41_SPECIAL, /* insertps */
    [0x22] = SSE41_SPECIAL, /* pinsrd/pinsrq */
    [0x40] = SSE41_OP(dpps),
    [0x41] = SSE41_OP(dppd),
    [0x42] = SSE41_OP(mpsadbw),
    [0x44] = PCLMULQDQ_OP(pclmulqdq),
    [0x60] = SSE42_OP(pcmpestrm),
    [0x61] = SSE42_OP(pcmpestri),
    [0x62] = SSE42_OP(pcmpistrm),
    [0x63] = SSE42_OP(pcmpistri),
    [0xdf] = AESNI_OP(aeskeygenassist),
};

static void gen_sse(CPUX86State *env, DisasContext *s, int b)
{
    int op1_offset, op2_offset, val;
    int modrm, mod, rm, reg;
    SSEFunc_0_epp sse_fn_epp;
    SSEFunc_0_eppi sse_fn_eppi;
    SSEFunc_0_ppi sse_fn_ppi;
    SSEFunc_0_eppt sse_fn_eppt;
    TCGMemOp ot;

    b &= 0xff;
    const int b1 =
        s->prefix & PREFIX_DATA ? 1
        : s->prefix & PREFIX_REPZ ? 2
        : s->prefix & PREFIX_REPNZ ? 3
        : 0;
    const int is_xmm =
        (0x10 <= b && b <= 0x5f)
        || b == 0xc6
        || b == 0xc2
        || !!b1;
    sse_fn_epp = sse_op_table1[b][b1];
    if (!sse_fn_epp) {
        goto unknown_op;
    }
    /* simple MMX/SSE operation */
    if (s->flags & HF_TS_MASK) {
        gen_exception(s, EXCP07_PREX);
        return;
    }
    if (s->flags & HF_EM_MASK) {
    illegal_op:
        gen_illegal_opcode(s);
        return;
    }
    if (is_xmm
        && !(s->flags & HF_OSFXSR_MASK)
        && ((b != 0x38 && b != 0x3a) || (s->prefix & PREFIX_DATA))) {
        goto unknown_op;
    }
    if (b == 0x0e) {
        if (!(s->cpuid_ext2_features & CPUID_EXT2_3DNOW)) {
            /* If we were fully decoding this we might use illegal_op.  */
            goto unknown_op;
        }
        /* femms */
        gen_helper_emms(cpu_env);
        return;
    }
    if (b == 0x77) {
        /* emms */
        gen_helper_emms(cpu_env);
        return;
    }
    /* prepare MMX state (XXX: optimize by storing fptt and fptags in
       the static cpu state) */
    if (!is_xmm) {
        gen_helper_enter_mmx(cpu_env);
    }

    modrm = x86_ldub_code(env, s);
    reg = ((modrm >> 3) & 7);
    if (is_xmm) {
        reg |= REX_R(s);
    }
    mod = (modrm >> 6) & 3;
    if (sse_fn_epp == SSE_SPECIAL) {
        b |= (b1 << 8);
        switch(b) {
        case 0x0e7: /* movntq */
            if (mod == 3) {
                goto illegal_op;
            }
            gen_lea_modrm(env, s, modrm);
            gen_stq_env_A0(s, offsetof(CPUX86State, fpregs[reg].mmx));
            break;
        case 0x1e7: /* movntdq */
        case 0x02b: /* movntps */
        case 0x12b: /* movntps */
            if (mod == 3)
                goto illegal_op;
            gen_lea_modrm(env, s, modrm);
            gen_sto_env_A0(s, offsetof(CPUX86State, xmm_regs[reg]));
            break;
        case 0x3f0: /* lddqu */
            if (mod == 3)
                goto illegal_op;
            gen_lea_modrm(env, s, modrm);
            gen_ldo_env_A0(s, offsetof(CPUX86State, xmm_regs[reg]));
            break;
        case 0x22b: /* movntss */
        case 0x32b: /* movntsd */
            if (mod == 3)
                goto illegal_op;
            gen_lea_modrm(env, s, modrm);
            if (b1 & 1) {
                gen_stq_env_A0(s, offsetof(CPUX86State,
                                           xmm_regs[reg].ZMM_Q(0)));
            } else {
                tcg_gen_ld32u_tl(s->T0, cpu_env, offsetof(CPUX86State,
                    xmm_regs[reg].ZMM_L(0)));
                gen_op_st_v(s, MO_32, s->T0, s->A0);
            }
            break;
        case 0x6e: /* movd mm, ea */
#ifdef TARGET_X86_64
            if (s->dflag == MO_64) {
                gen_ldst_modrm(env, s, modrm, MO_64, OR_TMP0, 0);
                tcg_gen_st_tl(s->T0, cpu_env,
                              offsetof(CPUX86State, fpregs[reg].mmx));
            } else
#endif
            {
                gen_ldst_modrm(env, s, modrm, MO_32, OR_TMP0, 0);
                tcg_gen_addi_ptr(s->ptr0, cpu_env,
                                 offsetof(CPUX86State,fpregs[reg].mmx));
                tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
                gen_helper_movl_mm_T0_mmx(s->ptr0, s->tmp2_i32);
            }
            break;
        case 0x16e: /* movd xmm, ea */
#ifdef TARGET_X86_64
            if (s->dflag == MO_64) {
                gen_ldst_modrm(env, s, modrm, MO_64, OR_TMP0, 0);
                tcg_gen_addi_ptr(s->ptr0, cpu_env,
                                 offsetof(CPUX86State,xmm_regs[reg]));
                gen_helper_movq_mm_T0_xmm(s->ptr0, s->T0);
            } else
#endif
            {
                gen_ldst_modrm(env, s, modrm, MO_32, OR_TMP0, 0);
                tcg_gen_addi_ptr(s->ptr0, cpu_env,
                                 offsetof(CPUX86State,xmm_regs[reg]));
                tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
                gen_helper_movl_mm_T0_xmm(s->ptr0, s->tmp2_i32);
            }
            break;
        case 0x6f: /* movq mm, ea */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_ldq_env_A0(s, offsetof(CPUX86State, fpregs[reg].mmx));
            } else {
                rm = (modrm & 7);
                tcg_gen_ld_i64(s->tmp1_i64, cpu_env,
                               offsetof(CPUX86State,fpregs[rm].mmx));
                tcg_gen_st_i64(s->tmp1_i64, cpu_env,
                               offsetof(CPUX86State,fpregs[reg].mmx));
            }
            break;
        case 0x010: /* movups */
        case 0x110: /* movupd */
        case 0x028: /* movaps */
        case 0x128: /* movapd */
        case 0x16f: /* movdqa xmm, ea */
        case 0x26f: /* movdqu xmm, ea */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_ldo_env_A0(s, offsetof(CPUX86State, xmm_regs[reg]));
            } else {
                rm = (modrm & 7) | REX_B(s);
                gen_op_movo(s, offsetof(CPUX86State, xmm_regs[reg]),
                            offsetof(CPUX86State,xmm_regs[rm]));
            }
            break;
        case 0x210: /* movss xmm, ea */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_op_ld_v(s, MO_32, s->T0, s->A0);
                tcg_gen_st32_tl(s->T0, cpu_env,
                                offsetof(CPUX86State, xmm_regs[reg].ZMM_L(0)));
                tcg_gen_movi_tl(s->T0, 0);
                tcg_gen_st32_tl(s->T0, cpu_env,
                                offsetof(CPUX86State, xmm_regs[reg].ZMM_L(1)));
                tcg_gen_st32_tl(s->T0, cpu_env,
                                offsetof(CPUX86State, xmm_regs[reg].ZMM_L(2)));
                tcg_gen_st32_tl(s->T0, cpu_env,
                                offsetof(CPUX86State, xmm_regs[reg].ZMM_L(3)));
            } else {
                rm = (modrm & 7) | REX_B(s);
                gen_op_movl(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_L(0)),
                            offsetof(CPUX86State,xmm_regs[rm].ZMM_L(0)));
            }
            break;
        case 0x310: /* movsd xmm, ea */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_ldq_env_A0(s, offsetof(CPUX86State,
                                           xmm_regs[reg].ZMM_Q(0)));
                tcg_gen_movi_tl(s->T0, 0);
                tcg_gen_st32_tl(s->T0, cpu_env,
                                offsetof(CPUX86State, xmm_regs[reg].ZMM_L(2)));
                tcg_gen_st32_tl(s->T0, cpu_env,
                                offsetof(CPUX86State, xmm_regs[reg].ZMM_L(3)));
            } else {
                rm = (modrm & 7) | REX_B(s);
                gen_op_movq(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_Q(0)),
                            offsetof(CPUX86State,xmm_regs[rm].ZMM_Q(0)));
            }
            break;
        case 0x012: /* movlps */
        case 0x112: /* movlpd */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_ldq_env_A0(s, offsetof(CPUX86State,
                                           xmm_regs[reg].ZMM_Q(0)));
            } else {
                /* movhlps */
                rm = (modrm & 7) | REX_B(s);
                gen_op_movq(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_Q(0)),
                            offsetof(CPUX86State,xmm_regs[rm].ZMM_Q(1)));
            }
            break;
        case 0x212: /* movsldup */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_ldo_env_A0(s, offsetof(CPUX86State, xmm_regs[reg]));
            } else {
                rm = (modrm & 7) | REX_B(s);
                gen_op_movl(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_L(0)),
                            offsetof(CPUX86State,xmm_regs[rm].ZMM_L(0)));
                gen_op_movl(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_L(2)),
                            offsetof(CPUX86State,xmm_regs[rm].ZMM_L(2)));
            }
            gen_op_movl(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_L(1)),
                        offsetof(CPUX86State,xmm_regs[reg].ZMM_L(0)));
            gen_op_movl(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_L(3)),
                        offsetof(CPUX86State,xmm_regs[reg].ZMM_L(2)));
            break;
        case 0x312: /* movddup */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_ldq_env_A0(s, offsetof(CPUX86State,
                                           xmm_regs[reg].ZMM_Q(0)));
            } else {
                rm = (modrm & 7) | REX_B(s);
                gen_op_movq(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_Q(0)),
                            offsetof(CPUX86State,xmm_regs[rm].ZMM_Q(0)));
            }
            gen_op_movq(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_Q(1)),
                        offsetof(CPUX86State,xmm_regs[reg].ZMM_Q(0)));
            break;
        case 0x016: /* movhps */
        case 0x116: /* movhpd */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_ldq_env_A0(s, offsetof(CPUX86State,
                                           xmm_regs[reg].ZMM_Q(1)));
            } else {
                /* movlhps */
                rm = (modrm & 7) | REX_B(s);
                gen_op_movq(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_Q(1)),
                            offsetof(CPUX86State,xmm_regs[rm].ZMM_Q(0)));
            }
            break;
        case 0x216: /* movshdup */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_ldo_env_A0(s, offsetof(CPUX86State, xmm_regs[reg]));
            } else {
                rm = (modrm & 7) | REX_B(s);
                gen_op_movl(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_L(1)),
                            offsetof(CPUX86State,xmm_regs[rm].ZMM_L(1)));
                gen_op_movl(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_L(3)),
                            offsetof(CPUX86State,xmm_regs[rm].ZMM_L(3)));
            }
            gen_op_movl(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_L(0)),
                        offsetof(CPUX86State,xmm_regs[reg].ZMM_L(1)));
            gen_op_movl(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_L(2)),
                        offsetof(CPUX86State,xmm_regs[reg].ZMM_L(3)));
            break;
        case 0x178:
        case 0x378:
            {
                int bit_index, field_length;

                if (b1 == 1 && reg != 0)
                    goto illegal_op;
                field_length = x86_ldub_code(env, s) & 0x3F;
                bit_index = x86_ldub_code(env, s) & 0x3F;
                tcg_gen_addi_ptr(s->ptr0, cpu_env,
                    offsetof(CPUX86State,xmm_regs[reg]));
                if (b1 == 1)
                    gen_helper_extrq_i(cpu_env, s->ptr0,
                                       tcg_const_i32(bit_index),
                                       tcg_const_i32(field_length));
                else
                    gen_helper_insertq_i(cpu_env, s->ptr0,
                                         tcg_const_i32(bit_index),
                                         tcg_const_i32(field_length));
            }
            break;
        case 0x7e: /* movd ea, mm */
#ifdef TARGET_X86_64
            if (s->dflag == MO_64) {
                tcg_gen_ld_i64(s->T0, cpu_env,
                               offsetof(CPUX86State,fpregs[reg].mmx));
                gen_ldst_modrm(env, s, modrm, MO_64, OR_TMP0, 1);
            } else
#endif
            {
                tcg_gen_ld32u_tl(s->T0, cpu_env,
                                 offsetof(CPUX86State,fpregs[reg].mmx.MMX_L(0)));
                gen_ldst_modrm(env, s, modrm, MO_32, OR_TMP0, 1);
            }
            break;
        case 0x17e: /* movd ea, xmm */
#ifdef TARGET_X86_64
            if (s->dflag == MO_64) {
                tcg_gen_ld_i64(s->T0, cpu_env,
                               offsetof(CPUX86State,xmm_regs[reg].ZMM_Q(0)));
                gen_ldst_modrm(env, s, modrm, MO_64, OR_TMP0, 1);
            } else
#endif
            {
                tcg_gen_ld32u_tl(s->T0, cpu_env,
                                 offsetof(CPUX86State,xmm_regs[reg].ZMM_L(0)));
                gen_ldst_modrm(env, s, modrm, MO_32, OR_TMP0, 1);
            }
            break;
        case 0x27e: /* movq xmm, ea */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_ldq_env_A0(s, offsetof(CPUX86State,
                                           xmm_regs[reg].ZMM_Q(0)));
            } else {
                rm = (modrm & 7) | REX_B(s);
                gen_op_movq(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_Q(0)),
                            offsetof(CPUX86State,xmm_regs[rm].ZMM_Q(0)));
            }
            gen_op_movq_env_0(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_Q(1)));
            break;
        case 0x7f: /* movq ea, mm */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_stq_env_A0(s, offsetof(CPUX86State, fpregs[reg].mmx));
            } else {
                rm = (modrm & 7);
                gen_op_movq(s, offsetof(CPUX86State, fpregs[rm].mmx),
                            offsetof(CPUX86State,fpregs[reg].mmx));
            }
            break;
        case 0x011: /* movups */
        case 0x111: /* movupd */
        case 0x029: /* movaps */
        case 0x129: /* movapd */
        case 0x17f: /* movdqa ea, xmm */
        case 0x27f: /* movdqu ea, xmm */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_sto_env_A0(s, offsetof(CPUX86State, xmm_regs[reg]));
            } else {
                rm = (modrm & 7) | REX_B(s);
                gen_op_movo(s, offsetof(CPUX86State, xmm_regs[rm]),
                            offsetof(CPUX86State,xmm_regs[reg]));
            }
            break;
        case 0x211: /* movss ea, xmm */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                tcg_gen_ld32u_tl(s->T0, cpu_env,
                                 offsetof(CPUX86State, xmm_regs[reg].ZMM_L(0)));
                gen_op_st_v(s, MO_32, s->T0, s->A0);
            } else {
                rm = (modrm & 7) | REX_B(s);
                gen_op_movl(s, offsetof(CPUX86State, xmm_regs[rm].ZMM_L(0)),
                            offsetof(CPUX86State,xmm_regs[reg].ZMM_L(0)));
            }
            break;
        case 0x311: /* movsd ea, xmm */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_stq_env_A0(s, offsetof(CPUX86State,
                                           xmm_regs[reg].ZMM_Q(0)));
            } else {
                rm = (modrm & 7) | REX_B(s);
                gen_op_movq(s, offsetof(CPUX86State, xmm_regs[rm].ZMM_Q(0)),
                            offsetof(CPUX86State,xmm_regs[reg].ZMM_Q(0)));
            }
            break;
        case 0x013: /* movlps */
        case 0x113: /* movlpd */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_stq_env_A0(s, offsetof(CPUX86State,
                                           xmm_regs[reg].ZMM_Q(0)));
            } else {
                goto illegal_op;
            }
            break;
        case 0x017: /* movhps */
        case 0x117: /* movhpd */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_stq_env_A0(s, offsetof(CPUX86State,
                                           xmm_regs[reg].ZMM_Q(1)));
            } else {
                goto illegal_op;
            }
            break;
        case 0x71: /* shift mm, im */
        case 0x72:
        case 0x73:
        case 0x171: /* shift xmm, im */
        case 0x172:
        case 0x173:
            if (b1 >= 2) {
                goto unknown_op;
            }
            val = x86_ldub_code(env, s);
            if (is_xmm) {
                tcg_gen_movi_tl(s->T0, val);
                tcg_gen_st32_tl(s->T0, cpu_env,
                                offsetof(CPUX86State, xmm_t0.ZMM_L(0)));
                tcg_gen_movi_tl(s->T0, 0);
                tcg_gen_st32_tl(s->T0, cpu_env,
                                offsetof(CPUX86State, xmm_t0.ZMM_L(1)));
                op1_offset = offsetof(CPUX86State,xmm_t0);
            } else {
                tcg_gen_movi_tl(s->T0, val);
                tcg_gen_st32_tl(s->T0, cpu_env,
                                offsetof(CPUX86State, mmx_t0.MMX_L(0)));
                tcg_gen_movi_tl(s->T0, 0);
                tcg_gen_st32_tl(s->T0, cpu_env,
                                offsetof(CPUX86State, mmx_t0.MMX_L(1)));
                op1_offset = offsetof(CPUX86State,mmx_t0);
            }
            sse_fn_epp = sse_op_table2[((b - 1) & 3) * 8 +
                                       (((modrm >> 3)) & 7)][b1];
            if (!sse_fn_epp) {
                goto unknown_op;
            }
            if (is_xmm) {
                rm = (modrm & 7) | REX_B(s);
                op2_offset = offsetof(CPUX86State,xmm_regs[rm]);
            } else {
                rm = (modrm & 7);
                op2_offset = offsetof(CPUX86State,fpregs[rm].mmx);
            }
            tcg_gen_addi_ptr(s->ptr0, cpu_env, op2_offset);
            tcg_gen_addi_ptr(s->ptr1, cpu_env, op1_offset);
            sse_fn_epp(cpu_env, s->ptr0, s->ptr1);
            break;
        case 0x050: /* movmskps */
            rm = (modrm & 7) | REX_B(s);
            tcg_gen_addi_ptr(s->ptr0, cpu_env,
                             offsetof(CPUX86State,xmm_regs[rm]));
            gen_helper_movmskps(s->tmp2_i32, cpu_env, s->ptr0);
            tcg_gen_extu_i32_tl(cpu_regs[reg], s->tmp2_i32);
            break;
        case 0x150: /* movmskpd */
            rm = (modrm & 7) | REX_B(s);
            tcg_gen_addi_ptr(s->ptr0, cpu_env,
                             offsetof(CPUX86State,xmm_regs[rm]));
            gen_helper_movmskpd(s->tmp2_i32, cpu_env, s->ptr0);
            tcg_gen_extu_i32_tl(cpu_regs[reg], s->tmp2_i32);
            break;
        case 0x02a: /* cvtpi2ps */
        case 0x12a: /* cvtpi2pd */
            gen_helper_enter_mmx(cpu_env);
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                op2_offset = offsetof(CPUX86State,mmx_t0);
                gen_ldq_env_A0(s, op2_offset);
            } else {
                rm = (modrm & 7);
                op2_offset = offsetof(CPUX86State,fpregs[rm].mmx);
            }
            op1_offset = offsetof(CPUX86State,xmm_regs[reg]);
            tcg_gen_addi_ptr(s->ptr0, cpu_env, op1_offset);
            tcg_gen_addi_ptr(s->ptr1, cpu_env, op2_offset);
            switch(b >> 8) {
            case 0x0:
                gen_helper_cvtpi2ps(cpu_env, s->ptr0, s->ptr1);
                break;
            default:
            case 0x1:
                gen_helper_cvtpi2pd(cpu_env, s->ptr0, s->ptr1);
                break;
            }
            break;
        case 0x22a: /* cvtsi2ss */
        case 0x32a: /* cvtsi2sd */
            ot = mo_64_32(s->dflag);
            gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
            op1_offset = offsetof(CPUX86State,xmm_regs[reg]);
            tcg_gen_addi_ptr(s->ptr0, cpu_env, op1_offset);
            if (ot == MO_32) {
                SSEFunc_0_epi sse_fn_epi = sse_op_table3ai[(b >> 8) & 1];
                tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
                sse_fn_epi(cpu_env, s->ptr0, s->tmp2_i32);
            } else {
#ifdef TARGET_X86_64
                SSEFunc_0_epl sse_fn_epl = sse_op_table3aq[(b >> 8) & 1];
                sse_fn_epl(cpu_env, s->ptr0, s->T0);
#else
                goto illegal_op;
#endif
            }
            break;
        case 0x02c: /* cvttps2pi */
        case 0x12c: /* cvttpd2pi */
        case 0x02d: /* cvtps2pi */
        case 0x12d: /* cvtpd2pi */
            gen_helper_enter_mmx(cpu_env);
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                op2_offset = offsetof(CPUX86State,xmm_t0);
                gen_ldo_env_A0(s, op2_offset);
            } else {
                rm = (modrm & 7) | REX_B(s);
                op2_offset = offsetof(CPUX86State,xmm_regs[rm]);
            }
            op1_offset = offsetof(CPUX86State,fpregs[reg & 7].mmx);
            tcg_gen_addi_ptr(s->ptr0, cpu_env, op1_offset);
            tcg_gen_addi_ptr(s->ptr1, cpu_env, op2_offset);
            switch(b) {
            case 0x02c:
                gen_helper_cvttps2pi(cpu_env, s->ptr0, s->ptr1);
                break;
            case 0x12c:
                gen_helper_cvttpd2pi(cpu_env, s->ptr0, s->ptr1);
                break;
            case 0x02d:
                gen_helper_cvtps2pi(cpu_env, s->ptr0, s->ptr1);
                break;
            case 0x12d:
                gen_helper_cvtpd2pi(cpu_env, s->ptr0, s->ptr1);
                break;
            }
            break;
        case 0x22c: /* cvttss2si */
        case 0x32c: /* cvttsd2si */
        case 0x22d: /* cvtss2si */
        case 0x32d: /* cvtsd2si */
            ot = mo_64_32(s->dflag);
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                if ((b >> 8) & 1) {
                    gen_ldq_env_A0(s, offsetof(CPUX86State, xmm_t0.ZMM_Q(0)));
                } else {
                    gen_op_ld_v(s, MO_32, s->T0, s->A0);
                    tcg_gen_st32_tl(s->T0, cpu_env,
                                    offsetof(CPUX86State, xmm_t0.ZMM_L(0)));
                }
                op2_offset = offsetof(CPUX86State,xmm_t0);
            } else {
                rm = (modrm & 7) | REX_B(s);
                op2_offset = offsetof(CPUX86State,xmm_regs[rm]);
            }
            tcg_gen_addi_ptr(s->ptr0, cpu_env, op2_offset);
            if (ot == MO_32) {
                SSEFunc_i_ep sse_fn_i_ep =
                    sse_op_table3bi[((b >> 7) & 2) | (b & 1)];
                sse_fn_i_ep(s->tmp2_i32, cpu_env, s->ptr0);
                tcg_gen_extu_i32_tl(s->T0, s->tmp2_i32);
            } else {
#ifdef TARGET_X86_64
                SSEFunc_l_ep sse_fn_l_ep =
                    sse_op_table3bq[((b >> 7) & 2) | (b & 1)];
                sse_fn_l_ep(s->T0, cpu_env, s->ptr0);
#else
                goto illegal_op;
#endif
            }
            gen_op_mov_reg_v(s, ot, reg, s->T0);
            break;
        case 0xc4: /* pinsrw */
        case 0x1c4:
            s->rip_offset = 1;
            gen_ldst_modrm(env, s, modrm, MO_16, OR_TMP0, 0);
            val = x86_ldub_code(env, s);
            if (b1) {
                val &= 7;
                tcg_gen_st16_tl(s->T0, cpu_env,
                                offsetof(CPUX86State,xmm_regs[reg].ZMM_W(val)));
            } else {
                val &= 3;
                tcg_gen_st16_tl(s->T0, cpu_env,
                                offsetof(CPUX86State,fpregs[reg].mmx.MMX_W(val)));
            }
            break;
        case 0xc5: /* pextrw */
        case 0x1c5:
            if (mod != 3)
                goto illegal_op;
            ot = mo_64_32(s->dflag);
            val = x86_ldub_code(env, s);
            if (b1) {
                val &= 7;
                rm = (modrm & 7) | REX_B(s);
                tcg_gen_ld16u_tl(s->T0, cpu_env,
                                 offsetof(CPUX86State,xmm_regs[rm].ZMM_W(val)));
            } else {
                val &= 3;
                rm = (modrm & 7);
                tcg_gen_ld16u_tl(s->T0, cpu_env,
                                offsetof(CPUX86State,fpregs[rm].mmx.MMX_W(val)));
            }
            reg = ((modrm >> 3) & 7) | REX_R(s);
            gen_op_mov_reg_v(s, ot, reg, s->T0);
            break;
        case 0x1d6: /* movq ea, xmm */
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_stq_env_A0(s, offsetof(CPUX86State,
                                           xmm_regs[reg].ZMM_Q(0)));
            } else {
                rm = (modrm & 7) | REX_B(s);
                gen_op_movq(s, offsetof(CPUX86State, xmm_regs[rm].ZMM_Q(0)),
                            offsetof(CPUX86State,xmm_regs[reg].ZMM_Q(0)));
                gen_op_movq_env_0(s,
                                  offsetof(CPUX86State, xmm_regs[rm].ZMM_Q(1)));
            }
            break;
        case 0x2d6: /* movq2dq */
            gen_helper_enter_mmx(cpu_env);
            rm = (modrm & 7);
            gen_op_movq(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_Q(0)),
                        offsetof(CPUX86State,fpregs[rm].mmx));
            gen_op_movq_env_0(s, offsetof(CPUX86State, xmm_regs[reg].ZMM_Q(1)));
            break;
        case 0x3d6: /* movdq2q */
            gen_helper_enter_mmx(cpu_env);
            rm = (modrm & 7) | REX_B(s);
            gen_op_movq(s, offsetof(CPUX86State, fpregs[reg & 7].mmx),
                        offsetof(CPUX86State,xmm_regs[rm].ZMM_Q(0)));
            break;
        case 0xd7: /* pmovmskb */
        case 0x1d7:
            if (mod != 3)
                goto illegal_op;
            if (b1) {
                rm = (modrm & 7) | REX_B(s);
                tcg_gen_addi_ptr(s->ptr0, cpu_env,
                                 offsetof(CPUX86State, xmm_regs[rm]));
                gen_helper_pmovmskb_xmm(s->tmp2_i32, cpu_env, s->ptr0);
            } else {
                rm = (modrm & 7);
                tcg_gen_addi_ptr(s->ptr0, cpu_env,
                                 offsetof(CPUX86State, fpregs[rm].mmx));
                gen_helper_pmovmskb_mmx(s->tmp2_i32, cpu_env, s->ptr0);
            }
            reg = ((modrm >> 3) & 7) | REX_R(s);
            tcg_gen_extu_i32_tl(cpu_regs[reg], s->tmp2_i32);
            break;

        case 0x138:
        case 0x038:
            b = modrm;
            if ((b & 0xf0) == 0xf0) {
                goto do_0f_38_fx;
            }
            modrm = x86_ldub_code(env, s);
            rm = modrm & 7;
            reg = ((modrm >> 3) & 7) | REX_R(s);
            mod = (modrm >> 6) & 3;
            if (b1 >= 2) {
                goto unknown_op;
            }

            sse_fn_epp = sse_op_table6[b].op[b1];
            if (!sse_fn_epp) {
                goto unknown_op;
            }
            if (!(s->cpuid_ext_features & sse_op_table6[b].ext_mask))
                goto illegal_op;

            if (b1) {
                op1_offset = offsetof(CPUX86State,xmm_regs[reg]);
                if (mod == 3) {
                    op2_offset = offsetof(CPUX86State,xmm_regs[rm | REX_B(s)]);
                } else {
                    op2_offset = offsetof(CPUX86State,xmm_t0);
                    gen_lea_modrm(env, s, modrm);
                    switch (b) {
                    case 0x20: case 0x30: /* pmovsxbw, pmovzxbw */
                    case 0x23: case 0x33: /* pmovsxwd, pmovzxwd */
                    case 0x25: case 0x35: /* pmovsxdq, pmovzxdq */
                        gen_ldq_env_A0(s, op2_offset +
                                        offsetof(ZMMReg, ZMM_Q(0)));
                        break;
                    case 0x21: case 0x31: /* pmovsxbd, pmovzxbd */
                    case 0x24: case 0x34: /* pmovsxwq, pmovzxwq */
                        tcg_gen_qemu_ld_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LEUL);
                        tcg_gen_st_i32(s->tmp2_i32, cpu_env, op2_offset +
                                        offsetof(ZMMReg, ZMM_L(0)));
                        break;
                    case 0x22: case 0x32: /* pmovsxbq, pmovzxbq */
                        tcg_gen_qemu_ld_tl(s->tmp0, s->A0,
                                           s->mem_index, MO_LEUW);
                        tcg_gen_st16_tl(s->tmp0, cpu_env, op2_offset +
                                        offsetof(ZMMReg, ZMM_W(0)));
                        break;
                    case 0x2a:            /* movntqda */
                        gen_ldo_env_A0(s, op1_offset);
                        return;
                    default:
                        gen_ldo_env_A0(s, op2_offset);
                    }
                }
            } else {
                op1_offset = offsetof(CPUX86State,fpregs[reg].mmx);
                if (mod == 3) {
                    op2_offset = offsetof(CPUX86State,fpregs[rm].mmx);
                } else {
                    op2_offset = offsetof(CPUX86State,mmx_t0);
                    gen_lea_modrm(env, s, modrm);
                    gen_ldq_env_A0(s, op2_offset);
                }
            }
            if (sse_fn_epp == SSE_SPECIAL) {
                goto unknown_op;
            }

            tcg_gen_addi_ptr(s->ptr0, cpu_env, op1_offset);
            tcg_gen_addi_ptr(s->ptr1, cpu_env, op2_offset);
            sse_fn_epp(cpu_env, s->ptr0, s->ptr1);

            if (b == 0x17) {
                set_cc_op(s, CC_OP_EFLAGS);
            }
            break;

        case 0x238:
        case 0x338:
        do_0f_38_fx:
            /* Various integer extensions at 0f 38 f[0-f].  */
            b = modrm | (b1 << 8);
            modrm = x86_ldub_code(env, s);
            reg = ((modrm >> 3) & 7) | REX_R(s);

            switch (b) {
            case 0x3f0: /* crc32 Gd,Eb */
            case 0x3f1: /* crc32 Gd,Ey */
            do_crc32:
                if (!(s->cpuid_ext_features & CPUID_EXT_SSE42)) {
                    goto illegal_op;
                }
                if ((b & 0xff) == 0xf0) {
                    ot = MO_8;
                } else if (s->dflag != MO_64) {
                    ot = (s->prefix & PREFIX_DATA ? MO_16 : MO_32);
                } else {
                    ot = MO_64;
                }

                tcg_gen_trunc_tl_i32(s->tmp2_i32, cpu_regs[reg]);
                gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
                gen_helper_crc32(s->T0, s->tmp2_i32,
                                 s->T0, tcg_const_i32(8 << ot));

                ot = mo_64_32(s->dflag);
                gen_op_mov_reg_v(s, ot, reg, s->T0);
                break;

            case 0x1f0: /* crc32 or movbe */
            case 0x1f1:
                /* For these insns, the f3 prefix is supposed to have priority
                   over the 66 prefix, but that's not what we implement above
                   setting b1.  */
                if (s->prefix & PREFIX_REPNZ) {
                    goto do_crc32;
                }
                /* FALLTHRU */
            case 0x0f0: /* movbe Gy,My */
            case 0x0f1: /* movbe My,Gy */
                if (!(s->cpuid_ext_features & CPUID_EXT_MOVBE)) {
                    goto illegal_op;
                }
                if (s->dflag != MO_64) {
                    ot = (s->prefix & PREFIX_DATA ? MO_16 : MO_32);
                } else {
                    ot = MO_64;
                }

                gen_lea_modrm(env, s, modrm);
                if ((b & 1) == 0) {
                    tcg_gen_qemu_ld_tl(s->T0, s->A0,
                                       s->mem_index, ot | MO_BE);
                    gen_op_mov_reg_v(s, ot, reg, s->T0);
                } else {
                    tcg_gen_qemu_st_tl(cpu_regs[reg], s->A0,
                                       s->mem_index, ot | MO_BE);
                }
                break;

            case 0x0f2: /* andn Gy, By, Ey */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_BMI1)
                    || !(s->prefix & PREFIX_VEX)
                    || s->vex_l != 0) {
                    goto illegal_op;
                }
                ot = mo_64_32(s->dflag);
                gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
                tcg_gen_andc_tl(s->T0, s->T0, cpu_regs[s->vex_v]);
                gen_op_mov_reg_v(s, ot, reg, s->T0);
                gen_op_update1_cc(s);
                set_cc_op(s, CC_OP_LOGICB + ot);
                break;

            case 0x0f7: /* bextr Gy, Ey, By */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_BMI1)
                    || !(s->prefix & PREFIX_VEX)
                    || s->vex_l != 0) {
                    goto illegal_op;
                }
                ot = mo_64_32(s->dflag);
                {
                    TCGv bound, zero;

                    gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
                    /* Extract START, and shift the operand.
                       Shifts larger than operand size get zeros.  */
                    tcg_gen_ext8u_tl(s->A0, cpu_regs[s->vex_v]);
                    tcg_gen_shr_tl(s->T0, s->T0, s->A0);

                    bound = tcg_const_tl(ot == MO_64 ? 63 : 31);
                    zero = tcg_const_tl(0);
                    tcg_gen_movcond_tl(TCG_COND_LEU, s->T0, s->A0, bound,
                                       s->T0, zero);
                    tcg_temp_free(zero);

                    /* Extract the LEN into a mask.  Lengths larger than
                       operand size get all ones.  */
                    tcg_gen_extract_tl(s->A0, cpu_regs[s->vex_v], 8, 8);
                    tcg_gen_movcond_tl(TCG_COND_LEU, s->A0, s->A0, bound,
                                       s->A0, bound);
                    tcg_temp_free(bound);
                    tcg_gen_movi_tl(s->T1, 1);
                    tcg_gen_shl_tl(s->T1, s->T1, s->A0);
                    tcg_gen_subi_tl(s->T1, s->T1, 1);
                    tcg_gen_and_tl(s->T0, s->T0, s->T1);

                    gen_op_mov_reg_v(s, ot, reg, s->T0);
                    gen_op_update1_cc(s);
                    set_cc_op(s, CC_OP_LOGICB + ot);
                }
                break;

            case 0x0f5: /* bzhi Gy, Ey, By */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_BMI2)
                    || !(s->prefix & PREFIX_VEX)
                    || s->vex_l != 0) {
                    goto illegal_op;
                }
                ot = mo_64_32(s->dflag);
                gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
                tcg_gen_ext8u_tl(s->T1, cpu_regs[s->vex_v]);
                {
                    TCGv bound = tcg_const_tl(ot == MO_64 ? 63 : 31);
                    /* Note that since we're using BMILG (in order to get O
                       cleared) we need to store the inverse into C.  */
                    tcg_gen_setcond_tl(TCG_COND_LT, cpu_cc_src,
                                       s->T1, bound);
                    tcg_gen_movcond_tl(TCG_COND_GT, s->T1, s->T1,
                                       bound, bound, s->T1);
                    tcg_temp_free(bound);
                }
                tcg_gen_movi_tl(s->A0, -1);
                tcg_gen_shl_tl(s->A0, s->A0, s->T1);
                tcg_gen_andc_tl(s->T0, s->T0, s->A0);
                gen_op_mov_reg_v(s, ot, reg, s->T0);
                gen_op_update1_cc(s);
                set_cc_op(s, CC_OP_BMILGB + ot);
                break;

            case 0x3f6: /* mulx By, Gy, rdx, Ey */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_BMI2)
                    || !(s->prefix & PREFIX_VEX)
                    || s->vex_l != 0) {
                    goto illegal_op;
                }
                ot = mo_64_32(s->dflag);
                gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
                switch (ot) {
                default:
                    tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
                    tcg_gen_trunc_tl_i32(s->tmp3_i32, cpu_regs[R_EDX]);
                    tcg_gen_mulu2_i32(s->tmp2_i32, s->tmp3_i32,
                                      s->tmp2_i32, s->tmp3_i32);
                    tcg_gen_extu_i32_tl(cpu_regs[s->vex_v], s->tmp2_i32);
                    tcg_gen_extu_i32_tl(cpu_regs[reg], s->tmp3_i32);
                    break;
#ifdef TARGET_X86_64
                case MO_64:
                    tcg_gen_mulu2_i64(s->T0, s->T1,
                                      s->T0, cpu_regs[R_EDX]);
                    tcg_gen_mov_i64(cpu_regs[s->vex_v], s->T0);
                    tcg_gen_mov_i64(cpu_regs[reg], s->T1);
                    break;
#endif
                }
                break;

            case 0x3f5: /* pdep Gy, By, Ey */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_BMI2)
                    || !(s->prefix & PREFIX_VEX)
                    || s->vex_l != 0) {
                    goto illegal_op;
                }
                ot = mo_64_32(s->dflag);
                gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
                /* Note that by zero-extending the mask operand, we
                   automatically handle zero-extending the result.  */
                if (ot == MO_64) {
                    tcg_gen_mov_tl(s->T1, cpu_regs[s->vex_v]);
                } else {
                    tcg_gen_ext32u_tl(s->T1, cpu_regs[s->vex_v]);
                }
                gen_helper_pdep(cpu_regs[reg], s->T0, s->T1);
                break;

            case 0x2f5: /* pext Gy, By, Ey */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_BMI2)
                    || !(s->prefix & PREFIX_VEX)
                    || s->vex_l != 0) {
                    goto illegal_op;
                }
                ot = mo_64_32(s->dflag);
                gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
                /* Note that by zero-extending the mask operand, we
                   automatically handle zero-extending the result.  */
                if (ot == MO_64) {
                    tcg_gen_mov_tl(s->T1, cpu_regs[s->vex_v]);
                } else {
                    tcg_gen_ext32u_tl(s->T1, cpu_regs[s->vex_v]);
                }
                gen_helper_pext(cpu_regs[reg], s->T0, s->T1);
                break;

            case 0x1f6: /* adcx Gy, Ey */
            case 0x2f6: /* adox Gy, Ey */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_ADX)) {
                    goto illegal_op;
                } else {
                    TCGv carry_in, carry_out, zero;
                    int end_op;

                    ot = mo_64_32(s->dflag);
                    gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);

                    /* Re-use the carry-out from a previous round.  */
                    carry_in = NULL;
                    carry_out = (b == 0x1f6 ? cpu_cc_dst : cpu_cc_src2);
                    switch (s->cc_op) {
                    case CC_OP_ADCX:
                        if (b == 0x1f6) {
                            carry_in = cpu_cc_dst;
                            end_op = CC_OP_ADCX;
                        } else {
                            end_op = CC_OP_ADCOX;
                        }
                        break;
                    case CC_OP_ADOX:
                        if (b == 0x1f6) {
                            end_op = CC_OP_ADCOX;
                        } else {
                            carry_in = cpu_cc_src2;
                            end_op = CC_OP_ADOX;
                        }
                        break;
                    case CC_OP_ADCOX:
                        end_op = CC_OP_ADCOX;
                        carry_in = carry_out;
                        break;
                    default:
                        end_op = (b == 0x1f6 ? CC_OP_ADCX : CC_OP_ADOX);
                        break;
                    }
                    /* If we can't reuse carry-out, get it out of EFLAGS.  */
                    if (!carry_in) {
                        if (s->cc_op != CC_OP_ADCX && s->cc_op != CC_OP_ADOX) {
                            gen_compute_eflags(s);
                        }
                        carry_in = s->tmp0;
                        tcg_gen_extract_tl(carry_in, cpu_cc_src,
                                           ctz32(b == 0x1f6 ? CC_C : CC_O), 1);
                    }

                    switch (ot) {
#ifdef TARGET_X86_64
                    case MO_32:
                        /* If we know TL is 64-bit, and we want a 32-bit
                           result, just do everything in 64-bit arithmetic.  */
                        tcg_gen_ext32u_i64(cpu_regs[reg], cpu_regs[reg]);
                        tcg_gen_ext32u_i64(s->T0, s->T0);
                        tcg_gen_add_i64(s->T0, s->T0, cpu_regs[reg]);
                        tcg_gen_add_i64(s->T0, s->T0, carry_in);
                        tcg_gen_ext32u_i64(cpu_regs[reg], s->T0);
                        tcg_gen_shri_i64(carry_out, s->T0, 32);
                        break;
#endif
                    default:
                        /* Otherwise compute the carry-out in two steps.  */
                        zero = tcg_const_tl(0);
                        tcg_gen_add2_tl(s->T0, carry_out,
                                        s->T0, zero,
                                        carry_in, zero);
                        tcg_gen_add2_tl(cpu_regs[reg], carry_out,
                                        cpu_regs[reg], carry_out,
                                        s->T0, zero);
                        tcg_temp_free(zero);
                        break;
                    }
                    set_cc_op(s, end_op);
                }
                break;

            case 0x1f7: /* shlx Gy, Ey, By */
            case 0x2f7: /* sarx Gy, Ey, By */
            case 0x3f7: /* shrx Gy, Ey, By */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_BMI2)
                    || !(s->prefix & PREFIX_VEX)
                    || s->vex_l != 0) {
                    goto illegal_op;
                }
                ot = mo_64_32(s->dflag);
                gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
                if (ot == MO_64) {
                    tcg_gen_andi_tl(s->T1, cpu_regs[s->vex_v], 63);
                } else {
                    tcg_gen_andi_tl(s->T1, cpu_regs[s->vex_v], 31);
                }
                if (b == 0x1f7) {
                    tcg_gen_shl_tl(s->T0, s->T0, s->T1);
                } else if (b == 0x2f7) {
                    if (ot != MO_64) {
                        tcg_gen_ext32s_tl(s->T0, s->T0);
                    }
                    tcg_gen_sar_tl(s->T0, s->T0, s->T1);
                } else {
                    if (ot != MO_64) {
                        tcg_gen_ext32u_tl(s->T0, s->T0);
                    }
                    tcg_gen_shr_tl(s->T0, s->T0, s->T1);
                }
                gen_op_mov_reg_v(s, ot, reg, s->T0);
                break;

            case 0x0f3:
            case 0x1f3:
            case 0x2f3:
            case 0x3f3: /* Group 17 */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_BMI1)
                    || !(s->prefix & PREFIX_VEX)
                    || s->vex_l != 0) {
                    goto illegal_op;
                }
                ot = mo_64_32(s->dflag);
                gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);

                tcg_gen_mov_tl(cpu_cc_src, s->T0);
                switch (reg & 7) {
                case 1: /* blsr By,Ey */
                    tcg_gen_subi_tl(s->T1, s->T0, 1);
                    tcg_gen_and_tl(s->T0, s->T0, s->T1);
                    break;
                case 2: /* blsmsk By,Ey */
                    tcg_gen_subi_tl(s->T1, s->T0, 1);
                    tcg_gen_xor_tl(s->T0, s->T0, s->T1);
                    break;
                case 3: /* blsi By, Ey */
                    tcg_gen_neg_tl(s->T1, s->T0);
                    tcg_gen_and_tl(s->T0, s->T0, s->T1);
                    break;
                default:
                    goto unknown_op;
                }
                tcg_gen_mov_tl(cpu_cc_dst, s->T0);
                gen_op_mov_reg_v(s, ot, s->vex_v, s->T0);
                set_cc_op(s, CC_OP_BMILGB + ot);
                break;

            default:
                goto unknown_op;
            }
            break;

        case 0x03a:
        case 0x13a:
            b = modrm;
            modrm = x86_ldub_code(env, s);
            rm = modrm & 7;
            reg = ((modrm >> 3) & 7) | REX_R(s);
            mod = (modrm >> 6) & 3;
            if (b1 >= 2) {
                goto unknown_op;
            }

            sse_fn_eppi = sse_op_table7[b].op[b1];
            if (!sse_fn_eppi) {
                goto unknown_op;
            }
            if (!(s->cpuid_ext_features & sse_op_table7[b].ext_mask))
                goto illegal_op;

            s->rip_offset = 1;

            if (sse_fn_eppi == SSE_SPECIAL) {
                ot = mo_64_32(s->dflag);
                rm = (modrm & 7) | REX_B(s);
                if (mod != 3)
                    gen_lea_modrm(env, s, modrm);
                reg = ((modrm >> 3) & 7) | REX_R(s);
                val = x86_ldub_code(env, s);
                switch (b) {
                case 0x14: /* pextrb */
                    tcg_gen_ld8u_tl(s->T0, cpu_env, offsetof(CPUX86State,
                                            xmm_regs[reg].ZMM_B(val & 15)));
                    if (mod == 3) {
                        gen_op_mov_reg_v(s, ot, rm, s->T0);
                    } else {
                        tcg_gen_qemu_st_tl(s->T0, s->A0,
                                           s->mem_index, MO_UB);
                    }
                    break;
                case 0x15: /* pextrw */
                    tcg_gen_ld16u_tl(s->T0, cpu_env, offsetof(CPUX86State,
                                            xmm_regs[reg].ZMM_W(val & 7)));
                    if (mod == 3) {
                        gen_op_mov_reg_v(s, ot, rm, s->T0);
                    } else {
                        tcg_gen_qemu_st_tl(s->T0, s->A0,
                                           s->mem_index, MO_LEUW);
                    }
                    break;
                case 0x16:
                    if (ot == MO_32) { /* pextrd */
                        tcg_gen_ld_i32(s->tmp2_i32, cpu_env,
                                        offsetof(CPUX86State,
                                                xmm_regs[reg].ZMM_L(val & 3)));
                        if (mod == 3) {
                            tcg_gen_extu_i32_tl(cpu_regs[rm], s->tmp2_i32);
                        } else {
                            tcg_gen_qemu_st_i32(s->tmp2_i32, s->A0,
                                                s->mem_index, MO_LEUL);
                        }
                    } else { /* pextrq */
#ifdef TARGET_X86_64
                        tcg_gen_ld_i64(s->tmp1_i64, cpu_env,
                                        offsetof(CPUX86State,
                                                xmm_regs[reg].ZMM_Q(val & 1)));
                        if (mod == 3) {
                            tcg_gen_mov_i64(cpu_regs[rm], s->tmp1_i64);
                        } else {
                            tcg_gen_qemu_st_i64(s->tmp1_i64, s->A0,
                                                s->mem_index, MO_LEQ);
                        }
#else
                        goto illegal_op;
#endif
                    }
                    break;
                case 0x17: /* extractps */
                    tcg_gen_ld32u_tl(s->T0, cpu_env, offsetof(CPUX86State,
                                            xmm_regs[reg].ZMM_L(val & 3)));
                    if (mod == 3) {
                        gen_op_mov_reg_v(s, ot, rm, s->T0);
                    } else {
                        tcg_gen_qemu_st_tl(s->T0, s->A0,
                                           s->mem_index, MO_LEUL);
                    }
                    break;
                case 0x20: /* pinsrb */
                    if (mod == 3) {
                        gen_op_mov_v_reg(s, MO_32, s->T0, rm);
                    } else {
                        tcg_gen_qemu_ld_tl(s->T0, s->A0,
                                           s->mem_index, MO_UB);
                    }
                    tcg_gen_st8_tl(s->T0, cpu_env, offsetof(CPUX86State,
                                            xmm_regs[reg].ZMM_B(val & 15)));
                    break;
                case 0x21: /* insertps */
                    if (mod == 3) {
                        tcg_gen_ld_i32(s->tmp2_i32, cpu_env,
                                        offsetof(CPUX86State,xmm_regs[rm]
                                                .ZMM_L((val >> 6) & 3)));
                    } else {
                        tcg_gen_qemu_ld_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LEUL);
                    }
                    tcg_gen_st_i32(s->tmp2_i32, cpu_env,
                                    offsetof(CPUX86State,xmm_regs[reg]
                                            .ZMM_L((val >> 4) & 3)));
                    if ((val >> 0) & 1)
                        tcg_gen_st_i32(tcg_const_i32(0 /*float32_zero*/),
                                        cpu_env, offsetof(CPUX86State,
                                                xmm_regs[reg].ZMM_L(0)));
                    if ((val >> 1) & 1)
                        tcg_gen_st_i32(tcg_const_i32(0 /*float32_zero*/),
                                        cpu_env, offsetof(CPUX86State,
                                                xmm_regs[reg].ZMM_L(1)));
                    if ((val >> 2) & 1)
                        tcg_gen_st_i32(tcg_const_i32(0 /*float32_zero*/),
                                        cpu_env, offsetof(CPUX86State,
                                                xmm_regs[reg].ZMM_L(2)));
                    if ((val >> 3) & 1)
                        tcg_gen_st_i32(tcg_const_i32(0 /*float32_zero*/),
                                        cpu_env, offsetof(CPUX86State,
                                                xmm_regs[reg].ZMM_L(3)));
                    break;
                case 0x22:
                    if (ot == MO_32) { /* pinsrd */
                        if (mod == 3) {
                            tcg_gen_trunc_tl_i32(s->tmp2_i32, cpu_regs[rm]);
                        } else {
                            tcg_gen_qemu_ld_i32(s->tmp2_i32, s->A0,
                                                s->mem_index, MO_LEUL);
                        }
                        tcg_gen_st_i32(s->tmp2_i32, cpu_env,
                                        offsetof(CPUX86State,
                                                xmm_regs[reg].ZMM_L(val & 3)));
                    } else { /* pinsrq */
#ifdef TARGET_X86_64
                        if (mod == 3) {
                            gen_op_mov_v_reg(s, ot, s->tmp1_i64, rm);
                        } else {
                            tcg_gen_qemu_ld_i64(s->tmp1_i64, s->A0,
                                                s->mem_index, MO_LEQ);
                        }
                        tcg_gen_st_i64(s->tmp1_i64, cpu_env,
                                        offsetof(CPUX86State,
                                                xmm_regs[reg].ZMM_Q(val & 1)));
#else
                        goto illegal_op;
#endif
                    }
                    break;
                }
                return;
            }

            if (b1) {
                op1_offset = offsetof(CPUX86State,xmm_regs[reg]);
                if (mod == 3) {
                    op2_offset = offsetof(CPUX86State,xmm_regs[rm | REX_B(s)]);
                } else {
                    op2_offset = offsetof(CPUX86State,xmm_t0);
                    gen_lea_modrm(env, s, modrm);
                    gen_ldo_env_A0(s, op2_offset);
                }
            } else {
                op1_offset = offsetof(CPUX86State,fpregs[reg].mmx);
                if (mod == 3) {
                    op2_offset = offsetof(CPUX86State,fpregs[rm].mmx);
                } else {
                    op2_offset = offsetof(CPUX86State,mmx_t0);
                    gen_lea_modrm(env, s, modrm);
                    gen_ldq_env_A0(s, op2_offset);
                }
            }
            val = x86_ldub_code(env, s);

            if ((b & 0xfc) == 0x60) { /* pcmpXstrX */
                set_cc_op(s, CC_OP_EFLAGS);

                if (s->dflag == MO_64) {
                    /* The helper must use entire 64-bit gp registers */
                    val |= 1 << 8;
                }
            }

            tcg_gen_addi_ptr(s->ptr0, cpu_env, op1_offset);
            tcg_gen_addi_ptr(s->ptr1, cpu_env, op2_offset);
            sse_fn_eppi(cpu_env, s->ptr0, s->ptr1, tcg_const_i32(val));
            break;

        case 0x33a:
            /* Various integer extensions at 0f 3a f[0-f].  */
            b = modrm | (b1 << 8);
            modrm = x86_ldub_code(env, s);
            reg = ((modrm >> 3) & 7) | REX_R(s);

            switch (b) {
            case 0x3f0: /* rorx Gy,Ey, Ib */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_BMI2)
                    || !(s->prefix & PREFIX_VEX)
                    || s->vex_l != 0) {
                    goto illegal_op;
                }
                ot = mo_64_32(s->dflag);
                gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
                b = x86_ldub_code(env, s);
                if (ot == MO_64) {
                    tcg_gen_rotri_tl(s->T0, s->T0, b & 63);
                } else {
                    tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
                    tcg_gen_rotri_i32(s->tmp2_i32, s->tmp2_i32, b & 31);
                    tcg_gen_extu_i32_tl(s->T0, s->tmp2_i32);
                }
                gen_op_mov_reg_v(s, ot, reg, s->T0);
                break;

            default:
                goto unknown_op;
            }
            break;

        default:
        unknown_op:
            gen_unknown_opcode(env, s);
            return;
        }
    } else {
        /* generic MMX or SSE operation */
        switch(b) {
        case 0x70: /* pshufx insn */
        case 0xc6: /* pshufx insn */
        case 0xc2: /* compare insns */
            s->rip_offset = 1;
            break;
        default:
            break;
        }
        if (is_xmm) {
            op1_offset = offsetof(CPUX86State,xmm_regs[reg]);
            if (mod != 3) {
                int sz = 4;

                gen_lea_modrm(env, s, modrm);
                op2_offset = offsetof(CPUX86State,xmm_t0);

                switch (b) {
                case 0x50 ... 0x5a:
                case 0x5c ... 0x5f:
                case 0xc2:
                    /* Most sse scalar operations.  */
                    if (b1 == 2) {
                        sz = 2;
                    } else if (b1 == 3) {
                        sz = 3;
                    }
                    break;

                case 0x2e:  /* ucomis[sd] */
                case 0x2f:  /* comis[sd] */
                    if (b1 == 0) {
                        sz = 2;
                    } else {
                        sz = 3;
                    }
                    break;
                }

                switch (sz) {
                case 2:
                    /* 32 bit access */
                    gen_op_ld_v(s, MO_32, s->T0, s->A0);
                    tcg_gen_st32_tl(s->T0, cpu_env,
                                    offsetof(CPUX86State,xmm_t0.ZMM_L(0)));
                    break;
                case 3:
                    /* 64 bit access */
                    gen_ldq_env_A0(s, offsetof(CPUX86State, xmm_t0.ZMM_D(0)));
                    break;
                default:
                    /* 128 bit access */
                    gen_ldo_env_A0(s, op2_offset);
                    break;
                }
            } else {
                rm = (modrm & 7) | REX_B(s);
                op2_offset = offsetof(CPUX86State,xmm_regs[rm]);
            }
        } else {
            op1_offset = offsetof(CPUX86State,fpregs[reg].mmx);
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                op2_offset = offsetof(CPUX86State,mmx_t0);
                gen_ldq_env_A0(s, op2_offset);
            } else {
                rm = (modrm & 7);
                op2_offset = offsetof(CPUX86State,fpregs[rm].mmx);
            }
        }
        switch(b) {
        case 0x0f: /* 3DNow! data insns */
            val = x86_ldub_code(env, s);
            sse_fn_epp = sse_op_table5[val];
            if (!sse_fn_epp) {
                goto unknown_op;
            }
            if (!(s->cpuid_ext2_features & CPUID_EXT2_3DNOW)) {
                goto illegal_op;
            }
            tcg_gen_addi_ptr(s->ptr0, cpu_env, op1_offset);
            tcg_gen_addi_ptr(s->ptr1, cpu_env, op2_offset);
            sse_fn_epp(cpu_env, s->ptr0, s->ptr1);
            break;
        case 0x70: /* pshufx insn */
        case 0xc6: /* pshufx insn */
            val = x86_ldub_code(env, s);
            tcg_gen_addi_ptr(s->ptr0, cpu_env, op1_offset);
            tcg_gen_addi_ptr(s->ptr1, cpu_env, op2_offset);
            /* XXX: introduce a new table? */
            sse_fn_ppi = (SSEFunc_0_ppi)sse_fn_epp;
            sse_fn_ppi(s->ptr0, s->ptr1, tcg_const_i32(val));
            break;
        case 0xc2:
            /* compare insns */
            val = x86_ldub_code(env, s);
            if (val >= 8)
                goto unknown_op;
            sse_fn_epp = sse_op_table4[val][b1];

            tcg_gen_addi_ptr(s->ptr0, cpu_env, op1_offset);
            tcg_gen_addi_ptr(s->ptr1, cpu_env, op2_offset);
            sse_fn_epp(cpu_env, s->ptr0, s->ptr1);
            break;
        case 0xf7:
            /* maskmov : we must prepare A0 */
            if (mod != 3)
                goto illegal_op;
            tcg_gen_mov_tl(s->A0, cpu_regs[R_EDI]);
            gen_extu(s->aflag, s->A0);
            gen_add_A0_ds_seg(s);

            tcg_gen_addi_ptr(s->ptr0, cpu_env, op1_offset);
            tcg_gen_addi_ptr(s->ptr1, cpu_env, op2_offset);
            /* XXX: introduce a new table? */
            sse_fn_eppt = (SSEFunc_0_eppt)sse_fn_epp;
            sse_fn_eppt(cpu_env, s->ptr0, s->ptr1, s->A0);
            break;
        default:
            tcg_gen_addi_ptr(s->ptr0, cpu_env, op1_offset);
            tcg_gen_addi_ptr(s->ptr1, cpu_env, op2_offset);
            sse_fn_epp(cpu_env, s->ptr0, s->ptr1);
            break;
        }
        if (b == 0x2e || b == 0x2f) {
            set_cc_op(s, CC_OP_EFLAGS);
        }
    }
}

#define gen_gvec_mov(dofs, aofs, oprsz, maxsz, vece)    \
    tcg_gen_gvec_mov(vece, dofs, aofs, oprsz, maxsz)

#define gen_gvec_add(dofs, aofs, bofs, oprsz, maxsz, vece)      \
    tcg_gen_gvec_add(vece, dofs, aofs, bofs, oprsz, maxsz)
#define gen_gvec_sub(dofs, aofs, bofs, oprsz, maxsz, vece)      \
    tcg_gen_gvec_sub(vece, dofs, aofs, bofs, oprsz, maxsz)

#define gen_gvec_ssadd(dofs, aofs, bofs, oprsz, maxsz, vece)    \
    tcg_gen_gvec_ssadd(vece, dofs, aofs, bofs, oprsz, maxsz)
#define gen_gvec_sssub(dofs, aofs, bofs, oprsz, maxsz, vece)    \
    tcg_gen_gvec_sssub(vece, dofs, aofs, bofs, oprsz, maxsz)
#define gen_gvec_usadd(dofs, aofs, bofs, oprsz, maxsz, vece)    \
    tcg_gen_gvec_usadd(vece, dofs, aofs, bofs, oprsz, maxsz)
#define gen_gvec_ussub(dofs, aofs, bofs, oprsz, maxsz, vece)    \
    tcg_gen_gvec_ussub(vece, dofs, aofs, bofs, oprsz, maxsz)

#define gen_gvec_smin(dofs, aofs, bofs, oprsz, maxsz, vece)     \
    tcg_gen_gvec_smin(vece, dofs, aofs, bofs, oprsz, maxsz)
#define gen_gvec_umin(dofs, aofs, bofs, oprsz, maxsz, vece)     \
    tcg_gen_gvec_umin(vece, dofs, aofs, bofs, oprsz, maxsz)
#define gen_gvec_smax(dofs, aofs, bofs, oprsz, maxsz, vece)     \
    tcg_gen_gvec_smax(vece, dofs, aofs, bofs, oprsz, maxsz)
#define gen_gvec_umax(dofs, aofs, bofs, oprsz, maxsz, vece)     \
    tcg_gen_gvec_umax(vece, dofs, aofs, bofs, oprsz, maxsz)

#define gen_gvec_and(dofs, aofs, bofs, oprsz, maxsz, vece)      \
    tcg_gen_gvec_and(vece, dofs, aofs, bofs, oprsz, maxsz)
#define gen_gvec_andn(dofs, aofs, bofs, oprsz, maxsz, vece)     \
    tcg_gen_gvec_andc(vece, dofs, bofs, aofs, oprsz, maxsz)
#define gen_gvec_or(dofs, aofs, bofs, oprsz, maxsz, vece)       \
    tcg_gen_gvec_or(vece, dofs, aofs, bofs, oprsz, maxsz)
#define gen_gvec_xor(dofs, aofs, bofs, oprsz, maxsz, vece)      \
    tcg_gen_gvec_xor(vece, dofs, aofs, bofs, oprsz, maxsz)

#define gen_gvec_cmp(dofs, aofs, bofs, oprsz, maxsz, vece, cond)        \
    tcg_gen_gvec_cmp(cond, vece, dofs, aofs, bofs, oprsz, maxsz)

typedef enum {
    CHECK_CPUID_MMX = 1,
    CHECK_CPUID_3DNOW,
    CHECK_CPUID_SSE,
    CHECK_CPUID_SSE2,
    CHECK_CPUID_CLFLUSH,
    CHECK_CPUID_SSE3,
    CHECK_CPUID_SSSE3,
    CHECK_CPUID_SSE4_1,
    CHECK_CPUID_SSE4_2,
    CHECK_CPUID_SSE4A,
    CHECK_CPUID_AES,
    CHECK_CPUID_PCLMULQDQ,
    CHECK_CPUID_AVX,
    CHECK_CPUID_AES_AVX,
    CHECK_CPUID_PCLMULQDQ_AVX,
    CHECK_CPUID_AVX2,
} CheckCpuidFeat;

static bool check_cpuid(CPUX86State *env, DisasContext *s, CheckCpuidFeat feat)
{
    switch (feat) {
    case CHECK_CPUID_MMX:
        return (s->cpuid_features & CPUID_MMX)
            && (s->cpuid_ext2_features & CPUID_EXT2_MMX);
    case CHECK_CPUID_3DNOW:
        return s->cpuid_ext2_features & CPUID_EXT2_3DNOW;
    case CHECK_CPUID_SSE:
        return s->cpuid_features & CPUID_SSE;
    case CHECK_CPUID_SSE2:
        return s->cpuid_features & CPUID_SSE2;
    case CHECK_CPUID_CLFLUSH:
        return s->cpuid_features & CPUID_CLFLUSH;
    case CHECK_CPUID_SSE3:
        return s->cpuid_ext_features & CPUID_EXT_SSE3;
    case CHECK_CPUID_SSSE3:
        return s->cpuid_ext_features & CPUID_EXT_SSSE3;
    case CHECK_CPUID_SSE4_1:
        return s->cpuid_ext_features & CPUID_EXT_SSE41;
    case CHECK_CPUID_SSE4_2:
        return s->cpuid_ext_features & CPUID_EXT_SSE42;
    case CHECK_CPUID_SSE4A:
        return s->cpuid_ext3_features & CPUID_EXT3_SSE4A;
    case CHECK_CPUID_AES:
        return s->cpuid_ext_features & CPUID_EXT_AES;
    case CHECK_CPUID_PCLMULQDQ:
        return s->cpuid_ext_features & CPUID_EXT_PCLMULQDQ;
    case CHECK_CPUID_AVX:
        return true /* s->cpuid_ext_features & CPUID_EXT_AVX */;
    case CHECK_CPUID_AES_AVX:
        return s->cpuid_ext_features & CPUID_EXT_AES
            /* && (s->cpuid_ext_features & CPUID_EXT_AVX) */;
    case CHECK_CPUID_PCLMULQDQ_AVX:
        return s->cpuid_ext_features & CPUID_EXT_PCLMULQDQ
            /* && (s->cpuid_ext_features & CPUID_EXT_AVX) */;
    case CHECK_CPUID_AVX2:
        return true /* s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_AVX2 */;
    default:
        g_assert_not_reached();
    }
}

/*
 * Instruction operand
 */
#define insnop_arg_t(opT)    insnop_ ## opT ## _arg_t
#define insnop_ctxt_t(opT)   insnop_ ## opT ## _ctxt_t
#define insnop_init(opT)     insnop_ ## opT ## _init
#define insnop_prepare(opT)  insnop_ ## opT ## _prepare
#define insnop_finalize(opT) insnop_ ## opT ## _finalize

#define INSNOP_INIT(opT)                                        \
    static bool insnop_init(opT)(insnop_ctxt_t(opT) *ctxt,      \
                                 CPUX86State *env,              \
                                 DisasContext *s,               \
                                 int modrm, bool is_write)

#define INSNOP_PREPARE(opT)                                             \
    static insnop_arg_t(opT) insnop_prepare(opT)(insnop_ctxt_t(opT) *ctxt, \
                                                 CPUX86State *env,      \
                                                 DisasContext *s,       \
                                                 int modrm, bool is_write)

#define INSNOP_FINALIZE(opT)                                    \
    static void insnop_finalize(opT)(insnop_ctxt_t(opT) *ctxt,  \
                                     CPUX86State *env,          \
                                     DisasContext *s,           \
                                     int modrm, bool is_write,  \
                                     insnop_arg_t(opT) arg)

/*
 * Operand alias
 */
#define DEF_INSNOP_ALIAS(opT, opT2)                                     \
    typedef insnop_arg_t(opT2) insnop_arg_t(opT);                       \
    typedef insnop_ctxt_t(opT2) insnop_ctxt_t(opT);                     \
                                                                        \
    INSNOP_INIT(opT)                                                    \
    {                                                                   \
        return insnop_init(opT2)(ctxt, env, s, modrm, is_write);        \
    }                                                                   \
    INSNOP_PREPARE(opT)                                                 \
    {                                                                   \
        return insnop_prepare(opT2)(ctxt, env, s, modrm, is_write);     \
    }                                                                   \
    INSNOP_FINALIZE(opT)                                                \
    {                                                                   \
        insnop_finalize(opT2)(ctxt, env, s, modrm, is_write, arg);      \
    }

/*
 * Generic unifying either-or operand
 */
#define DEF_INSNOP_EITHER(opT, opT1, opT2)                              \
    typedef insnop_arg_t(opT1) insnop_arg_t(opT);                       \
    typedef struct {                                                    \
        bool is_ ## opT1;                                               \
        union {                                                         \
            insnop_ctxt_t(opT1) ctxt_ ## opT1;                          \
            insnop_ctxt_t(opT2) ctxt_ ## opT2;                          \
        };                                                              \
    } insnop_ctxt_t(opT);                                               \
                                                                        \
    INSNOP_INIT(opT)                                                    \
    {                                                                   \
        if (insnop_init(opT1)(&ctxt->ctxt_ ## opT1,                     \
                              env, s, modrm, is_write)) {               \
            ctxt->is_ ## opT1 = true;                                   \
            return true;                                                \
        }                                                               \
        if (insnop_init(opT2)(&ctxt->ctxt_ ## opT2,                     \
                              env, s, modrm, is_write)) {               \
            ctxt->is_ ## opT1 = false;                                  \
            return true;                                                \
        }                                                               \
        return false;                                                   \
    }                                                                   \
    INSNOP_PREPARE(opT)                                                 \
    {                                                                   \
        return (ctxt->is_ ## opT1                                       \
                ? insnop_prepare(opT1)(&ctxt->ctxt_ ## opT1,            \
                                       env, s, modrm, is_write)         \
                : insnop_prepare(opT2)(&ctxt->ctxt_ ## opT2,            \
                                       env, s, modrm, is_write));       \
    }                                                                   \
    INSNOP_FINALIZE(opT)                                                \
    {                                                                   \
        (ctxt->is_ ## opT1                                              \
         ? insnop_finalize(opT1)(&ctxt->ctxt_ ## opT1,                  \
                                 env, s, modrm, is_write, arg)          \
         : insnop_finalize(opT2)(&ctxt->ctxt_ ## opT2,                  \
                                 env, s, modrm, is_write, arg));        \
    }

/*
 * Generic load-store operand
 */
#define insnop_ldst(opTarg, opTptr)             \
    insnop_ldst_ ## opTarg ## opTptr

#define INSNOP_LDST(opTarg, opTptr)                                     \
    static void insnop_ldst(opTarg, opTptr)(CPUX86State *env,           \
                                            DisasContext *s,            \
                                            bool is_write,              \
                                            insnop_arg_t(opTarg) arg,   \
                                            insnop_arg_t(opTptr) ptr)

#define DEF_INSNOP_LDST(opT, opTarg, opTptr)                            \
    typedef insnop_arg_t(opTarg) insnop_arg_t(opT);                     \
    typedef struct {                                                    \
        insnop_ctxt_t(opTarg) arg_ctxt;                                 \
        insnop_ctxt_t(opTptr) ptr_ctxt;                                 \
        insnop_arg_t(opTptr) ptr;                                       \
    } insnop_ctxt_t(opT);                                               \
                                                                        \
    /* forward declaration */                                           \
    INSNOP_LDST(opTarg, opTptr);                                        \
                                                                        \
    INSNOP_INIT(opT)                                                    \
    {                                                                   \
        return insnop_init(opTarg)(&ctxt->arg_ctxt,                     \
                                   env, s, modrm, is_write)             \
            && insnop_init(opTptr)(&ctxt->ptr_ctxt,                     \
                                   env, s, modrm, is_write);            \
    }                                                                   \
    INSNOP_PREPARE(opT)                                                 \
    {                                                                   \
        const insnop_arg_t(opTarg) arg =                                \
            insnop_prepare(opTarg)(&ctxt->arg_ctxt,                     \
                                   env, s, modrm, is_write);            \
        ctxt->ptr =                                                     \
            insnop_prepare(opTptr)(&ctxt->ptr_ctxt,                     \
                                   env, s, modrm, is_write);            \
        if (!is_write) {                                                \
            insnop_ldst(opTarg, opTptr)(env, s, is_write,               \
                                        arg, ctxt->ptr);                \
        }                                                               \
        return arg;                                                     \
    }                                                                   \
    INSNOP_FINALIZE(opT)                                                \
    {                                                                   \
        if (is_write) {                                                 \
            insnop_ldst(opTarg, opTptr)(env, s, is_write,               \
                                        arg, ctxt->ptr);                \
        }                                                               \
        insnop_finalize(opTptr)(&ctxt->ptr_ctxt,                        \
                                env, s, modrm, is_write, ctxt->ptr);    \
        insnop_finalize(opTarg)(&ctxt->arg_ctxt,                        \
                                env, s, modrm, is_write, arg);          \
    }

/*
 * tcg_i32
 *
 * Operand which allocates a 32-bit TCG temporary and frees it
 * automatically after use.
 */
typedef TCGv_i32 insnop_arg_t(tcg_i32);
typedef struct {} insnop_ctxt_t(tcg_i32);

INSNOP_INIT(tcg_i32)
{
    return true;
}
INSNOP_PREPARE(tcg_i32)
{
    return tcg_temp_new_i32();
}
INSNOP_FINALIZE(tcg_i32)
{
    tcg_temp_free_i32(arg);
}

/*
 * tcg_i64
 *
 * Operand which allocates a 64-bit TCG temporary and frees it
 * automatically after use.
 */
typedef TCGv_i64 insnop_arg_t(tcg_i64);
typedef struct {} insnop_ctxt_t(tcg_i64);

INSNOP_INIT(tcg_i64)
{
    return true;
}
INSNOP_PREPARE(tcg_i64)
{
    return tcg_temp_new_i64();
}
INSNOP_FINALIZE(tcg_i64)
{
    tcg_temp_free_i64(arg);
}

/*
 * modrm
 *
 * Operand whose value is the ModR/M byte.
 */
typedef int insnop_arg_t(modrm);
typedef struct {} insnop_ctxt_t(modrm);

INSNOP_INIT(modrm)
{
    return true;
}
INSNOP_PREPARE(modrm)
{
    return modrm;
}
INSNOP_FINALIZE(modrm)
{
}

/*
 * modrm_mod
 *
 * Operand whose value is the MOD field of the ModR/M byte.
 */
typedef int insnop_arg_t(modrm_mod);
typedef struct {} insnop_ctxt_t(modrm_mod);

INSNOP_INIT(modrm_mod)
{
    return true;
}
INSNOP_PREPARE(modrm_mod)
{
    return (modrm >> 6) & 3;
}
INSNOP_FINALIZE(modrm_mod)
{
}

/*
 * modrm_reg
 *
 * Operand whose value is the REG field of the ModR/M byte, extended
 * with the REX.R bit if REX prefix is present.
 */
typedef int insnop_arg_t(modrm_reg);
typedef struct {} insnop_ctxt_t(modrm_reg);

INSNOP_INIT(modrm_reg)
{
    return true;
}
INSNOP_PREPARE(modrm_reg)
{
    return ((modrm >> 3) & 7) | REX_R(s);
}
INSNOP_FINALIZE(modrm_reg)
{
}

/*
 * modrm_rm
 *
 * Operand whose value is the RM field of the ModR/M byte, extended
 * with the REX.B bit if REX prefix is present.
 */
typedef int insnop_arg_t(modrm_rm);
typedef struct {} insnop_ctxt_t(modrm_rm);

INSNOP_INIT(modrm_rm)
{
    return true;
}
INSNOP_PREPARE(modrm_rm)
{
    return (modrm & 7) | REX_B(s);
}
INSNOP_FINALIZE(modrm_rm)
{
}

/*
 * modrm_rm_direct
 *
 * Equivalent of modrm_rm, but only decodes successfully if
 * the ModR/M byte has the direct form (i.e. MOD=3).
 */
typedef insnop_arg_t(modrm_rm) insnop_arg_t(modrm_rm_direct);
typedef struct {
    insnop_ctxt_t(modrm_rm) rm;
} insnop_ctxt_t(modrm_rm_direct);

INSNOP_INIT(modrm_rm_direct)
{
    bool ret;
    insnop_ctxt_t(modrm_mod) modctxt;

    ret = insnop_init(modrm_mod)(&modctxt, env, s, modrm, 0);
    if (ret) {
        const int mod = insnop_prepare(modrm_mod)(&modctxt, env, s, modrm, 0);
        if (mod == 3) {
            ret = insnop_init(modrm_rm)(&ctxt->rm, env, s, modrm, is_write);
        } else {
            ret = false;
        }
        insnop_finalize(modrm_mod)(&modctxt, env, s, modrm, 0, mod);
    }
    return ret;
}
INSNOP_PREPARE(modrm_rm_direct)
{
    return insnop_prepare(modrm_rm)(&ctxt->rm, env, s, modrm, is_write);
}
INSNOP_FINALIZE(modrm_rm_direct)
{
    insnop_finalize(modrm_rm)(&ctxt->rm, env, s, modrm, is_write, arg);
}

/*
 * Immediate operand
 */
typedef uint8_t insnop_arg_t(Ib);
typedef struct {} insnop_ctxt_t(Ib);

INSNOP_INIT(Ib)
{
    return true;
}
INSNOP_PREPARE(Ib)
{
    return x86_ldub_code(env, s);
}
INSNOP_FINALIZE(Ib)
{
}

/*
 * Memory-pointer operand
 */
typedef TCGv insnop_arg_t(M);
typedef struct {} insnop_ctxt_t(M);

INSNOP_INIT(M)
{
    bool ret;
    insnop_ctxt_t(modrm_mod) modctxt;

    ret = insnop_init(modrm_mod)(&modctxt, env, s, modrm, is_write);
    if (ret) {
        const int mod =
            insnop_prepare(modrm_mod)(&modctxt, env, s, modrm, is_write);
        ret = (mod != 3);
        insnop_finalize(modrm_mod)(&modctxt, env, s, modrm, is_write, mod);
    }
    return ret;
}
INSNOP_PREPARE(M)
{
    gen_lea_modrm(env, s, modrm);
    return s->A0;
}
INSNOP_FINALIZE(M)
{
}

DEF_INSNOP_ALIAS(Mb, M)
DEF_INSNOP_ALIAS(Mw, M)
DEF_INSNOP_ALIAS(Md, M)
DEF_INSNOP_ALIAS(Mq, M)
DEF_INSNOP_ALIAS(Mhq, M)
DEF_INSNOP_ALIAS(Mdq, M)
DEF_INSNOP_ALIAS(Mqq, M)

DEF_INSNOP_ALIAS(MDdq, M)
DEF_INSNOP_ALIAS(MDqq, M)
DEF_INSNOP_ALIAS(MQdq, M)
DEF_INSNOP_ALIAS(MQqq, M)

/*
 * 32-bit general register operands
 */
DEF_INSNOP_LDST(Gd, tcg_i32, modrm_reg)
DEF_INSNOP_LDST(Rd, tcg_i32, modrm_rm_direct)

INSNOP_LDST(tcg_i32, modrm_reg)
{
    assert(0 <= ptr && ptr < CPU_NB_REGS);
    if (is_write) {
        tcg_gen_extu_i32_tl(cpu_regs[ptr], arg);
    } else {
        tcg_gen_trunc_tl_i32(arg, cpu_regs[ptr]);
    }
}
INSNOP_LDST(tcg_i32, modrm_rm_direct)
{
    insnop_ldst(tcg_i32, modrm_reg)(env, s, is_write, arg, ptr);
}

DEF_INSNOP_LDST(MEd, tcg_i32, Md)
DEF_INSNOP_EITHER(Ed, Rd, MEd)
DEF_INSNOP_LDST(MRdMw, tcg_i32, Mw)
DEF_INSNOP_EITHER(RdMw, Rd, MRdMw)
DEF_INSNOP_LDST(MRdMb, tcg_i32, Mb)
DEF_INSNOP_EITHER(RdMb, Rd, MRdMb)

INSNOP_LDST(tcg_i32, Md)
{
    if (is_write) {
        tcg_gen_qemu_st_i32(arg, ptr, s->mem_index, MO_LEUL);
    } else {
        tcg_gen_qemu_ld_i32(arg, ptr, s->mem_index, MO_LEUL);
    }
}
INSNOP_LDST(tcg_i32, Mw)
{
    if (is_write) {
        tcg_gen_qemu_st_i32(arg, ptr, s->mem_index, MO_LEUW);
    } else {
        tcg_gen_qemu_ld_i32(arg, ptr, s->mem_index, MO_LEUW);
    }
}
INSNOP_LDST(tcg_i32, Mb)
{
    if (is_write) {
        tcg_gen_qemu_st_i32(arg, ptr, s->mem_index, MO_UB);
    } else {
        tcg_gen_qemu_ld_i32(arg, ptr, s->mem_index, MO_UB);
    }
}

/*
 * 64-bit general register operands
 */
DEF_INSNOP_LDST(Gq, tcg_i64, modrm_reg)
DEF_INSNOP_LDST(Rq, tcg_i64, modrm_rm_direct)

INSNOP_LDST(tcg_i64, modrm_reg)
{
#ifdef TARGET_X86_64
    assert(0 <= ptr && ptr < CPU_NB_REGS);
    if (is_write) {
        tcg_gen_mov_i64(cpu_regs[ptr], arg);
    } else {
        tcg_gen_mov_i64(arg, cpu_regs[ptr]);
    }
#else /* !TARGET_X86_64 */
    g_assert_not_reached();
#endif /* !TARGET_X86_64 */
}
INSNOP_LDST(tcg_i64, modrm_rm_direct)
{
    insnop_ldst(tcg_i64, modrm_reg)(env, s, is_write, arg, ptr);
}

DEF_INSNOP_LDST(MEq, tcg_i64, Mq)
DEF_INSNOP_EITHER(Eq, Rq, MEq)

INSNOP_LDST(tcg_i64, Mq)
{
    if (is_write) {
        tcg_gen_qemu_st_i64(arg, ptr, s->mem_index, MO_LEQ);
    } else {
        tcg_gen_qemu_ld_i64(arg, ptr, s->mem_index, MO_LEQ);
    }
}

/*
 * MMX-technology register operands
 */
typedef unsigned int insnop_arg_t(mm);
typedef struct {} insnop_ctxt_t(mm);

INSNOP_INIT(mm)
{
    return true;
}
INSNOP_PREPARE(mm)
{
    return offsetof(CPUX86State, mmx_t0);
}
INSNOP_FINALIZE(mm)
{
}

#define DEF_INSNOP_MM(opT, opTmmid)                                     \
    typedef insnop_arg_t(mm) insnop_arg_t(opT);                         \
    typedef struct {                                                    \
        insnop_ctxt_t(opTmmid) mmid;                                    \
    } insnop_ctxt_t(opT);                                               \
                                                                        \
    INSNOP_INIT(opT)                                                    \
    {                                                                   \
        return insnop_init(opTmmid)(&ctxt->mmid, env, s, modrm, is_write); \
    }                                                                   \
    INSNOP_PREPARE(opT)                                                 \
    {                                                                   \
        const insnop_arg_t(opTmmid) mmid =                              \
            insnop_prepare(opTmmid)(&ctxt->mmid, env, s, modrm, is_write); \
        const insnop_arg_t(opT) arg =                                   \
            offsetof(CPUX86State, fpregs[mmid & 7].mmx);                \
        insnop_finalize(opTmmid)(&ctxt->mmid, env, s, modrm, is_write, mmid); \
        return arg;                                                     \
    }                                                                   \
    INSNOP_FINALIZE(opT)                                                \
    {                                                                   \
    }

DEF_INSNOP_MM(P, modrm_reg)
DEF_INSNOP_ALIAS(Pq, P)

DEF_INSNOP_MM(N, modrm_rm_direct)
DEF_INSNOP_ALIAS(Nd, N)
DEF_INSNOP_ALIAS(Nq, N)

DEF_INSNOP_LDST(MQd, mm, Md)
DEF_INSNOP_LDST(MQq, mm, Mq)
DEF_INSNOP_EITHER(Qd, Nd, MQd)
DEF_INSNOP_EITHER(Qq, Nq, MQq)

INSNOP_LDST(mm, Md)
{
    const insnop_arg_t(mm) ofs = offsetof(MMXReg, MMX_L(0));
    const TCGv_i32 r32 = tcg_temp_new_i32();
    if (is_write) {
        tcg_gen_ld_i32(r32, cpu_env, arg + ofs);
        tcg_gen_qemu_st_i32(r32, ptr, s->mem_index, MO_LEUL);
    } else {
        tcg_gen_qemu_ld_i32(r32, ptr, s->mem_index, MO_LEUL);
        tcg_gen_st_i32(r32, cpu_env, arg + ofs);
    }
    tcg_temp_free_i32(r32);
}
INSNOP_LDST(mm, Mq)
{
    const insnop_arg_t(mm) ofs = offsetof(MMXReg, MMX_Q(0));
    const TCGv_i64 r64 = tcg_temp_new_i64();
    if (is_write) {
        tcg_gen_ld_i64(r64, cpu_env, arg + ofs);
        tcg_gen_qemu_st_i64(r64, ptr, s->mem_index, MO_LEQ);
    } else {
        tcg_gen_qemu_ld_i64(r64, ptr, s->mem_index, MO_LEQ);
        tcg_gen_st_i64(r64, cpu_env, arg + ofs);
    }
    tcg_temp_free_i64(r64);
}

/*
 * vex_v
 *
 * Operand whose value is the VVVV field of the VEX prefix.
 */
typedef int insnop_arg_t(vex_v);
typedef struct {} insnop_ctxt_t(vex_v);

INSNOP_INIT(vex_v)
{
    return s->prefix & PREFIX_VEX;
}
INSNOP_PREPARE(vex_v)
{
    return s->vex_v;
}
INSNOP_FINALIZE(vex_v)
{
}

/*
 * is4
 *
 * Upper 4 bits of the 8-bit immediate selector, used with the SSE/AVX
 * register file in some instructions.
 */
typedef int insnop_arg_t(is4);
typedef struct {
    insnop_ctxt_t(Ib) ctxt_Ib;
    insnop_arg_t(Ib) arg_Ib;
} insnop_ctxt_t(is4);

INSNOP_INIT(is4)
{
    return insnop_init(Ib)(&ctxt->ctxt_Ib, env, s, modrm, is_write);
}
INSNOP_PREPARE(is4)
{
    ctxt->arg_Ib = insnop_prepare(Ib)(&ctxt->ctxt_Ib, env, s, modrm, is_write);
    return (ctxt->arg_Ib >> 4) & 0xf;
}
INSNOP_FINALIZE(is4)
{
    insnop_finalize(Ib)(&ctxt->ctxt_Ib, env, s, modrm, is_write, ctxt->arg_Ib);
}

/*
 * SSE/AVX-technology registers
 */
typedef unsigned int insnop_arg_t(xmm);
typedef struct {} insnop_ctxt_t(xmm);

INSNOP_INIT(xmm)
{
    return true;
}
INSNOP_PREPARE(xmm)
{
    return offsetof(CPUX86State, xmm_t0);
}
INSNOP_FINALIZE(xmm)
{
}

#define DEF_INSNOP_XMM(opT, opTxmmid)                                   \
    typedef insnop_arg_t(xmm) insnop_arg_t(opT);                        \
    typedef struct {                                                    \
        insnop_ctxt_t(opTxmmid) xmmid;                                  \
    } insnop_ctxt_t(opT);                                               \
                                                                        \
    INSNOP_INIT(opT)                                                    \
    {                                                                   \
        return insnop_init(opTxmmid)(&ctxt->xmmid, env, s, modrm, is_write); \
    }                                                                   \
    INSNOP_PREPARE(opT)                                                 \
    {                                                                   \
        const insnop_arg_t(opTxmmid) xmmid =                            \
            insnop_prepare(opTxmmid)(&ctxt->xmmid, env, s, modrm, is_write); \
        const insnop_arg_t(opT) arg =                                   \
            offsetof(CPUX86State, xmm_regs[xmmid]);                     \
        insnop_finalize(opTxmmid)(&ctxt->xmmid, env, s,                 \
                                  modrm, is_write, xmmid);              \
        return arg;                                                     \
    }                                                                   \
    INSNOP_FINALIZE(opT)                                                \
    {                                                                   \
    }

DEF_INSNOP_XMM(V, modrm_reg)
DEF_INSNOP_ALIAS(Vd, V)
DEF_INSNOP_ALIAS(Vq, V)
DEF_INSNOP_ALIAS(Vdq, V)
DEF_INSNOP_ALIAS(Vqq, V)

DEF_INSNOP_XMM(U, modrm_rm_direct)
DEF_INSNOP_ALIAS(Ub, U)
DEF_INSNOP_ALIAS(Uw, U)
DEF_INSNOP_ALIAS(Ud, U)
DEF_INSNOP_ALIAS(Uq, U)
DEF_INSNOP_ALIAS(Udq, U)
DEF_INSNOP_ALIAS(Uqq, U)

DEF_INSNOP_ALIAS(Hd, Vd)
DEF_INSNOP_ALIAS(Hq, Vq)
DEF_INSNOP_ALIAS(Hdq, Vdq)
DEF_INSNOP_ALIAS(Hqq, Vqq)

DEF_INSNOP_XMM(L, is4)
DEF_INSNOP_ALIAS(Ldq, L)
DEF_INSNOP_ALIAS(Lqq, L)

DEF_INSNOP_LDST(MWb, xmm, Mb)
DEF_INSNOP_LDST(MWw, xmm, Mw)
DEF_INSNOP_LDST(MWd, xmm, Md)
DEF_INSNOP_LDST(MWq, xmm, Mq)
DEF_INSNOP_LDST(MWdq, xmm, Mdq)
DEF_INSNOP_LDST(MWqq, xmm, Mqq)
DEF_INSNOP_LDST(MUdqMhq, xmm, Mhq)
DEF_INSNOP_EITHER(Wb, Ub, MWb)
DEF_INSNOP_EITHER(Ww, Uw, MWw)
DEF_INSNOP_EITHER(Wd, Ud, MWd)
DEF_INSNOP_EITHER(Wq, Uq, MWq)
DEF_INSNOP_EITHER(Wdq, Udq, MWdq)
DEF_INSNOP_EITHER(Wqq, Uqq, MWqq)
DEF_INSNOP_EITHER(UdqMq, Udq, MWq)
DEF_INSNOP_EITHER(UdqMhq, Udq, MUdqMhq)

INSNOP_LDST(xmm, Mb)
{
    const insnop_arg_t(xmm) ofs = offsetof(ZMMReg, ZMM_B(0));
    const TCGv_i32 r32 = tcg_temp_new_i32();
    if (is_write) {
        tcg_gen_ld_i32(r32, cpu_env, arg + ofs);
        tcg_gen_qemu_st_i32(r32, ptr, s->mem_index, MO_UB);
    } else {
        tcg_gen_qemu_ld_i32(r32, ptr, s->mem_index, MO_UB);
        tcg_gen_st_i32(r32, cpu_env, arg + ofs);
    }
    tcg_temp_free_i32(r32);
}
INSNOP_LDST(xmm, Mw)
{
    const insnop_arg_t(xmm) ofs = offsetof(ZMMReg, ZMM_W(0));
    const TCGv_i32 r32 = tcg_temp_new_i32();
    if (is_write) {
        tcg_gen_ld_i32(r32, cpu_env, arg + ofs);
        tcg_gen_qemu_st_i32(r32, ptr, s->mem_index, MO_LEUW);
    } else {
        tcg_gen_qemu_ld_i32(r32, ptr, s->mem_index, MO_LEUW);
        tcg_gen_st_i32(r32, cpu_env, arg + ofs);
    }
    tcg_temp_free_i32(r32);
}
INSNOP_LDST(xmm, Md)
{
    const insnop_arg_t(xmm) ofs = offsetof(ZMMReg, ZMM_L(0));
    const TCGv_i32 r32 = tcg_temp_new_i32();
    if (is_write) {
        tcg_gen_ld_i32(r32, cpu_env, arg + ofs);
        tcg_gen_qemu_st_i32(r32, ptr, s->mem_index, MO_LEUL);
    } else {
        tcg_gen_qemu_ld_i32(r32, ptr, s->mem_index, MO_LEUL);
        tcg_gen_st_i32(r32, cpu_env, arg + ofs);
    }
    tcg_temp_free_i32(r32);
}
INSNOP_LDST(xmm, Mq)
{
    const insnop_arg_t(xmm) ofs = offsetof(ZMMReg, ZMM_Q(0));
    const TCGv_i64 r64 = tcg_temp_new_i64();
    if (is_write) {
        tcg_gen_ld_i64(r64, cpu_env, arg + ofs);
        tcg_gen_qemu_st_i64(r64, ptr, s->mem_index, MO_LEQ);
    } else {
        tcg_gen_qemu_ld_i64(r64, ptr, s->mem_index, MO_LEQ);
        tcg_gen_st_i64(r64, cpu_env, arg + ofs);
    }
    tcg_temp_free_i64(r64);
}
INSNOP_LDST(xmm, Mdq)
{
    const insnop_arg_t(xmm) ofs = offsetof(ZMMReg, ZMM_Q(0));
    const insnop_arg_t(xmm) ofs1 = offsetof(ZMMReg, ZMM_Q(1));
    const TCGv_i64 r64 = tcg_temp_new_i64();
    const TCGv ptr1 = tcg_temp_new();
    tcg_gen_addi_tl(ptr1, ptr, sizeof(uint64_t));
    if (is_write) {
        tcg_gen_ld_i64(r64, cpu_env, arg + ofs);
        tcg_gen_qemu_st_i64(r64, ptr, s->mem_index, MO_LEQ);
        tcg_gen_ld_i64(r64, cpu_env, arg + ofs1);
        tcg_gen_qemu_st_i64(r64, ptr1, s->mem_index, MO_LEQ);
    } else {
        tcg_gen_qemu_ld_i64(r64, ptr, s->mem_index, MO_LEQ);
        tcg_gen_st_i64(r64, cpu_env, arg + ofs);
        tcg_gen_qemu_ld_i64(r64, ptr1, s->mem_index, MO_LEQ);
        tcg_gen_st_i64(r64, cpu_env, arg + ofs1);
    }
    tcg_temp_free_i64(r64);
    tcg_temp_free(ptr1);
}
INSNOP_LDST(xmm, Mqq)
{
    insnop_ldst(xmm, Mdq)(env, s, is_write, arg, ptr);
}
INSNOP_LDST(xmm, Mhq)
{
    const insnop_arg_t(xmm) ofs = offsetof(ZMMReg, ZMM_Q(1));
    const TCGv_i64 r64 = tcg_temp_new_i64();
    if (is_write) {
        tcg_gen_ld_i64(r64, cpu_env, arg + ofs);
        tcg_gen_qemu_st_i64(r64, ptr, s->mem_index, MO_LEQ);
    } else {
        tcg_gen_qemu_ld_i64(r64, ptr, s->mem_index, MO_LEQ);
        tcg_gen_st_i64(r64, cpu_env, arg + ofs);
    }
    tcg_temp_free_i64(r64);
}

/*
 * Code generators
 */
#define gen_insn(mnem, argc, ...)               \
    glue(gen_insn, argc)(mnem, ## __VA_ARGS__)
#define gen_insn0(mnem)                         \
    gen_ ## mnem ## _0
#define gen_insn1(mnem, opT1)                   \
    gen_ ## mnem ## _1 ## opT1
#define gen_insn2(mnem, opT1, opT2)             \
    gen_ ## mnem ## _2 ## opT1 ## opT2
#define gen_insn3(mnem, opT1, opT2, opT3)       \
    gen_ ## mnem ## _3 ## opT1 ## opT2 ## opT3
#define gen_insn4(mnem, opT1, opT2, opT3, opT4)         \
    gen_ ## mnem ## _4 ## opT1 ## opT2 ## opT3 ## opT4
#define gen_insn5(mnem, opT1, opT2, opT3, opT4, opT5)           \
    gen_ ## mnem ## _5 ## opT1 ## opT2 ## opT3 ## opT4 ## opT5

#define GEN_INSN0(mnem)                         \
    static void gen_insn0(mnem)(                \
        CPUX86State *env, DisasContext *s)
#define GEN_INSN1(mnem, opT1)                   \
    static void gen_insn1(mnem, opT1)(          \
        CPUX86State *env, DisasContext *s,      \
        insnop_arg_t(opT1) arg1)
#define GEN_INSN2(mnem, opT1, opT2)                             \
    static void gen_insn2(mnem, opT1, opT2)(                    \
        CPUX86State *env, DisasContext *s,                      \
        insnop_arg_t(opT1) arg1, insnop_arg_t(opT2) arg2)
#define GEN_INSN3(mnem, opT1, opT2, opT3)                       \
    static void gen_insn3(mnem, opT1, opT2, opT3)(              \
        CPUX86State *env, DisasContext *s,                      \
        insnop_arg_t(opT1) arg1, insnop_arg_t(opT2) arg2,       \
        insnop_arg_t(opT3) arg3)
#define GEN_INSN4(mnem, opT1, opT2, opT3, opT4)                 \
    static void gen_insn4(mnem, opT1, opT2, opT3, opT4)(        \
        CPUX86State *env, DisasContext *s,                      \
        insnop_arg_t(opT1) arg1, insnop_arg_t(opT2) arg2,       \
        insnop_arg_t(opT3) arg3, insnop_arg_t(opT4) arg4)
#define GEN_INSN5(mnem, opT1, opT2, opT3, opT4, opT5)           \
    static void gen_insn5(mnem, opT1, opT2, opT3, opT4, opT5)(  \
        CPUX86State *env, DisasContext *s,                      \
        insnop_arg_t(opT1) arg1, insnop_arg_t(opT2) arg2,       \
        insnop_arg_t(opT3) arg3, insnop_arg_t(opT4) arg4,       \
        insnop_arg_t(opT5) arg5)

#define DEF_GEN_INSN0_HELPER(mnem, helper)      \
    GEN_INSN0(mnem)                             \
    {                                           \
        gen_helper_ ## helper(cpu_env);         \
    }

#define DEF_GEN_INSN2_HELPER_EPD(mnem, helper, opT1, opT2)      \
    GEN_INSN2(mnem, opT1, opT2)                                 \
    {                                                           \
        const TCGv_ptr arg1_ptr = tcg_temp_new_ptr();           \
                                                                \
        tcg_gen_addi_ptr(arg1_ptr, cpu_env, arg1);              \
        gen_helper_ ## helper(cpu_env, arg1_ptr, arg2);         \
                                                                \
        tcg_temp_free_ptr(arg1_ptr);                            \
    }
#define DEF_GEN_INSN2_HELPER_DEP(mnem, helper, opT1, opT2)      \
    GEN_INSN2(mnem, opT1, opT2)                                 \
    {                                                           \
        const TCGv_ptr arg2_ptr = tcg_temp_new_ptr();           \
                                                                \
        tcg_gen_addi_ptr(arg2_ptr, cpu_env, arg2);              \
        gen_helper_ ## helper(arg1, cpu_env, arg2_ptr);         \
                                                                \
        tcg_temp_free_ptr(arg2_ptr);                            \
    }
#ifdef TARGET_X86_64
#define DEF_GEN_INSN2_HELPER_EPQ(mnem, helper, opT1, opT2)      \
    DEF_GEN_INSN2_HELPER_EPD(mnem, helper, opT1, opT2)
#define DEF_GEN_INSN2_HELPER_QEP(mnem, helper, opT1, opT2)      \
    DEF_GEN_INSN2_HELPER_DEP(mnem, helper, opT1, opT2)
#else /* !TARGET_X86_64 */
#define DEF_GEN_INSN2_HELPER_EPQ(mnem, helper, opT1, opT2)      \
    GEN_INSN2(mnem, opT1, opT2)                                 \
    {                                                           \
        g_assert_not_reached();                                 \
    }
#define DEF_GEN_INSN2_HELPER_QEP(mnem, helper, opT1, opT2)      \
    GEN_INSN2(mnem, opT1, opT2)                                 \
    {                                                           \
        g_assert_not_reached();                                 \
    }
#endif /* !TARGET_X86_64 */
#define DEF_GEN_INSN2_HELPER_EPP(mnem, helper, opT1, opT2)      \
    GEN_INSN2(mnem, opT1, opT2)                                 \
    {                                                           \
        const TCGv_ptr arg1_ptr = tcg_temp_new_ptr();           \
        const TCGv_ptr arg2_ptr = tcg_temp_new_ptr();           \
                                                                \
        tcg_gen_addi_ptr(arg1_ptr, cpu_env, arg1);              \
        tcg_gen_addi_ptr(arg2_ptr, cpu_env, arg2);              \
        gen_helper_ ## helper(cpu_env, arg1_ptr, arg2_ptr);     \
                                                                \
        tcg_temp_free_ptr(arg1_ptr);                            \
        tcg_temp_free_ptr(arg2_ptr);                            \
    }

#define DEF_GEN_INSN3_HELPER_EPD(mnem, helper, opT1, opT2, opT3)        \
    GEN_INSN3(mnem, opT1, opT2, opT3)                                   \
    {                                                                   \
        const TCGv_ptr arg1_ptr = tcg_temp_new_ptr();                   \
                                                                        \
        assert(arg1 == arg2);                                           \
        tcg_gen_addi_ptr(arg1_ptr, cpu_env, arg1);                      \
        gen_helper_ ## helper(cpu_env, arg1_ptr, arg3);                 \
                                                                        \
        tcg_temp_free_ptr(arg1_ptr);                                    \
    }
#ifdef TARGET_X86_64
#define DEF_GEN_INSN3_HELPER_EPQ(mnem, helper, opT1, opT2, opT3)        \
    DEF_GEN_INSN3_HELPER_EPD(mnem, helper, opT1, opT2, opT3)
#else /* !TARGET_X86_64 */
#define DEF_GEN_INSN3_HELPER_EPQ(mnem, helper, opT1, opT2, opT3)        \
    GEN_INSN3(mnem, opT1, opT2, opT3)                                   \
    {                                                                   \
        g_assert_not_reached();                                         \
    }
#endif /* !TARGET_X86_64 */
#define DEF_GEN_INSN3_HELPER_EPP(mnem, helper, opT1, opT2, opT3)        \
    GEN_INSN3(mnem, opT1, opT2, opT3)                                   \
    {                                                                   \
        const TCGv_ptr arg1_ptr = tcg_temp_new_ptr();                   \
        const TCGv_ptr arg3_ptr = tcg_temp_new_ptr();                   \
                                                                        \
        assert(arg1 == arg2);                                           \
        tcg_gen_addi_ptr(arg1_ptr, cpu_env, arg1);                      \
        tcg_gen_addi_ptr(arg3_ptr, cpu_env, arg3);                      \
        gen_helper_ ## helper(cpu_env, arg1_ptr, arg3_ptr);             \
                                                                        \
        tcg_temp_free_ptr(arg1_ptr);                                    \
        tcg_temp_free_ptr(arg3_ptr);                                    \
    }
#define DEF_GEN_INSN3_HELPER_PPI(mnem, helper, opT1, opT2, opT3)        \
    GEN_INSN3(mnem, opT1, opT2, opT3)                                   \
    {                                                                   \
        const TCGv_ptr arg1_ptr = tcg_temp_new_ptr();                   \
        const TCGv_ptr arg2_ptr = tcg_temp_new_ptr();                   \
        const TCGv_i32 arg3_r32 = tcg_temp_new_i32();                   \
                                                                        \
        tcg_gen_addi_ptr(arg1_ptr, cpu_env, arg1);                      \
        tcg_gen_addi_ptr(arg2_ptr, cpu_env, arg2);                      \
        tcg_gen_movi_i32(arg3_r32, arg3);                               \
        gen_helper_ ## helper(arg1_ptr, arg2_ptr, arg3_r32);            \
                                                                        \
        tcg_temp_free_ptr(arg1_ptr);                                    \
        tcg_temp_free_ptr(arg2_ptr);                                    \
        tcg_temp_free_i32(arg3_r32);                                    \
    }
#define DEF_GEN_INSN3_HELPER_EPPI(mnem, helper, opT1, opT2, opT3)       \
    GEN_INSN3(mnem, opT1, opT2, opT3)                                   \
    {                                                                   \
        const TCGv_ptr arg1_ptr = tcg_temp_new_ptr();                   \
        const TCGv_ptr arg2_ptr = tcg_temp_new_ptr();                   \
        const TCGv_i32 arg3_r32 = tcg_temp_new_i32();                   \
                                                                        \
        tcg_gen_addi_ptr(arg1_ptr, cpu_env, arg1);                      \
        tcg_gen_addi_ptr(arg2_ptr, cpu_env, arg2);                      \
        tcg_gen_movi_i32(arg3_r32, arg3);                               \
        gen_helper_ ## helper(cpu_env, arg1_ptr, arg2_ptr, arg3_r32);   \
                                                                        \
        tcg_temp_free_ptr(arg1_ptr);                                    \
        tcg_temp_free_ptr(arg2_ptr);                                    \
        tcg_temp_free_i32(arg3_r32);                                    \
    }

#define DEF_GEN_INSN4_HELPER_PPI(mnem, helper, opT1, opT2, opT3, opT4)  \
    GEN_INSN4(mnem, opT1, opT2, opT3, opT4)                             \
    {                                                                   \
        const TCGv_ptr arg1_ptr = tcg_temp_new_ptr();                   \
        const TCGv_ptr arg3_ptr = tcg_temp_new_ptr();                   \
        const TCGv_i32 arg4_r32 = tcg_temp_new_i32();                   \
                                                                        \
        assert(arg1 == arg2);                                           \
        tcg_gen_addi_ptr(arg1_ptr, cpu_env, arg1);                      \
        tcg_gen_addi_ptr(arg3_ptr, cpu_env, arg3);                      \
        tcg_gen_movi_i32(arg4_r32, arg4);                               \
        gen_helper_ ## helper(arg1_ptr, arg3_ptr, arg4_r32);            \
                                                                        \
        tcg_temp_free_ptr(arg1_ptr);                                    \
        tcg_temp_free_ptr(arg3_ptr);                                    \
        tcg_temp_free_i32(arg4_r32);                                    \
    }
#define DEF_GEN_INSN4_HELPER_EPPI(mnem, helper, opT1, opT2, opT3, opT4) \
    GEN_INSN4(mnem, opT1, opT2, opT3, opT4)                             \
    {                                                                   \
        const TCGv_ptr arg1_ptr = tcg_temp_new_ptr();                   \
        const TCGv_ptr arg3_ptr = tcg_temp_new_ptr();                   \
        const TCGv_i32 arg4_r32 = tcg_temp_new_i32();                   \
                                                                        \
        assert(arg1 == arg2);                                           \
        tcg_gen_addi_ptr(arg1_ptr, cpu_env, arg1);                      \
        tcg_gen_addi_ptr(arg3_ptr, cpu_env, arg3);                      \
        tcg_gen_movi_i32(arg4_r32, arg4);                               \
        gen_helper_ ## helper(cpu_env, arg1_ptr, arg3_ptr, arg4_r32);   \
                                                                        \
        tcg_temp_free_ptr(arg1_ptr);                                    \
        tcg_temp_free_ptr(arg3_ptr);                                    \
        tcg_temp_free_i32(arg4_r32);                                    \
    }

#define MM_OPRSZ sizeof(MMXReg)
#define MM_MAXSZ sizeof(MMXReg)
#define XMM_OPRSZ (!(s->prefix & PREFIX_VEX) ? sizeof(XMMReg) : \
                   s->vex_l ? sizeof(XMMReg) :                  \
                   sizeof(XMMReg))
#define XMM_MAXSZ (!(s->prefix & PREFIX_VEX) ? sizeof(XMMReg) : \
                   sizeof(YMMReg))

#define DEF_GEN_INSN2_GVEC(mnem, opT1, opT2, gvec, ...) \
    GEN_INSN2(mnem, opT1, opT2)                         \
    {                                                   \
        gen_gvec_ ## gvec(arg1, arg2, ## __VA_ARGS__);  \
    }
#define DEF_GEN_INSN3_GVEC(mnem, opT1, opT2, opT3, gvec, ...)   \
    GEN_INSN3(mnem, opT1, opT2, opT3)                           \
    {                                                           \
        gen_gvec_ ## gvec(arg1, arg2, arg3, ## __VA_ARGS__);    \
    }
#define DEF_GEN_INSN4_GVEC(mnem, opT1, opT2, opT3, opT4, gvec, ...)     \
    GEN_INSN4(mnem, opT1, opT2, opT3, opT4)                             \
    {                                                                   \
        gen_gvec_ ## gvec(arg1, arg2, arg3, arg4, ## __VA_ARGS__);      \
    }

GEN_INSN2(movq, Pq, Eq);        /* forward declaration */
GEN_INSN2(movq, Vdq, Eq);       /* forward declaration */
GEN_INSN2(movd, Pq, Ed)
{
    const insnop_arg_t(Eq) arg2_r64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(arg2_r64, arg2);
    gen_insn2(movq, Pq, Eq)(env, s, arg1, arg2_r64);
    tcg_temp_free_i64(arg2_r64);
}
GEN_INSN2(movd, Ed, Pq)
{
    tcg_gen_ld_i32(arg1, cpu_env, arg2 + offsetof(MMXReg, MMX_L(0)));
}
GEN_INSN2(movd, Vdq, Ed)
{
    const insnop_arg_t(Eq) arg2_r64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(arg2_r64, arg2);
    gen_insn2(movq, Vdq, Eq)(env, s, arg1, arg2_r64);
    tcg_temp_free_i64(arg2_r64);
}
GEN_INSN2(movd, Ed, Vdq)
{
    tcg_gen_ld_i32(arg1, cpu_env, arg2 + offsetof(ZMMReg, ZMM_L(0)));
}
GEN_INSN2(vmovd, Vdq, Ed)
{
    gen_insn2(movd, Vdq, Ed)(env, s, arg1, arg2);
}
GEN_INSN2(vmovd, Ed, Vdq)
{
    gen_insn2(movd, Ed, Vdq)(env, s, arg1, arg2);
}

GEN_INSN2(movq, Pq, Eq)
{
    tcg_gen_st_i64(arg2, cpu_env, arg1 + offsetof(MMXReg, MMX_Q(0)));
}
GEN_INSN2(movq, Eq, Pq)
{
    tcg_gen_ld_i64(arg1, cpu_env, arg2 + offsetof(MMXReg, MMX_Q(0)));
}
GEN_INSN2(movq, Vdq, Eq)
{
    const TCGv_i64 r64 = tcg_temp_new_i64();
    tcg_gen_st_i64(arg2, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(0)));
    tcg_gen_movi_i64(r64, 0);
    tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(1)));
    tcg_temp_free_i64(r64);
}
GEN_INSN2(movq, Eq, Vdq)
{
    tcg_gen_ld_i64(arg1, cpu_env, arg2 + offsetof(ZMMReg, ZMM_Q(0)));
}
GEN_INSN2(vmovq, Vdq, Eq)
{
    gen_insn2(movq, Vdq, Eq)(env, s, arg1, arg2);
}
GEN_INSN2(vmovq, Eq, Vdq)
{
    gen_insn2(movq, Eq, Vdq)(env, s, arg1, arg2);
}

DEF_GEN_INSN2_GVEC(movq, Pq, Qq, mov, MM_OPRSZ, MM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(movq, Qq, Pq, mov, MM_OPRSZ, MM_MAXSZ, MO_64)
GEN_INSN2(movq, Vdq, Wq)
{
    const TCGv_i64 r64 = tcg_temp_new_i64();
    tcg_gen_ld_i64(r64, cpu_env, arg2 + offsetof(ZMMReg, ZMM_Q(0)));
    gen_insn2(movq, Vdq, Eq)(env, s, arg1, r64);
    tcg_temp_free_i64(r64);
}
GEN_INSN2(movq, UdqMq, Vq)
{
    gen_insn2(movq, Vdq, Wq)(env, s, arg1, arg2);
}
GEN_INSN2(vmovq, Vdq, Wq)
{
    gen_insn2(movq, Vdq, Wq)(env, s, arg1, arg2);
}
GEN_INSN2(vmovq, UdqMq, Vq)
{
    gen_insn2(movq, UdqMq, Vq)(env, s, arg1, arg2);
}

DEF_GEN_INSN2_GVEC(movaps, Vdq, Wdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(movaps, Wdq, Vdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovaps, Vdq, Wdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovaps, Wdq, Vdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovaps, Vqq, Wqq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovaps, Wqq, Vqq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(movapd, Vdq, Wdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(movapd, Wdq, Vdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovapd, Vdq, Wdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovapd, Wdq, Vdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovapd, Vqq, Wqq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovapd, Wqq, Vqq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(movdqa, Vdq, Wdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(movdqa, Wdq, Vdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovdqa, Vdq, Wdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovdqa, Wdq, Vdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovdqa, Vqq, Wqq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovdqa, Wqq, Vqq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(movups, Vdq, Wdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(movups, Wdq, Vdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovups, Vdq, Wdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovups, Wdq, Vdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovups, Vqq, Wqq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovups, Wqq, Vqq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(movupd, Vdq, Wdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(movupd, Wdq, Vdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovupd, Vdq, Wdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovupd, Wdq, Vdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovupd, Vqq, Wqq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovupd, Wqq, Vqq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(movdqu, Vdq, Wdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(movdqu, Wdq, Vdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovdqu, Vdq, Wdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovdqu, Wdq, Vdq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovdqu, Vqq, Wqq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN2_GVEC(vmovdqu, Wqq, Vqq, mov, XMM_OPRSZ, XMM_MAXSZ, MO_64)

GEN_INSN4(movss, Vdq, Vdq, Wd, modrm_mod)
{
    const TCGv_i32 r32 = tcg_temp_new_i32();
    const TCGv_i64 r64 = tcg_temp_new_i64();
    if (arg4 == 3) {
        /* merging movss */
        tcg_gen_ld_i32(r32, cpu_env, arg3 + offsetof(ZMMReg, ZMM_L(0)));
        tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(0)));
        if (arg1 != arg2) {
            tcg_gen_ld_i32(r32, cpu_env, arg2 + offsetof(ZMMReg, ZMM_L(1)));
            tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(1)));
            tcg_gen_ld_i64(r64, cpu_env, arg2 + offsetof(ZMMReg, ZMM_Q(1)));
            tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(1)));
        }
    } else {
        /* zero-extending movss */
        tcg_gen_ld_i32(r32, cpu_env, arg3 + offsetof(ZMMReg, ZMM_L(0)));
        tcg_gen_extu_i32_i64(r64, r32);
        tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(0)));
        tcg_gen_movi_i64(r64, 0);
        tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(1)));
    }
    tcg_temp_free_i32(r32);
    tcg_temp_free_i64(r64);
}
GEN_INSN2(movss, Wd, Vd)
{
    gen_insn4(movss, Vdq, Vdq, Wd, modrm_mod)(env, s, arg1, arg1, arg2, 3);
}
GEN_INSN5(vmovss, Vdq, Hdq, Wd, modrm_mod, vex_v)
{
    if (arg4 == 3 || arg5 == 0) {
        gen_insn4(movss, Vdq, Vdq, Wd, modrm_mod)(env, s, arg1,
                                                  arg2, arg3, arg4);
    } else {
        gen_unknown_opcode(env, s);
    }
}
GEN_INSN5(vmovss, Wdq, Hdq, Vd, modrm_mod, vex_v)
{
    if (arg4 == 3 || arg5 == 0) {
        gen_insn4(movss, Vdq, Vdq, Wd, modrm_mod)(env, s, arg1,
                                                  arg2, arg3, 3);
    } else {
        gen_unknown_opcode(env, s);
    }
}

GEN_INSN4(movsd, Vdq, Vdq, Wq, modrm_mod)
{
    const TCGv_i64 r64 = tcg_temp_new_i64();
    tcg_gen_ld_i64(r64, cpu_env, arg3 + offsetof(ZMMReg, ZMM_Q(0)));
    tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(0)));
    if (arg4 == 3) {
        /* merging movsd */
        if (arg1 != arg2) {
            tcg_gen_ld_i64(r64, cpu_env, arg2 + offsetof(ZMMReg, ZMM_Q(1)));
            tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(1)));
        }
    } else {
        /* zero-extending movsd */
        tcg_gen_movi_i64(r64, 0);
        tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(1)));
    }
    tcg_temp_free_i64(r64);
}
GEN_INSN2(movsd, Wq, Vq)
{
    gen_insn4(movsd, Vdq, Vdq, Wq, modrm_mod)(env, s, arg1, arg1, arg2, 3);
}
GEN_INSN5(vmovsd, Vdq, Hdq, Wq, modrm_mod, vex_v)
{
    if (arg4 == 3 || arg5 == 0) {
        gen_insn4(movsd, Vdq, Vdq, Wq, modrm_mod)(env, s, arg1,
                                                  arg2, arg3, arg4);
    } else {
        gen_unknown_opcode(env, s);
    }
}
GEN_INSN5(vmovsd, Wdq, Hdq, Vq, modrm_mod, vex_v)
{
    if (arg4 == 3 || arg5 == 0) {
        gen_insn4(movsd, Vdq, Vdq, Wq, modrm_mod)(env, s, arg1,
                                                  arg2, arg3, 3);
    } else {
        gen_unknown_opcode(env, s);
    }
}

GEN_INSN2(movq2dq, Vdq, Nq)
{
    const insnop_arg_t(Vdq) dofs = offsetof(ZMMReg, ZMM_Q(0));
    const insnop_arg_t(Nq) aofs = offsetof(MMXReg, MMX_Q(0));
    gen_op_movq(s, arg1 + dofs, arg2 + aofs);

    const TCGv_i64 r64z = tcg_const_i64(0);
    tcg_gen_st_i64(r64z, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(1)));
    tcg_temp_free_i64(r64z);
}
GEN_INSN2(movdq2q, Pq, Uq)
{
    const insnop_arg_t(Pq) dofs = offsetof(MMXReg, MMX_Q(0));
    const insnop_arg_t(Uq) aofs = offsetof(ZMMReg, ZMM_Q(0));
    gen_op_movq(s, arg1 + dofs, arg2 + aofs);
}

GEN_INSN3(movhlps, Vdq, Vdq, UdqMhq)
{
    const TCGv_i64 r64 = tcg_temp_new_i64();
    tcg_gen_ld_i64(r64, cpu_env, arg3 + offsetof(ZMMReg, ZMM_Q(1)));
    tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(0)));
    if (arg1 != arg2) {
        tcg_gen_ld_i64(r64, cpu_env, arg2 + offsetof(ZMMReg, ZMM_Q(1)));
        tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(1)));
    }
    tcg_temp_free_i64(r64);
}
GEN_INSN3(vmovhlps, Vdq, Hdq, UdqMhq)
{
    gen_insn3(movhlps, Vdq, Vdq, UdqMhq)(env, s, arg1, arg2, arg3);
}
GEN_INSN2(movlps, Mq, Vq)
{
    insnop_ldst(xmm, Mq)(env, s, 1, arg2, arg1);
}
GEN_INSN2(vmovlps, Mq, Vq)
{
    gen_insn2(movlps, Mq, Vq)(env, s, arg1, arg2);
}

GEN_INSN3(movlpd, Vdq, Vdq, Mq)
{
    insnop_ldst(xmm, Mq)(env, s, 0, arg1, arg3);
    if (arg1 != arg2) {
        const TCGv_i64 r64 = tcg_temp_new_i64();
        tcg_gen_ld_i64(r64, cpu_env, arg2 + offsetof(ZMMReg, ZMM_Q(1)));
        tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(1)));
        tcg_temp_free_i64(r64);
    }
}
GEN_INSN3(vmovlpd, Vdq, Hdq, Mq)
{
    gen_insn3(movlpd, Vdq, Vdq, Mq)(env, s, arg1, arg2, arg3);
}
GEN_INSN2(movlpd, Mq, Vq)
{
    insnop_ldst(xmm, Mq)(env, s, 1, arg2, arg1);
}
GEN_INSN2(vmovlpd, Mq, Vq)
{
    gen_insn2(movlpd, Mq, Vq)(env, s, arg1, arg2);
}

GEN_INSN3(movlhps, Vdq, Vq, Wq)
{
    const TCGv_i64 r64 = tcg_temp_new_i64();
    tcg_gen_ld_i64(r64, cpu_env, arg3 + offsetof(ZMMReg, ZMM_Q(0)));
    tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(1)));
    if (arg1 != arg2) {
        tcg_gen_ld_i64(r64, cpu_env, arg2 + offsetof(ZMMReg, ZMM_Q(0)));
        tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(0)));
    }
    tcg_temp_free_i64(r64);
}
GEN_INSN3(vmovlhps, Vdq, Hq, Wq)
{
    gen_insn3(movlhps, Vdq, Vq, Wq)(env, s, arg1, arg2, arg3);
}
GEN_INSN2(movhps, Mq, Vdq)
{
    insnop_ldst(xmm, Mhq)(env, s, 1, arg2, arg1);
}
GEN_INSN2(vmovhps, Mq, Vdq)
{
    gen_insn2(movhps, Mq, Vdq)(env, s, arg1, arg2);
}

GEN_INSN3(movhpd, Vdq, Vq, Mq)
{
    if (arg1 != arg2) {
        const TCGv_i64 r64 = tcg_temp_new_i64();
        tcg_gen_ld_i64(r64, cpu_env, arg2 + offsetof(ZMMReg, ZMM_Q(0)));
        tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(0)));
        tcg_temp_free_i64(r64);
    }
    insnop_ldst(xmm, Mhq)(env, s, 0, arg1, arg3);
}
GEN_INSN2(movhpd, Mq, Vdq)
{
    insnop_ldst(xmm, Mhq)(env, s, 1, arg2, arg1);
}
GEN_INSN3(vmovhpd, Vdq, Hq, Mq)
{
    gen_insn3(movhpd, Vdq, Vq, Mq)(env, s, arg1, arg2, arg3);
}
GEN_INSN2(vmovhpd, Mq, Vdq)
{
    gen_insn2(movhpd, Mq, Vdq)(env, s, arg1, arg2);
}

DEF_GEN_INSN2_HELPER_DEP(pmovmskb, pmovmskb_mmx, Gd, Nq)
GEN_INSN2(pmovmskb, Gq, Nq)
{
    const TCGv_i32 arg1_r32 = tcg_temp_new_i32();
    gen_insn2(pmovmskb, Gd, Nq)(env, s, arg1_r32, arg2);
    tcg_gen_extu_i32_i64(arg1, arg1_r32);
    tcg_temp_free_i32(arg1_r32);
}
DEF_GEN_INSN2_HELPER_DEP(pmovmskb, pmovmskb_xmm, Gd, Udq)
GEN_INSN2(pmovmskb, Gq, Udq)
{
    const TCGv_i32 arg1_r32 = tcg_temp_new_i32();
    gen_insn2(pmovmskb, Gd, Udq)(env, s, arg1_r32, arg2);
    tcg_gen_extu_i32_i64(arg1, arg1_r32);
    tcg_temp_free_i32(arg1_r32);
}
DEF_GEN_INSN2_HELPER_DEP(vpmovmskb, pmovmskb_xmm, Gd, Udq)
GEN_INSN2(vpmovmskb, Gq, Udq)
{
    const TCGv_i32 arg1_r32 = tcg_temp_new_i32();
    gen_insn2(vpmovmskb, Gd, Udq)(env, s, arg1_r32, arg2);
    tcg_gen_extu_i32_i64(arg1, arg1_r32);
    tcg_temp_free_i32(arg1_r32);
}
DEF_GEN_INSN2_HELPER_DEP(vpmovmskb, pmovmskb_xmm, Gd, Uqq)
GEN_INSN2(vpmovmskb, Gq, Uqq)
{
    const TCGv_i32 arg1_r32 = tcg_temp_new_i32();
    gen_insn2(vpmovmskb, Gd, Uqq)(env, s, arg1_r32, arg2);
    tcg_gen_extu_i32_i64(arg1, arg1_r32);
    tcg_temp_free_i32(arg1_r32);
}

DEF_GEN_INSN2_HELPER_DEP(movmskps, movmskps, Gd, Udq)
GEN_INSN2(movmskps, Gq, Udq)
{
    const TCGv_i32 arg1_r32 = tcg_temp_new_i32();
    gen_insn2(movmskps, Gd, Udq)(env, s, arg1_r32, arg2);
    tcg_gen_extu_i32_i64(arg1, arg1_r32);
    tcg_temp_free_i32(arg1_r32);
}
DEF_GEN_INSN2_HELPER_DEP(vmovmskps, movmskps, Gd, Udq)
GEN_INSN2(vmovmskps, Gq, Udq)
{
    const TCGv_i32 arg1_r32 = tcg_temp_new_i32();
    gen_insn2(vmovmskps, Gd, Udq)(env, s, arg1_r32, arg2);
    tcg_gen_extu_i32_i64(arg1, arg1_r32);
    tcg_temp_free_i32(arg1_r32);
}
DEF_GEN_INSN2_HELPER_DEP(vmovmskps, movmskps, Gd, Uqq)
GEN_INSN2(vmovmskps, Gq, Uqq)
{
    const TCGv_i32 arg1_r32 = tcg_temp_new_i32();
    gen_insn2(vmovmskps, Gd, Uqq)(env, s, arg1_r32, arg2);
    tcg_gen_extu_i32_i64(arg1, arg1_r32);
    tcg_temp_free_i32(arg1_r32);
}

DEF_GEN_INSN2_HELPER_DEP(movmskpd, movmskpd, Gd, Udq)
GEN_INSN2(movmskpd, Gq, Udq)
{
    const TCGv_i32 arg1_r32 = tcg_temp_new_i32();
    gen_insn2(movmskpd, Gd, Udq)(env, s, arg1_r32, arg2);
    tcg_gen_extu_i32_i64(arg1, arg1_r32);
    tcg_temp_free_i32(arg1_r32);
}
DEF_GEN_INSN2_HELPER_DEP(vmovmskpd, movmskpd, Gd, Udq)
GEN_INSN2(vmovmskpd, Gq, Udq)
{
    const TCGv_i32 arg1_r32 = tcg_temp_new_i32();
    gen_insn2(vmovmskpd, Gd, Udq)(env, s, arg1_r32, arg2);
    tcg_gen_extu_i32_i64(arg1, arg1_r32);
    tcg_temp_free_i32(arg1_r32);
}
DEF_GEN_INSN2_HELPER_DEP(vmovmskpd, movmskpd, Gd, Uqq)
GEN_INSN2(vmovmskpd, Gq, Uqq)
{
    const TCGv_i32 arg1_r32 = tcg_temp_new_i32();
    gen_insn2(vmovmskpd, Gd, Uqq)(env, s, arg1_r32, arg2);
    tcg_gen_extu_i32_i64(arg1, arg1_r32);
    tcg_temp_free_i32(arg1_r32);
}

GEN_INSN2(lddqu, Vdq, Mdq)
{
    insnop_ldst(xmm, Mdq)(env, s, 0, arg1, arg2);
}
GEN_INSN2(vlddqu, Vdq, Mdq)
{
    gen_insn2(lddqu, Vdq, Mdq)(env, s, arg1, arg2);
}
GEN_INSN2(vlddqu, Vqq, Mqq)
{
    gen_insn2(vlddqu, Vdq, Mdq)(env, s, arg1, arg2);
}

GEN_INSN2(movshdup, Vdq, Wdq)
{
    const TCGv_i32 r32 = tcg_temp_new_i32();

    tcg_gen_ld_i32(r32, cpu_env, arg2 + offsetof(ZMMReg, ZMM_L(1)));
    tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(0)));
    if (arg1 != arg2) {
        tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(1)));
    }

    tcg_gen_ld_i32(r32, cpu_env, arg2 + offsetof(ZMMReg, ZMM_L(3)));
    tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(2)));
    if (arg1 != arg2) {
        tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(3)));
    }

    tcg_temp_free_i32(r32);
}
GEN_INSN2(vmovshdup, Vdq, Wdq)
{
    gen_insn2(movshdup, Vdq, Wdq)(env, s, arg1, arg2);
}
GEN_INSN2(vmovshdup, Vqq, Wqq)
{
    gen_insn2(vmovshdup, Vdq, Wdq)(env, s, arg1, arg2);
}

GEN_INSN2(movsldup, Vdq, Wdq)
{
    const TCGv_i32 r32 = tcg_temp_new_i32();

    tcg_gen_ld_i32(r32, cpu_env, arg2 + offsetof(ZMMReg, ZMM_L(0)));
    if (arg1 != arg2) {
        tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(0)));
    }
    tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(1)));

    tcg_gen_ld_i32(r32, cpu_env, arg2 + offsetof(ZMMReg, ZMM_L(2)));
    if (arg1 != arg2) {
        tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(2)));
    }
    tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(3)));

    tcg_temp_free_i32(r32);
}
GEN_INSN2(vmovsldup, Vdq, Wdq)
{
    gen_insn2(movsldup, Vdq, Wdq)(env, s, arg1, arg2);
}
GEN_INSN2(vmovsldup, Vqq, Wqq)
{
    gen_insn2(movsldup, Vdq, Wdq)(env, s, arg1, arg2);
}

GEN_INSN2(movddup, Vdq, Wq)
{
    const TCGv_i64 r64 = tcg_temp_new_i64();

    tcg_gen_ld_i64(r64, cpu_env, arg2 + offsetof(ZMMReg, ZMM_Q(0)));
    if (arg1 != arg2) {
        tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(0)));
    }
    tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(1)));

    tcg_temp_free_i64(r64);
}
GEN_INSN2(vmovddup, Vdq, Wq)
{
    gen_insn2(movddup, Vdq, Wq)(env, s, arg1, arg2);
}
GEN_INSN2(vmovddup, Vqq, Wqq)
{
    gen_insn2(vmovddup, Vdq, Wq)(env, s, arg1, arg2);
}

DEF_GEN_INSN3_GVEC(paddb, Pq, Pq, Qq, add, MM_OPRSZ, MM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(paddb, Vdq, Vdq, Wdq, add, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpaddb, Vdq, Hdq, Wdq, add, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpaddb, Vqq, Hqq, Wqq, add, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(paddw, Pq, Pq, Qq, add, MM_OPRSZ, MM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(paddw, Vdq, Vdq, Wdq, add, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpaddw, Vdq, Hdq, Wdq, add, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpaddw, Vqq, Hqq, Wqq, add, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(paddd, Pq, Pq, Qq, add, MM_OPRSZ, MM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(paddd, Vdq, Vdq, Wdq, add, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(vpaddd, Vdq, Hdq, Wdq, add, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(vpaddd, Vqq, Hqq, Wqq, add, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(paddq, Pq, Pq, Qq, add, MM_OPRSZ, MM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(paddq, Vdq, Vdq, Wdq, add, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vpaddq, Vdq, Hdq, Wdq, add, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vpaddq, Vqq, Hqq, Wqq, add, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(paddsb, Pq, Pq, Qq, ssadd, MM_OPRSZ, MM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(paddsb, Vdq, Vdq, Wdq, ssadd, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpaddsb, Vdq, Hdq, Wdq, ssadd, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpaddsb, Vqq, Hqq, Wqq, ssadd, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(paddsw, Pq, Pq, Qq, ssadd, MM_OPRSZ, MM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(paddsw, Vdq, Vdq, Wdq, ssadd, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpaddsw, Vdq, Hdq, Wdq, ssadd, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpaddsw, Vqq, Hqq, Wqq, ssadd, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(paddusb, Pq, Pq, Qq, usadd, MM_OPRSZ, MM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(paddusb, Vdq, Vdq, Wdq, usadd, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpaddusb, Vdq, Hdq, Wdq, usadd, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpaddusb, Vqq, Hqq, Wqq, usadd, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(paddusw, Pq, Pq, Qq, usadd, MM_OPRSZ, MM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(paddusw, Vdq, Vdq, Wdq, usadd, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpaddusw, Vdq, Hdq, Wdq, usadd, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpaddusw, Vqq, Hqq, Wqq, usadd, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_HELPER_EPP(addps, addps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vaddps, addps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vaddps, addps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(addpd, addpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vaddpd, addpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vaddpd, addpd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(addss, addss, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vaddss, addss, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(addsd, addsd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vaddsd, addsd, Vq, Hq, Wq)
DEF_GEN_INSN3_HELPER_EPP(phaddw, phaddw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(phaddw, phaddw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vphaddw, phaddw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vphaddw, phaddw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(phaddd, phaddd_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(phaddd, phaddd_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vphaddd, phaddd_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vphaddd, phaddd_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(phaddsw, phaddsw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(phaddsw, phaddsw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vphaddsw, phaddsw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vphaddsw, phaddsw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(haddps, haddps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vhaddps, haddps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vhaddps, haddps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(haddpd, haddpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vhaddpd, haddpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vhaddpd, haddpd, Vqq, Hqq, Wqq)

DEF_GEN_INSN3_GVEC(psubb, Pq, Pq, Qq, sub, MM_OPRSZ, MM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(psubb, Vdq, Vdq, Wdq, sub, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpsubb, Vdq, Hdq, Wdq, sub, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpsubb, Vqq, Hqq, Wqq, sub, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(psubw, Pq, Pq, Qq, sub, MM_OPRSZ, MM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(psubw, Vdq, Vdq, Wdq, sub, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpsubw, Vdq, Hdq, Wdq, sub, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpsubw, Vqq, Hqq, Wqq, sub, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(psubd, Pq, Pq, Qq, sub, MM_OPRSZ, MM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(psubd, Vdq, Vdq, Wdq, sub, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(vpsubd, Vdq, Hdq, Wdq, sub, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(vpsubd, Vqq, Hqq, Wqq, sub, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(psubq, Pq, Pq, Qq, sub, MM_OPRSZ, MM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(psubq, Vdq, Vdq, Wdq, sub, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vpsubq, Vdq, Hdq, Wdq, sub, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vpsubq, Vqq, Hqq, Wqq, sub, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(psubsb, Pq, Pq, Qq, sssub, MM_OPRSZ, MM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(psubsb, Vdq, Vdq, Wdq, sssub, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpsubsb, Vdq, Hdq, Wdq, sssub, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpsubsb, Vqq, Hqq, Wqq, sssub, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(psubsw, Pq, Pq, Qq, sssub, MM_OPRSZ, MM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(psubsw, Vdq, Vdq, Wdq, sssub, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpsubsw, Vdq, Hdq, Wdq, sssub, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpsubsw, Vqq, Hqq, Wqq, sssub, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(psubusb, Pq, Pq, Qq, ussub, MM_OPRSZ, MM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(psubusb, Vdq, Vdq, Wdq, ussub, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpsubusb, Vdq, Hdq, Wdq, ussub, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpsubusb, Vqq, Hqq, Wqq, ussub, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(psubusw, Pq, Pq, Qq, ussub, MM_OPRSZ, MM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(psubusw, Vdq, Vdq, Wdq, ussub, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpsubusw, Vdq, Hdq, Wdq, ussub, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpsubusw, Vqq, Hqq, Wqq, ussub, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_HELPER_EPP(subps, subps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vsubps, subps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vsubps, subps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(subpd, subpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vsubpd, subpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vsubpd, subpd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(subss, subss, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vsubss, subss, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(subsd, subsd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vsubsd, subsd, Vq, Hq, Wq)
DEF_GEN_INSN3_HELPER_EPP(phsubw, phsubw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(phsubw, phsubw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vphsubw, phsubw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vphsubw, phsubw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(phsubd, phsubd_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(phsubd, phsubd_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vphsubd, phsubd_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vphsubd, phsubd_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(phsubsw, phsubsw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(phsubsw, phsubsw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vphsubsw, phsubsw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vphsubsw, phsubsw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(hsubps, hsubps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vhsubps, hsubps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vhsubps, hsubps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(hsubpd, hsubpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vhsubpd, hsubpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vhsubpd, hsubpd, Vqq, Hqq, Wqq)

DEF_GEN_INSN3_HELPER_EPP(addsubps, addsubps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vaddsubps, addsubps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vaddsubps, addsubps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(addsubpd, addsubpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vaddsubpd, addsubpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vaddsubpd, addsubpd, Vqq, Hqq, Wqq)

DEF_GEN_INSN3_HELPER_EPP(pmullw, pmullw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(pmullw, pmullw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmullw, pmullw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmullw, pmullw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(pmulld, pmulld_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmulld, pmulld_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmulld, pmulld_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(pmulhw, pmulhw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(pmulhw, pmulhw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmulhw, pmulhw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmulhw, pmulhw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(pmulhuw, pmulhuw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(pmulhuw, pmulhuw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmulhuw, pmulhuw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmulhuw, pmulhuw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(pmuldq, pmuldq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmuldq, pmuldq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmuldq, pmuldq_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(pmuludq, pmuludq_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(pmuludq, pmuludq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmuludq, pmuludq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmuludq, pmuludq_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(pmulhrsw, pmulhrsw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(pmulhrsw, pmulhrsw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmulhrsw, pmulhrsw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmulhrsw, pmulhrsw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(mulps, mulps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vmulps, mulps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vmulps, mulps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(mulpd, mulpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vmulpd, mulpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vmulpd, mulpd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(mulss, mulss, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vmulss, mulss, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(mulsd, mulsd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vmulsd, mulsd, Vq, Hq, Wq)
DEF_GEN_INSN3_HELPER_EPP(pmaddwd, pmaddwd_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(pmaddwd, pmaddwd_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmaddwd, pmaddwd_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmaddwd, pmaddwd_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(pmaddubsw, pmaddubsw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(pmaddubsw, pmaddubsw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmaddubsw, pmaddubsw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpmaddubsw, pmaddubsw_xmm, Vqq, Hqq, Wqq)

DEF_GEN_INSN3_HELPER_EPP(divps, divps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vdivps, divps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vdivps, divps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(divpd, divpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vdivpd, divpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vdivpd, divpd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(divss, divss, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vdivss, divss, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(divsd, divsd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vdivsd, divsd, Vq, Hq, Wq)

DEF_GEN_INSN2_HELPER_EPP(rcpps, rcpps, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vrcpps, rcpps, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vrcpps, rcpps, Vqq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(rcpss, rcpss, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vrcpss, rcpss, Vd, Hd, Wd)
DEF_GEN_INSN2_HELPER_EPP(sqrtps, sqrtps, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vsqrtps, sqrtps, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vsqrtps, sqrtps, Vqq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(sqrtpd, sqrtpd, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vsqrtpd, sqrtpd, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vsqrtpd, sqrtpd, Vqq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(sqrtss, sqrtss, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vsqrtss, sqrtss, Vd, Hd, Wd)
DEF_GEN_INSN2_HELPER_EPP(sqrtsd, sqrtsd, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vsqrtsd, sqrtsd, Vq, Hq, Wq)
DEF_GEN_INSN2_HELPER_EPP(rsqrtps, rsqrtps, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vrsqrtps, rsqrtps, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vrsqrtps, rsqrtps, Vqq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(rsqrtss, rsqrtss, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vrsqrtss, rsqrtss, Vd, Hd, Wd)

DEF_GEN_INSN3_GVEC(pminub, Pq, Pq, Qq, umin, MM_OPRSZ, MM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(pminub, Vdq, Vdq, Wdq, umin, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpminub, Vdq, Hdq, Wdq, umin, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpminub, Vqq, Hqq, Wqq, umin, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(pminuw, Vdq, Vdq, Wdq, umin, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpminuw, Vdq, Hdq, Wdq, umin, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpminuw, Vqq, Hqq, Wqq, umin, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(pminud, Vdq, Vdq, Wdq, umin, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(vpminud, Vdq, Hdq, Wdq, umin, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(vpminud, Vqq, Hqq, Wqq, umin, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(pminsb, Vdq, Vdq, Wdq, smin, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpminsb, Vdq, Hdq, Wdq, smin, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpminsb, Vqq, Hqq, Wqq, smin, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(pminsw, Pq, Pq, Qq, smin, MM_OPRSZ, MM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(pminsw, Vdq, Vdq, Wdq, smin, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpminsw, Vdq, Hdq, Wdq, smin, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpminsw, Vqq, Hqq, Wqq, smin, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(pminsd, Vdq, Vdq, Wdq, smin, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(vpminsd, Vdq, Hdq, Wdq, smin, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(vpminsd, Vqq, Hqq, Wqq, smin, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_HELPER_EPP(minps, minps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vminps, minps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vminps, minps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(minpd, minpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vminpd, minpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vminpd, minpd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(minss, minss, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vminss, minss, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(minsd, minsd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vminsd, minsd, Vq, Hq, Wq)
DEF_GEN_INSN2_HELPER_EPP(phminposuw, phminposuw_xmm, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vphminposuw, phminposuw_xmm, Vdq, Wdq)
DEF_GEN_INSN3_GVEC(pmaxub, Pq, Pq, Qq, umax, MM_OPRSZ, MM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(pmaxub, Vdq, Vdq, Wdq, umax, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpmaxub, Vdq, Hdq, Wdq, umax, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpmaxub, Vqq, Hqq, Wqq, umax, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(pmaxuw, Vdq, Vdq, Wdq, umax, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpmaxuw, Vdq, Hdq, Wdq, umax, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpmaxuw, Vqq, Hqq, Wqq, umax, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(pmaxud, Vdq, Vdq, Wdq, umax, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(vpmaxud, Vdq, Hdq, Wdq, umax, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(vpmaxud, Vqq, Hqq, Wqq, umax, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(pmaxsb, Vdq, Vdq, Wdq, smax, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpmaxsb, Vdq, Hdq, Wdq, smax, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(vpmaxsb, Vqq, Hqq, Wqq, smax, XMM_OPRSZ, XMM_MAXSZ, MO_8)
DEF_GEN_INSN3_GVEC(pmaxsw, Pq, Pq, Qq, smax, MM_OPRSZ, MM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(pmaxsw, Vdq, Vdq, Wdq, smax, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpmaxsw, Vdq, Hdq, Wdq, smax, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(vpmaxsw, Vqq, Hqq, Wqq, smax, XMM_OPRSZ, XMM_MAXSZ, MO_16)
DEF_GEN_INSN3_GVEC(pmaxsd, Vdq, Vdq, Wdq, smax, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(vpmaxsd, Vdq, Hdq, Wdq, smax, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_GVEC(vpmaxsd, Vqq, Hqq, Wqq, smax, XMM_OPRSZ, XMM_MAXSZ, MO_32)
DEF_GEN_INSN3_HELPER_EPP(maxps, maxps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vmaxps, maxps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vmaxps, maxps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(maxpd, maxpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vmaxpd, maxpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vmaxpd, maxpd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(maxss, maxss, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vmaxss, maxss, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(maxsd, maxsd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vmaxsd, maxsd, Vq, Hq, Wq)
DEF_GEN_INSN3_HELPER_EPP(pavgb, pavgb_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(pavgb, pavgb_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpavgb, pavgb_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpavgb, pavgb_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(pavgw, pavgw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(pavgw, pavgw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpavgw, pavgw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpavgw, pavgw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(psadbw, psadbw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(psadbw, psadbw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsadbw, psadbw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsadbw, psadbw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN4_HELPER_EPPI(mpsadbw, mpsadbw_xmm, Vdq, Vdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vmpsadbw, mpsadbw_xmm, Vdq, Hdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vmpsadbw, mpsadbw_xmm, Vqq, Hqq, Wqq, Ib)
DEF_GEN_INSN2_HELPER_EPP(pabsb, pabsb_mmx, Pq, Qq)
DEF_GEN_INSN2_HELPER_EPP(pabsb, pabsb_xmm, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vpabsb, pabsb_xmm, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vpabsb, pabsb_xmm, Vqq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(pabsw, pabsw_mmx, Pq, Qq)
DEF_GEN_INSN2_HELPER_EPP(pabsw, pabsw_xmm, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vpabsw, pabsw_xmm, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vpabsw, pabsw_xmm, Vqq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(pabsd, pabsd_mmx, Pq, Qq)
DEF_GEN_INSN2_HELPER_EPP(pabsd, pabsd_xmm, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vpabsd, pabsd_xmm, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vpabsd, pabsd_xmm, Vqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(psignb, psignb_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(psignb, psignb_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsignb, psignb_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsignb, psignb_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(psignw, psignw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(psignw, psignw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsignw, psignw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsignw, psignw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(psignd, psignd_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(psignd, psignd_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsignd, psignd_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsignd, psignd_xmm, Vqq, Hqq, Wqq)

DEF_GEN_INSN4_HELPER_EPPI(dpps, dpps_xmm, Vdq, Vdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vdpps, dpps_xmm, Vdq, Hdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vdpps, dpps_xmm, Vqq, Hqq, Wqq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(dppd, dppd_xmm, Vdq, Vdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vdppd, dppd_xmm, Vdq, Hdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(roundps, roundps_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(vroundps, roundps_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(vroundps, roundps_xmm, Vqq, Wqq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(roundpd, roundpd_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(vroundpd, roundpd_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(vroundpd, roundpd_xmm, Vqq, Wqq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(roundss, roundss_xmm, Vd, Wd, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vroundss, roundss_xmm, Vd, Hd, Wd, Ib)
DEF_GEN_INSN3_HELPER_EPPI(roundsd, roundsd_xmm, Vq, Wq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vroundsd, roundsd_xmm, Vq, Hq, Wq, Ib)

DEF_GEN_INSN3_HELPER_EPP(aesdec, aesdec_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vaesdec, aesdec_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(aesdeclast, aesdeclast_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vaesdeclast, aesdeclast_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(aesenc, aesenc_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vaesenc, aesenc_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(aesenclast, aesenclast_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vaesenclast, aesenclast_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(aesimc, aesimc_xmm, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vaesimc, aesimc_xmm, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPPI(aeskeygenassist, aeskeygenassist_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(vaeskeygenassist, aeskeygenassist_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(pclmulqdq, pclmulqdq_xmm, Vdq, Vdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vpclmulqdq, pclmulqdq_xmm, Vdq, Hdq, Wdq, Ib)

DEF_GEN_INSN3_GVEC(pcmpeqb, Pq, Pq, Qq, cmp, MM_OPRSZ, MM_MAXSZ, MO_8, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(pcmpeqb, Vdq, Vdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_8, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(vpcmpeqb, Vdq, Hdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_8, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(vpcmpeqb, Vqq, Hqq, Wqq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_8, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(pcmpeqw, Pq, Pq, Qq, cmp, MM_OPRSZ, MM_MAXSZ, MO_16, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(pcmpeqw, Vdq, Vdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_16, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(vpcmpeqw, Vdq, Hdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_16, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(vpcmpeqw, Vqq, Hqq, Wqq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_16, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(pcmpeqd, Pq, Pq, Qq, cmp, MM_OPRSZ, MM_MAXSZ, MO_32, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(pcmpeqd, Vdq, Vdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_32, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(vpcmpeqd, Vdq, Hdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_32, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(vpcmpeqd, Vqq, Hqq, Wqq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_32, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(pcmpeqq, Vdq, Vdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_64, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(vpcmpeqq, Vdq, Hdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_64, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(vpcmpeqq, Vqq, Hqq, Wqq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_64, TCG_COND_EQ)
DEF_GEN_INSN3_GVEC(pcmpgtb, Pq, Pq, Qq, cmp, MM_OPRSZ, MM_MAXSZ, MO_8, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(pcmpgtb, Vdq, Vdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_8, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(vpcmpgtb, Vdq, Hdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_8, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(vpcmpgtb, Vqq, Hqq, Wqq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_8, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(pcmpgtw, Pq, Pq, Qq, cmp, MM_OPRSZ, MM_MAXSZ, MO_16, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(pcmpgtw, Vdq, Vdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_16, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(vpcmpgtw, Vdq, Hdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_16, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(vpcmpgtw, Vqq, Hqq, Wqq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_16, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(pcmpgtd, Pq, Pq, Qq, cmp, MM_OPRSZ, MM_MAXSZ, MO_32, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(pcmpgtd, Vdq, Vdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_32, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(vpcmpgtd, Vdq, Hdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_32, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(vpcmpgtd, Vqq, Hqq, Wqq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_32, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(pcmpgtq, Vdq, Vdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_64, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(vpcmpgtq, Vdq, Hdq, Wdq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_64, TCG_COND_GT)
DEF_GEN_INSN3_GVEC(vpcmpgtq, Vqq, Hqq, Wqq, cmp, XMM_OPRSZ, XMM_MAXSZ, MO_64, TCG_COND_GT)

DEF_GEN_INSN3_HELPER_EPPI(pcmpestrm, pcmpestrm_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(vpcmpestrm, pcmpestrm_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(pcmpestri, pcmpestri_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(vpcmpestri, pcmpestri_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(pcmpistrm, pcmpistrm_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(vpcmpistrm, pcmpistrm_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(pcmpistri, pcmpistri_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_EPPI(vpcmpistri, pcmpistri_xmm, Vdq, Wdq, Ib)

DEF_GEN_INSN2_HELPER_EPP(ptest, ptest_xmm, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vptest, ptest_xmm, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vptest, ptest_xmm, Vqq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(vtestps, ptest_xmm, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vtestps, ptest_xmm, Vqq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(vtestpd, ptest_xmm, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vtestpd, ptest_xmm, Vqq, Wqq)

DEF_GEN_INSN3_HELPER_EPP(cmpeqps, cmpeqps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpeqps, cmpeqps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpeqps, cmpeqps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpeqpd, cmpeqpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpeqpd, cmpeqpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpeqpd, cmpeqpd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpeqss, cmpeqss, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vcmpeqss, cmpeqss, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(cmpeqsd, cmpeqsd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vcmpeqsd, cmpeqsd, Vq, Hq, Wq)
DEF_GEN_INSN3_HELPER_EPP(cmpltps, cmpltps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpltps, cmpltps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpltps, cmpltps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpltpd, cmpltpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpltpd, cmpltpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpltpd, cmpltpd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpltss, cmpltss, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vcmpltss, cmpltss, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(cmpltsd, cmpltsd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vcmpltsd, cmpltsd, Vq, Hq, Wq)
DEF_GEN_INSN3_HELPER_EPP(cmpleps, cmpleps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpleps, cmpleps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpleps, cmpleps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmplepd, cmplepd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmplepd, cmplepd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmplepd, cmplepd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpless, cmpless, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vcmpless, cmpless, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(cmplesd, cmplesd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vcmplesd, cmplesd, Vq, Hq, Wq)
DEF_GEN_INSN3_HELPER_EPP(cmpunordps, cmpunordps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpunordps, cmpunordps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpunordps, cmpunordps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpunordpd, cmpunordpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpunordpd, cmpunordpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpunordpd, cmpunordpd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpunordss, cmpunordss, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vcmpunordss, cmpunordss, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(cmpunordsd, cmpunordsd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vcmpunordsd, cmpunordsd, Vq, Hq, Wq)
DEF_GEN_INSN3_HELPER_EPP(cmpneqps, cmpneqps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpneqps, cmpneqps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpneqps, cmpneqps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpneqpd, cmpneqpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpneqpd, cmpneqpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpneqpd, cmpneqpd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpneqss, cmpneqss, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vcmpneqss, cmpneqss, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(cmpneqsd, cmpneqsd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vcmpneqsd, cmpneqsd, Vq, Hq, Wq)
DEF_GEN_INSN3_HELPER_EPP(cmpnltps, cmpnltps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpnltps, cmpnltps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpnltps, cmpnltps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpnltpd, cmpnltpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpnltpd, cmpnltpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpnltpd, cmpnltpd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpnltss, cmpnltss, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vcmpnltss, cmpnltss, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(cmpnltsd, cmpnltsd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vcmpnltsd, cmpnltsd, Vq, Hq, Wq)
DEF_GEN_INSN3_HELPER_EPP(cmpnleps, cmpnleps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpnleps, cmpnleps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpnleps, cmpnleps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpnlepd, cmpnlepd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpnlepd, cmpnlepd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpnlepd, cmpnlepd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpnless, cmpnless, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vcmpnless, cmpnless, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(cmpnlesd, cmpnlesd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vcmpnlesd, cmpnlesd, Vq, Hq, Wq)
DEF_GEN_INSN3_HELPER_EPP(cmpordps, cmpordps, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpordps, cmpordps, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpordps, cmpordps, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpordpd, cmpordpd, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpordpd, cmpordpd, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vcmpordpd, cmpordpd, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(cmpordss, cmpordss, Vd, Vd, Wd)
DEF_GEN_INSN3_HELPER_EPP(vcmpordss, cmpordss, Vd, Hd, Wd)
DEF_GEN_INSN3_HELPER_EPP(cmpordsd, cmpordsd, Vq, Vq, Wq)
DEF_GEN_INSN3_HELPER_EPP(vcmpordsd, cmpordsd, Vq, Hq, Wq)

GEN_INSN4(cmpps, Vdq, Vdq, Wdq, Ib)
{
    switch (arg4 & 7) {
    case 0:
        gen_insn3(cmpeqps, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 1:
        gen_insn3(cmpltps, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 2:
        gen_insn3(cmpleps, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 3:
        gen_insn3(cmpunordps, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 4:
        gen_insn3(cmpneqps, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 5:
        gen_insn3(cmpnltps, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 6:
        gen_insn3(cmpnleps, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 7:
        gen_insn3(cmpordps, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    }

    g_assert_not_reached();
}

GEN_INSN4(vcmpps, Vdq, Hdq, Wdq, Ib)
{
    switch (arg4 & 7) {
    case 0:
        gen_insn3(vcmpeqps, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 1:
        gen_insn3(vcmpltps, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 2:
        gen_insn3(vcmpleps, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 3:
        gen_insn3(vcmpunordps, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 4:
        gen_insn3(vcmpneqps, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 5:
        gen_insn3(vcmpnltps, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 6:
        gen_insn3(vcmpnleps, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 7:
        gen_insn3(vcmpordps, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    }

    g_assert_not_reached();
}

GEN_INSN4(vcmpps, Vqq, Hqq, Wqq, Ib)
{
    switch (arg4 & 7) {
    case 0:
        gen_insn3(vcmpeqps, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 1:
        gen_insn3(vcmpltps, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 2:
        gen_insn3(vcmpleps, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 3:
        gen_insn3(vcmpunordps, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 4:
        gen_insn3(vcmpneqps, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 5:
        gen_insn3(vcmpnltps, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 6:
        gen_insn3(vcmpnleps, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 7:
        gen_insn3(vcmpordps, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    }

    g_assert_not_reached();
}

GEN_INSN4(cmppd, Vdq, Vdq, Wdq, Ib)
{
    switch (arg4 & 7) {
    case 0:
        gen_insn3(cmpeqpd, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 1:
        gen_insn3(cmpltpd, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 2:
        gen_insn3(cmplepd, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 3:
        gen_insn3(cmpunordpd, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 4:
        gen_insn3(cmpneqpd, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 5:
        gen_insn3(cmpnltpd, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 6:
        gen_insn3(cmpnlepd, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 7:
        gen_insn3(cmpordpd, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    }

    g_assert_not_reached();
}

GEN_INSN4(vcmppd, Vdq, Hdq, Wdq, Ib)
{
    switch (arg4 & 7) {
    case 0:
        gen_insn3(vcmpeqpd, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 1:
        gen_insn3(vcmpltpd, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 2:
        gen_insn3(vcmplepd, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 3:
        gen_insn3(vcmpunordpd, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 4:
        gen_insn3(vcmpneqpd, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 5:
        gen_insn3(vcmpnltpd, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 6:
        gen_insn3(vcmpnlepd, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    case 7:
        gen_insn3(vcmpordpd, Vdq, Hdq, Wdq)(env, s, arg1, arg2, arg3);
        return;
    }

    g_assert_not_reached();
}

GEN_INSN4(vcmppd, Vqq, Hqq, Wqq, Ib)
{
    switch (arg4 & 7) {
    case 0:
        gen_insn3(vcmpeqpd, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 1:
        gen_insn3(vcmpltpd, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 2:
        gen_insn3(vcmplepd, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 3:
        gen_insn3(vcmpunordpd, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 4:
        gen_insn3(vcmpneqpd, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 5:
        gen_insn3(vcmpnltpd, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 6:
        gen_insn3(vcmpnlepd, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    case 7:
        gen_insn3(vcmpordpd, Vqq, Hqq, Wqq)(env, s, arg1, arg2, arg3);
        return;
    }

    g_assert_not_reached();
}

GEN_INSN4(cmpss, Vd, Vd, Wd, Ib)
{
    switch (arg4 & 7) {
    case 0:
        gen_insn3(cmpeqss, Vd, Vd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 1:
        gen_insn3(cmpltss, Vd, Vd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 2:
        gen_insn3(cmpless, Vd, Vd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 3:
        gen_insn3(cmpunordss, Vd, Vd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 4:
        gen_insn3(cmpneqss, Vd, Vd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 5:
        gen_insn3(cmpnltss, Vd, Vd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 6:
        gen_insn3(cmpnless, Vd, Vd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 7:
        gen_insn3(cmpordss, Vd, Vd, Wd)(env, s, arg1, arg2, arg3);
        return;
    }

    g_assert_not_reached();
}

GEN_INSN4(vcmpss, Vd, Hd, Wd, Ib)
{
    switch (arg4 & 7) {
    case 0:
        gen_insn3(vcmpeqss, Vd, Hd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 1:
        gen_insn3(vcmpltss, Vd, Hd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 2:
        gen_insn3(vcmpless, Vd, Hd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 3:
        gen_insn3(vcmpunordss, Vd, Hd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 4:
        gen_insn3(vcmpneqss, Vd, Hd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 5:
        gen_insn3(vcmpnltss, Vd, Hd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 6:
        gen_insn3(vcmpnless, Vd, Hd, Wd)(env, s, arg1, arg2, arg3);
        return;
    case 7:
        gen_insn3(vcmpordss, Vd, Hd, Wd)(env, s, arg1, arg2, arg3);
        return;
    }

    g_assert_not_reached();
}

GEN_INSN4(cmpsd, Vq, Vq, Wq, Ib)
{
    switch (arg4 & 7) {
    case 0:
        gen_insn3(cmpeqsd, Vq, Vq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 1:
        gen_insn3(cmpltsd, Vq, Vq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 2:
        gen_insn3(cmplesd, Vq, Vq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 3:
        gen_insn3(cmpunordsd, Vq, Vq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 4:
        gen_insn3(cmpneqsd, Vq, Vq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 5:
        gen_insn3(cmpnltsd, Vq, Vq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 6:
        gen_insn3(cmpnlesd, Vq, Vq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 7:
        gen_insn3(cmpordsd, Vq, Vq, Wq)(env, s, arg1, arg2, arg3);
        return;
    }

    g_assert_not_reached();
}

GEN_INSN4(vcmpsd, Vq, Hq, Wq, Ib)
{
    switch (arg4 & 7) {
    case 0:
        gen_insn3(vcmpeqsd, Vq, Hq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 1:
        gen_insn3(vcmpltsd, Vq, Hq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 2:
        gen_insn3(vcmplesd, Vq, Hq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 3:
        gen_insn3(vcmpunordsd, Vq, Hq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 4:
        gen_insn3(vcmpneqsd, Vq, Hq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 5:
        gen_insn3(vcmpnltsd, Vq, Hq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 6:
        gen_insn3(vcmpnlesd, Vq, Hq, Wq)(env, s, arg1, arg2, arg3);
        return;
    case 7:
        gen_insn3(vcmpordsd, Vq, Hq, Wq)(env, s, arg1, arg2, arg3);
        return;
    }

    g_assert_not_reached();
}

DEF_GEN_INSN2_HELPER_EPP(comiss, comiss, Vd, Wd)
DEF_GEN_INSN2_HELPER_EPP(vcomiss, comiss, Vd, Wd)
DEF_GEN_INSN2_HELPER_EPP(comisd, comisd, Vq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vcomisd, comisd, Vq, Wq)
DEF_GEN_INSN2_HELPER_EPP(ucomiss, ucomiss, Vd, Wd)
DEF_GEN_INSN2_HELPER_EPP(vucomiss, ucomiss, Vd, Wd)
DEF_GEN_INSN2_HELPER_EPP(ucomisd, ucomisd, Vq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vucomisd, ucomisd, Vq, Wq)

DEF_GEN_INSN3_GVEC(pand, Pq, Pq, Qq, and, MM_OPRSZ, MM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(pand, Vdq, Vdq, Wdq, and, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vpand, Vdq, Hdq, Wdq, and, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vpand, Vqq, Hqq, Wqq, and, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(andps, Vdq, Vdq, Wdq, and, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vandps, Vdq, Hdq, Wdq, and, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vandps, Vqq, Hqq, Wqq, and, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(andpd, Vdq, Vdq, Wdq, and, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vandpd, Vdq, Hdq, Wdq, and, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vandpd, Vqq, Hqq, Wqq, and, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(pandn, Pq, Pq, Qq, andn, MM_OPRSZ, MM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(pandn, Vdq, Vdq, Wdq, andn, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vpandn, Vdq, Hdq, Wdq, andn, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vpandn, Vqq, Hqq, Wqq, andn, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(andnps, Vdq, Vdq, Wdq, andn, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vandnps, Vdq, Hdq, Wdq, andn, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vandnps, Vqq, Hqq, Wqq, andn, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(andnpd, Vdq, Vdq, Wdq, andn, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vandnpd, Vdq, Hdq, Wdq, andn, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vandnpd, Vqq, Hqq, Wqq, andn, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(por, Pq, Pq, Qq, or, MM_OPRSZ, MM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(por, Vdq, Vdq, Wdq, or, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vpor, Vdq, Hdq, Wdq, or, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vpor, Vqq, Hqq, Wqq, or, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(orps, Vdq, Vdq, Wdq, or, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vorps, Vdq, Hdq, Wdq, or, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vorps, Vqq, Hqq, Wqq, or, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(orpd, Vdq, Vdq, Wdq, or, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vorpd, Vdq, Hdq, Wdq, or, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vorpd, Vqq, Hqq, Wqq, or, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(pxor, Pq, Pq, Qq, xor, MM_OPRSZ, MM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(pxor, Vdq, Vdq, Wdq, xor, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vpxor, Vdq, Hdq, Wdq, xor, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vpxor, Vqq, Hqq, Wqq, xor, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(xorps, Vdq, Vdq, Wdq, xor, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vxorps, Vdq, Hdq, Wdq, xor, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vxorps, Vqq, Hqq, Wqq, xor, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(xorpd, Vdq, Vdq, Wdq, xor, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vxorpd, Vdq, Hdq, Wdq, xor, XMM_OPRSZ, XMM_MAXSZ, MO_64)
DEF_GEN_INSN3_GVEC(vxorpd, Vqq, Hqq, Wqq, xor, XMM_OPRSZ, XMM_MAXSZ, MO_64)

DEF_GEN_INSN3_HELPER_EPP(psllw, psllw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(psllw, psllw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsllw, psllw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsllw, psllw_xmm, Vqq, Hqq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(pslld, pslld_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(pslld, pslld_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpslld, pslld_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpslld, pslld_xmm, Vqq, Hqq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(psllq, psllq_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(psllq, psllq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsllq, psllq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsllq, psllq_xmm, Vqq, Hqq, Wdq)

GEN_INSN3(vpsllvd, Vdq, Hdq, Wdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpsllvd, Vqq, Hqq, Wqq)
{
    /* XXX TODO implement this */
}

GEN_INSN3(vpsllvq, Vdq, Hdq, Wdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpsllvq, Vqq, Hqq, Wqq)
{
    /* XXX TODO implement this */
}

DEF_GEN_INSN3_HELPER_EPP(pslldq, pslldq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpslldq, pslldq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpslldq, pslldq_xmm, Vqq, Hqq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(psrlw, psrlw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(psrlw, psrlw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsrlw, psrlw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsrlw, psrlw_xmm, Vqq, Hqq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(psrld, psrld_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(psrld, psrld_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsrld, psrld_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsrld, psrld_xmm, Vqq, Hqq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(psrlq, psrlq_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(psrlq, psrlq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsrlq, psrlq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsrlq, psrlq_xmm, Vqq, Hqq, Wdq)

GEN_INSN3(vpsrlvd, Vdq, Hdq, Wdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpsrlvd, Vqq, Hqq, Wqq)
{
    /* XXX TODO implement this */
}

GEN_INSN3(vpsrlvq, Vdq, Hdq, Wdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpsrlvq, Vqq, Hqq, Wqq)
{
    /* XXX TODO implement this */
}

DEF_GEN_INSN3_HELPER_EPP(psrldq, psrldq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsrldq, psrldq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsrldq, psrldq_xmm, Vqq, Hqq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(psraw, psraw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(psraw, psraw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsraw, psraw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsraw, psraw_xmm, Vqq, Hqq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(psrad, psrad_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(psrad, psrad_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsrad, psrad_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpsrad, psrad_xmm, Vqq, Hqq, Wdq)

GEN_INSN3(vpsravd, Vdq, Hdq, Wdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpsravd, Vqq, Hqq, Wqq)
{
    /* XXX TODO implement this */
}

#define DEF_GEN_PSHIFT_IMM_MM(mnem, opT1, opT2)                         \
    GEN_INSN3(mnem, opT1, opT2, Ib)                                     \
    {                                                                   \
        const uint64_t arg3_ui64 = (uint8_t)arg3;                       \
        const insnop_arg_t(Eq) arg3_r64 = s->tmp1_i64;                  \
        const insnop_arg_t(Qq) arg3_mm =                                \
            offsetof(CPUX86State, mmx_t0.MMX_Q(0));                     \
                                                                        \
        tcg_gen_movi_i64(arg3_r64, arg3_ui64);                          \
        gen_insn2(movq, Pq, Eq)(env, s, arg3_mm, arg3_r64);             \
        gen_insn3(mnem, Pq, Pq, Qq)(env, s, arg1, arg2, arg3_mm);       \
    }
#define DEF_GEN_PSHIFT_IMM_XMM(mnem, opT1, opT2)                        \
    GEN_INSN3(mnem, opT1, opT2, Ib)                                     \
    {                                                                   \
        const uint64_t arg3_ui64 = (uint8_t)arg3;                       \
        const insnop_arg_t(Eq) arg3_r64 = s->tmp1_i64;                  \
        const insnop_arg_t(Wdq) arg3_xmm =                              \
            offsetof(CPUX86State, xmm_t0.ZMM_Q(0));                     \
                                                                        \
        tcg_gen_movi_i64(arg3_r64, arg3_ui64);                          \
        gen_insn2(movq, Vdq, Eq)(env, s, arg3_xmm, arg3_r64);           \
        gen_insn3(mnem, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3_xmm);   \
    }
#define DEF_GEN_VPSHIFT_IMM_XMM(mnem, opT1, opT2)                       \
    GEN_INSN3(mnem, opT1, opT2, Ib)                                     \
    {                                                                   \
        const uint64_t arg3_ui64 = (uint8_t)arg3;                       \
        const insnop_arg_t(Eq) arg3_r64 = s->tmp1_i64;                  \
        const insnop_arg_t(Wdq) arg3_xmm =                              \
            offsetof(CPUX86State, xmm_t0.ZMM_Q(0));                     \
                                                                        \
        tcg_gen_movi_i64(arg3_r64, arg3_ui64);                          \
        gen_insn2(movq, Vdq, Eq)(env, s, arg3_xmm, arg3_r64);           \
        gen_insn3(mnem, Vdq, Hdq, Wdq)(env, s, arg2, arg2, arg3_xmm);   \
    }
#define DEF_GEN_VPSHIFT_IMM_YMM(mnem, opT1, opT2)                       \
    GEN_INSN3(mnem, opT1, opT2, Ib)                                     \
    {                                                                   \
        const uint64_t arg3_ui64 = (uint8_t)arg3;                       \
        const insnop_arg_t(Eq) arg3_r64 = s->tmp1_i64;                  \
        const insnop_arg_t(Wdq) arg3_xmm =                              \
            offsetof(CPUX86State, xmm_t0.ZMM_Q(0));                     \
                                                                        \
        tcg_gen_movi_i64(arg3_r64, arg3_ui64);                          \
        gen_insn2(movq, Vdq, Eq)(env, s, arg3_xmm, arg3_r64);           \
        gen_insn3(mnem, Vqq, Hqq, Wdq)(env, s, arg2, arg2, arg3_xmm);   \
    }

DEF_GEN_PSHIFT_IMM_MM(psllw, Nq, Nq)
DEF_GEN_PSHIFT_IMM_XMM(psllw, Udq, Udq)
DEF_GEN_VPSHIFT_IMM_XMM(vpsllw, Hdq, Udq)
DEF_GEN_VPSHIFT_IMM_YMM(vpsllw, Hqq, Uqq)
DEF_GEN_PSHIFT_IMM_MM(pslld, Nq, Nq)
DEF_GEN_PSHIFT_IMM_XMM(pslld, Udq, Udq)
DEF_GEN_VPSHIFT_IMM_XMM(vpslld, Hdq, Udq)
DEF_GEN_VPSHIFT_IMM_YMM(vpslld, Hqq, Uqq)
DEF_GEN_PSHIFT_IMM_MM(psllq, Nq, Nq)
DEF_GEN_PSHIFT_IMM_XMM(psllq, Udq, Udq)
DEF_GEN_VPSHIFT_IMM_XMM(vpsllq, Hdq, Udq)
DEF_GEN_VPSHIFT_IMM_YMM(vpsllq, Hqq, Uqq)
DEF_GEN_PSHIFT_IMM_XMM(pslldq, Udq, Udq)
DEF_GEN_VPSHIFT_IMM_XMM(vpslldq, Hdq, Udq)
DEF_GEN_VPSHIFT_IMM_YMM(vpslldq, Hqq, Uqq)
DEF_GEN_PSHIFT_IMM_MM(psrlw, Nq, Nq)
DEF_GEN_PSHIFT_IMM_XMM(psrlw, Udq, Udq)
DEF_GEN_VPSHIFT_IMM_XMM(vpsrlw, Hdq, Udq)
DEF_GEN_VPSHIFT_IMM_YMM(vpsrlw, Hqq, Uqq)
DEF_GEN_PSHIFT_IMM_MM(psrld, Nq, Nq)
DEF_GEN_PSHIFT_IMM_XMM(psrld, Udq, Udq)
DEF_GEN_VPSHIFT_IMM_XMM(vpsrld, Hdq, Udq)
DEF_GEN_VPSHIFT_IMM_YMM(vpsrld, Hqq, Uqq)
DEF_GEN_PSHIFT_IMM_MM(psrlq, Nq, Nq)
DEF_GEN_PSHIFT_IMM_XMM(psrlq, Udq, Udq)
DEF_GEN_VPSHIFT_IMM_XMM(vpsrlq, Hdq, Udq)
DEF_GEN_VPSHIFT_IMM_YMM(vpsrlq, Hqq, Uqq)
DEF_GEN_PSHIFT_IMM_XMM(psrldq, Udq, Udq)
DEF_GEN_VPSHIFT_IMM_XMM(vpsrldq, Hdq, Udq)
DEF_GEN_VPSHIFT_IMM_YMM(vpsrldq, Hqq, Uqq)
DEF_GEN_PSHIFT_IMM_MM(psraw, Nq, Nq)
DEF_GEN_PSHIFT_IMM_XMM(psraw, Udq, Udq)
DEF_GEN_VPSHIFT_IMM_XMM(vpsraw, Hdq, Udq)
DEF_GEN_VPSHIFT_IMM_XMM(vpsraw, Hqq, Uqq)
DEF_GEN_PSHIFT_IMM_MM(psrad, Nq, Nq)
DEF_GEN_PSHIFT_IMM_XMM(psrad, Udq, Udq)
DEF_GEN_VPSHIFT_IMM_XMM(vpsrad, Hdq, Udq)
DEF_GEN_VPSHIFT_IMM_XMM(vpsrad, Hqq, Uqq)

DEF_GEN_INSN4_HELPER_EPPI(palignr, palignr_mmx, Pq, Pq, Qq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(palignr, palignr_xmm, Vdq, Vdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vpalignr, palignr_xmm, Vdq, Hdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vpalignr, palignr_xmm, Vqq, Hqq, Wqq, Ib)

DEF_GEN_INSN3_HELPER_EPP(packsswb, packsswb_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(packsswb, packsswb_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpacksswb, packsswb_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpacksswb, packsswb_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(packssdw, packssdw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(packssdw, packssdw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpackssdw, packssdw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpackssdw, packssdw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(packuswb, packuswb_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(packuswb, packuswb_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpackuswb, packuswb_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpackuswb, packuswb_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(packusdw, packusdw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpackusdw, packusdw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpackusdw, packusdw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(punpcklbw, punpcklbw_mmx, Pq, Pq, Qd)
DEF_GEN_INSN3_HELPER_EPP(punpcklbw, punpcklbw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpcklbw, punpcklbw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpcklbw, punpcklbw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(punpcklwd, punpcklwd_mmx, Pq, Pq, Qd)
DEF_GEN_INSN3_HELPER_EPP(punpcklwd, punpcklwd_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpcklwd, punpcklwd_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpcklwd, punpcklwd_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(punpckldq, punpckldq_mmx, Pq, Pq, Qd)
DEF_GEN_INSN3_HELPER_EPP(punpckldq, punpckldq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpckldq, punpckldq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpckldq, punpckldq_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(punpcklqdq, punpcklqdq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpcklqdq, punpcklqdq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpcklqdq, punpcklqdq_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(punpckhbw, punpckhbw_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(punpckhbw, punpckhbw_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpckhbw, punpckhbw_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpckhbw, punpckhbw_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(punpckhwd, punpckhwd_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(punpckhwd, punpckhwd_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpckhwd, punpckhwd_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpckhwd, punpckhwd_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(punpckhdq, punpckhdq_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(punpckhdq, punpckhdq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpckhdq, punpckhdq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpckhdq, punpckhdq_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(punpckhqdq, punpckhqdq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpckhqdq, punpckhqdq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpunpckhqdq, punpckhqdq_xmm, Vqq, Hqq, Wqq)

DEF_GEN_INSN3_HELPER_EPP(unpcklps, punpckldq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vunpcklps, punpckldq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vunpcklps, punpckldq_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(unpcklpd, punpcklqdq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vunpcklpd, punpcklqdq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vunpcklpd, punpcklqdq_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(unpckhps, punpckhdq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vunpckhps, punpckhdq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vunpckhps, punpckhdq_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_EPP(unpckhpd, punpckhqdq_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vunpckhpd, punpckhqdq_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vunpckhpd, punpckhqdq_xmm, Vqq, Hqq, Wqq)

DEF_GEN_INSN3_HELPER_EPP(pshufb, pshufb_mmx, Pq, Pq, Qq)
DEF_GEN_INSN3_HELPER_EPP(pshufb, pshufb_xmm, Vdq, Vdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpshufb, pshufb_xmm, Vdq, Hdq, Wdq)
DEF_GEN_INSN3_HELPER_EPP(vpshufb, pshufb_xmm, Vqq, Hqq, Wqq)
DEF_GEN_INSN3_HELPER_PPI(pshufw, pshufw_mmx, Pq, Qq, Ib)
DEF_GEN_INSN3_HELPER_PPI(pshuflw, pshuflw_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_PPI(vpshuflw, pshuflw_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_PPI(vpshuflw, pshuflw_xmm, Vqq, Wqq, Ib)
DEF_GEN_INSN3_HELPER_PPI(pshufhw, pshufhw_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_PPI(vpshufhw, pshufhw_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_PPI(vpshufhw, pshufhw_xmm, Vqq, Wqq, Ib)
DEF_GEN_INSN3_HELPER_PPI(pshufd, pshufd_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_PPI(vpshufd, pshufd_xmm, Vdq, Wdq, Ib)
DEF_GEN_INSN3_HELPER_PPI(vpshufd, pshufd_xmm, Vqq, Wqq, Ib)
DEF_GEN_INSN4_HELPER_PPI(shufps, shufps, Vdq, Vdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_PPI(vshufps, shufps, Vdq, Hdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_PPI(vshufps, shufps, Vqq, Hqq, Wqq, Ib)
DEF_GEN_INSN4_HELPER_PPI(shufpd, shufpd, Vdq, Vdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_PPI(vshufpd, shufpd, Vdq, Hdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_PPI(vshufpd, shufpd, Vqq, Hqq, Wqq, Ib)

DEF_GEN_INSN4_HELPER_EPPI(blendps, blendps_xmm, Vdq, Vdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vblendps, blendps_xmm, Vdq, Hdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vblendps, blendps_xmm, Vqq, Hqq, Wqq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(blendpd, blendpd_xmm, Vdq, Vdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vblendpd, blendpd_xmm, Vdq, Hdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vblendpd, blendpd_xmm, Vqq, Hqq, Wqq, Ib)

DEF_GEN_INSN3_HELPER_EPP(blendvps, blendvps_xmm, Vdq, Vdq, Wdq)
GEN_INSN4(vblendvps, Vdq, Hdq, Wdq, Ldq)
{
    gen_insn3(blendvps, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
}
GEN_INSN4(vblendvps, Vqq, Hqq, Wqq, Lqq)
{
    gen_insn3(blendvps, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
}

DEF_GEN_INSN3_HELPER_EPP(blendvpd, blendvpd_xmm, Vdq, Vdq, Wdq)
GEN_INSN4(vblendvpd, Vdq, Hdq, Wdq, Ldq)
{
    gen_insn3(blendvpd, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
}
GEN_INSN4(vblendvpd, Vqq, Hqq, Wqq, Lqq)
{
    gen_insn3(blendvpd, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
}

DEF_GEN_INSN3_HELPER_EPP(pblendvb, pblendvb_xmm, Vdq, Vdq, Wdq)
GEN_INSN4(vpblendvb, Vdq, Hdq, Wdq, Ldq)
{
    gen_insn3(pblendvb, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
}
GEN_INSN4(vpblendvb, Vqq, Hqq, Wqq, Lqq)
{
    gen_insn3(pblendvb, Vdq, Vdq, Wdq)(env, s, arg1, arg2, arg3);
}

DEF_GEN_INSN4_HELPER_EPPI(pblendw, pblendw_xmm, Vdq, Vdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vpblendw, pblendw_xmm, Vdq, Hdq, Wdq, Ib)
DEF_GEN_INSN4_HELPER_EPPI(vpblendw, pblendw_xmm, Vqq, Hqq, Wqq, Ib)

GEN_INSN4(vpblendd, Vdq, Hdq, Wdq, Ib)
{
    /* XXX TODO implement this */
}
GEN_INSN4(vpblendd, Vqq, Hqq, Wqq, Ib)
{
    /* XXX TODO implement this */
}

GEN_INSN4(insertps, Vdq, Vdq, Wd, Ib)
{
    assert(arg1 == arg2);

    const size_t dofs = offsetof(ZMMReg, ZMM_L(arg4 & 3));
    const size_t aofs = offsetof(ZMMReg, ZMM_L(0));
    gen_op_movl(s, arg1 + dofs, arg3 + aofs);
}
GEN_INSN4(vinsertps, Vdq, Hdq, Wd, Ib)
{
    if (arg1 != arg2) {
        gen_insn2(vmovaps, Vdq, Wdq)(env, s, arg1, arg2);
    }
    gen_insn4(insertps, Vdq, Vdq, Wd, Ib)(env, s, arg1, arg1, arg3, arg4);
}
GEN_INSN4(pinsrb, Vdq, Vdq, RdMb, Ib)
{
    assert(arg1 == arg2);

    const size_t ofs = offsetof(ZMMReg, ZMM_B(arg4 & 15));
    tcg_gen_st8_i32(arg3, cpu_env, arg1 + ofs);
}
GEN_INSN4(vpinsrb, Vdq, Hdq, RdMb, Ib)
{
    if (arg1 != arg2) {
        gen_insn2(vmovaps, Vdq, Wdq)(env, s, arg1, arg2);
    }
    gen_insn4(pinsrb, Vdq, Vdq, RdMb, Ib)(env, s, arg1, arg1, arg3, arg4);
}
GEN_INSN4(pinsrw, Pq, Pq, RdMw, Ib)
{
    assert(arg1 == arg2);

    const size_t ofs = offsetof(MMXReg, MMX_W(arg4 & 3));
    tcg_gen_st16_i32(arg3, cpu_env, arg1 + ofs);
}
GEN_INSN4(pinsrw, Vdq, Vdq, RdMw, Ib)
{
    assert(arg1 == arg2);

    const size_t ofs = offsetof(ZMMReg, ZMM_W(arg4 & 7));
    tcg_gen_st16_i32(arg3, cpu_env, arg1 + ofs);
}
GEN_INSN4(vpinsrw, Vdq, Hdq, RdMw, Ib)
{
    if (arg1 != arg2) {
        gen_insn2(vmovaps, Vdq, Wdq)(env, s, arg1, arg2);
    }
    gen_insn4(pinsrw, Vdq, Vdq, RdMw, Ib)(env, s, arg1, arg1, arg3, arg4);
}
GEN_INSN4(pinsrd, Vdq, Vdq, Ed, Ib)
{
    assert(arg1 == arg2);

    const size_t ofs = offsetof(ZMMReg, ZMM_L(arg4 & 3));
    tcg_gen_st_i32(arg3, cpu_env, arg1 + ofs);
}
GEN_INSN4(vpinsrd, Vdq, Hdq, Ed, Ib)
{
    if (arg1 != arg2) {
        gen_insn2(vmovaps, Vdq, Wdq)(env, s, arg1, arg2);
    }
    gen_insn4(pinsrd, Vdq, Vdq, Ed, Ib)(env, s, arg1, arg1, arg3, arg4);
}
GEN_INSN4(pinsrq, Vdq, Vdq, Eq, Ib)
{
    assert(arg1 == arg2);

    const size_t ofs = offsetof(ZMMReg, ZMM_Q(arg4 & 1));
    tcg_gen_st_i64(arg3, cpu_env, arg1 + ofs);
}
GEN_INSN4(vpinsrq, Vdq, Hdq, Eq, Ib)
{
    if (arg1 != arg2) {
        gen_insn2(vmovaps, Vdq, Wdq)(env, s, arg1, arg2);
    }
    gen_insn4(pinsrq, Vdq, Vdq, Eq, Ib)(env, s, arg1, arg1, arg3, arg4);
}
GEN_INSN4(vinsertf128, Vqq, Hqq, Wdq, Ib)
{
    gen_insn2(movaps, Vdq, Wdq)(env, s, arg1, arg3);
}
GEN_INSN4(vinserti128, Vqq, Hqq, Wdq, Ib)
{
    gen_insn2(movaps, Vdq, Wdq)(env, s, arg1, arg3);
}

GEN_INSN3(extractps, Ed, Vdq, Ib)
{
    const size_t ofs = offsetof(ZMMReg, ZMM_L(arg3 & 3));
    tcg_gen_ld_i32(arg1, cpu_env, arg2 + ofs);
}
GEN_INSN3(vextractps, Ed, Vdq, Ib)
{
    gen_insn3(extractps, Ed, Vdq, Ib)(env, s, arg1, arg2, arg3);
}
GEN_INSN3(pextrb, RdMb, Vdq, Ib)
{
    const size_t ofs = offsetof(ZMMReg, ZMM_B(arg3 & 15));
    tcg_gen_ld8u_i32(arg1, cpu_env, arg2 + ofs);
}
GEN_INSN3(vpextrb, RdMb, Vdq, Ib)
{
    gen_insn3(pextrb, RdMb, Vdq, Ib)(env, s, arg1, arg2, arg3);
}
GEN_INSN3(pextrw, RdMw, Vdq, Ib)
{
    const size_t ofs = offsetof(ZMMReg, ZMM_W(arg3 & 7));
    tcg_gen_ld16u_i32(arg1, cpu_env, arg2 + ofs);
}
GEN_INSN3(vpextrw, RdMw, Vdq, Ib)
{
    gen_insn3(pextrw, RdMw, Vdq, Ib)(env, s, arg1, arg2, arg3);
}
GEN_INSN3(pextrd, Ed, Vdq, Ib)
{
    const size_t ofs = offsetof(ZMMReg, ZMM_L(arg3 & 3));
    tcg_gen_ld_i32(arg1, cpu_env, arg2 + ofs);
}
GEN_INSN3(vpextrd, Ed, Vdq, Ib)
{
    gen_insn3(pextrd, Ed, Vdq, Ib)(env, s, arg1, arg2, arg3);
}
GEN_INSN3(pextrq, Eq, Vdq, Ib)
{
    const size_t ofs = offsetof(ZMMReg, ZMM_Q(arg3 & 1));
    tcg_gen_ld_i64(arg1, cpu_env, arg2 + ofs);
}
GEN_INSN3(vpextrq, Eq, Vdq, Ib)
{
    gen_insn3(pextrq, Eq, Vdq, Ib)(env, s, arg1, arg2, arg3);
}
GEN_INSN3(pextrw, Gd, Nq, Ib)
{
    const size_t ofs = offsetof(MMXReg, MMX_W(arg3 & 3));
    tcg_gen_ld16u_i32(arg1, cpu_env, arg2 + ofs);
}
GEN_INSN3(pextrw, Gq, Nq, Ib)
{
    const size_t ofs = offsetof(MMXReg, MMX_W(arg3 & 3));
    tcg_gen_ld16u_i64(arg1, cpu_env, arg2 + ofs);
}
GEN_INSN3(pextrw, Gd, Udq, Ib)
{
    const size_t ofs = offsetof(ZMMReg, ZMM_W(arg3 & 7));
    tcg_gen_ld16u_i32(arg1, cpu_env, arg2 + ofs);
}
GEN_INSN3(pextrw, Gq, Udq, Ib)
{
    const size_t ofs = offsetof(ZMMReg, ZMM_W(arg3 & 7));
    tcg_gen_ld16u_i64(arg1, cpu_env, arg2 + ofs);
}
GEN_INSN3(vpextrw, Gd, Udq, Ib)
{
    gen_insn3(pextrw, Gd, Udq, Ib)(env, s, arg1, arg2, arg3);
}
GEN_INSN3(vpextrw, Gq, Udq, Ib)
{
    gen_insn3(pextrw, Gq, Udq, Ib)(env, s, arg1, arg2, arg3);
}
GEN_INSN3(vextractf128, Wdq, Vqq, Ib)
{
    gen_insn2(movaps, Wdq, Vdq)(env, s, arg1, arg2);
}
GEN_INSN3(vextracti128, Wdq, Vqq, Ib)
{
    gen_insn2(movaps, Wdq, Vdq)(env, s, arg1, arg2);
}

GEN_INSN2(vpbroadcastb, Vdq, Wb)
{
    /* XXX TODO implement this */
}
GEN_INSN2(vpbroadcastb, Vqq, Wb)
{
    /* XXX TODO implement this */
}
GEN_INSN2(vpbroadcastw, Vdq, Ww)
{
    /* XXX TODO implement this */
}
GEN_INSN2(vpbroadcastw, Vqq, Ww)
{
    /* XXX TODO implement this */
}
GEN_INSN2(vpbroadcastd, Vdq, Wd)
{
    /* XXX TODO implement this */
}
GEN_INSN2(vpbroadcastd, Vqq, Wd)
{
    /* XXX TODO implement this */
}
GEN_INSN2(vpbroadcastq, Vdq, Wq)
{
    /* XXX TODO implement this */
}
GEN_INSN2(vpbroadcastq, Vqq, Wq)
{
    /* XXX TODO implement this */
}

GEN_INSN3(vbroadcastss, Vdq, Wd, modrm_mod)
{
    if (arg3 != 3 && !check_cpuid(env, s, CHECK_CPUID_AVX2)) {
        gen_unknown_opcode(env, s);
        return;
    }

    const TCGv_i32 r32 = tcg_temp_new_i32();
    tcg_gen_ld_i32(r32, cpu_env, arg2 + offsetof(ZMMReg, ZMM_L(0)));
    if (arg1 != arg2) {
        tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(0)));
    }
    tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(1)));
    tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(2)));
    tcg_gen_st_i32(r32, cpu_env, arg1 + offsetof(ZMMReg, ZMM_L(3)));
    tcg_temp_free_i32(r32);
}
GEN_INSN3(vbroadcastss, Vqq, Wd, modrm_mod)
{
    gen_insn3(vbroadcastss, Vdq, Wd, modrm_mod)(env, s, arg1, arg2, arg3);
}
GEN_INSN3(vbroadcastsd, Vqq, Wq, modrm_mod)
{
    if (arg3 != 3 && !check_cpuid(env, s, CHECK_CPUID_AVX2)) {
        gen_unknown_opcode(env, s);
        return;
    }

    const TCGv_i64 r64 = tcg_temp_new_i64();
    tcg_gen_ld_i64(r64, cpu_env, arg2 + offsetof(ZMMReg, ZMM_Q(0)));
    if (arg1 != arg2) {
        tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(0)));
    }
    tcg_gen_st_i64(r64, cpu_env, arg1 + offsetof(ZMMReg, ZMM_Q(1)));
    tcg_temp_free_i64(r64);
}
GEN_INSN2(vbroadcastf128, Vqq, Mdq)
{
    insnop_ldst(xmm, Mqq)(env, s, 0, arg1, arg2);
}
GEN_INSN2(vbroadcasti128, Vqq, Mdq)
{
    insnop_ldst(xmm, Mqq)(env, s, 0, arg1, arg2);
}

GEN_INSN4(vperm2f128, Vqq, Hqq, Wqq, Ib)
{
    /* XXX TODO implement this */
}
GEN_INSN4(vperm2i128, Vqq, Hqq, Wqq, Ib)
{
    /* XXX TODO implement this */
}

GEN_INSN3(vpermd, Vqq, Hqq, Wqq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpermps, Vqq, Hqq, Wqq)
{
    /* XXX TODO implement this */
}

GEN_INSN3(vpermilps, Vdq, Hdq, Wdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpermilps, Vqq, Hqq, Wqq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpermilps, Vdq, Wdq, Ib)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpermilps, Vqq, Wqq, Ib)
{
    /* XXX TODO implement this */
}

GEN_INSN3(vpermilpd, Vdq, Hdq, Wdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpermilpd, Vqq, Hqq, Wqq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpermilpd, Vdq, Wdq, Ib)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpermilpd, Vqq, Wqq, Ib)
{
    /* XXX TODO implement this */
}

GEN_INSN3(vpermq, Vqq, Wqq, Ib)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpermpd, Vqq, Wqq, Ib)
{
    /* XXX TODO implement this */
}

GEN_INSN5(vgatherdps, Vdq, Hdq, Vdq, MDdq, Hdq)
{
    /* XXX TODO implement this */
}
GEN_INSN5(vgatherdps, Vqq, Hqq, Vqq, MDqq, Hqq)
{
    /* XXX TODO implement this */
}
GEN_INSN5(vgatherdpd, Vdq, Hdq, Vdq, MDdq, Hdq)
{
    /* XXX TODO implement this */
}
GEN_INSN5(vgatherdpd, Vqq, Hqq, Vqq, MDdq, Hqq)
{
    /* XXX TODO implement this */
}

GEN_INSN5(vgatherqps, Vdq, Hdq, Vdq, MQdq, Hdq)
{
    /* XXX TODO implement this */
}
GEN_INSN5(vgatherqps, Vdq, Hdq, Vdq, MQqq, Hdq)
{
    /* XXX TODO implement this */
}
GEN_INSN5(vgatherqpd, Vdq, Hdq, Vdq, MQdq, Hdq)
{
    /* XXX TODO implement this */
}
GEN_INSN5(vgatherqpd, Vqq, Hqq, Vqq, MQqq, Hqq)
{
    /* XXX TODO implement this */
}

GEN_INSN5(vpgatherdd, Vdq, Hdq, Vdq, MDdq, Hdq)
{
    /* XXX TODO implement this */
}
GEN_INSN5(vpgatherdd, Vqq, Hqq, Vqq, MDqq, Hqq)
{
    /* XXX TODO implement this */
}
GEN_INSN5(vpgatherdq, Vdq, Hdq, Vdq, MDdq, Hdq)
{
    /* XXX TODO implement this */
}
GEN_INSN5(vpgatherdq, Vqq, Hqq, Vqq, MDdq, Hqq)
{
    /* XXX TODO implement this */
}

GEN_INSN5(vpgatherqd, Vdq, Hdq, Vdq, MQdq, Hdq)
{
    /* XXX TODO implement this */
}
GEN_INSN5(vpgatherqd, Vdq, Hdq, Vdq, MQqq, Hdq)
{
    /* XXX TODO implement this */
}
GEN_INSN5(vpgatherqq, Vdq, Hdq, Vdq, MQdq, Hdq)
{
    /* XXX TODO implement this */
}
GEN_INSN5(vpgatherqq, Vqq, Hqq, Vqq, MQqq, Hqq)
{
    /* XXX TODO implement this */
}

DEF_GEN_INSN2_HELPER_EPP(pmovsxbw, pmovsxbw_xmm, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vpmovsxbw, pmovsxbw_xmm, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vpmovsxbw, pmovsxbw_xmm, Vqq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(pmovsxbd, pmovsxbd_xmm, Vdq, Wd)
DEF_GEN_INSN2_HELPER_EPP(vpmovsxbd, pmovsxbd_xmm, Vdq, Wd)
DEF_GEN_INSN2_HELPER_EPP(vpmovsxbd, pmovsxbd_xmm, Vqq, Wq)
DEF_GEN_INSN2_HELPER_EPP(pmovsxbq, pmovsxbq_xmm, Vdq, Ww)
DEF_GEN_INSN2_HELPER_EPP(vpmovsxbq, pmovsxbq_xmm, Vdq, Ww)
DEF_GEN_INSN2_HELPER_EPP(vpmovsxbq, pmovsxbq_xmm, Vqq, Wd)
DEF_GEN_INSN2_HELPER_EPP(pmovsxwd, pmovsxwd_xmm, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vpmovsxwd, pmovsxwd_xmm, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vpmovsxwd, pmovsxwd_xmm, Vqq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(pmovsxwq, pmovsxwq_xmm, Vdq, Wd)
DEF_GEN_INSN2_HELPER_EPP(vpmovsxwq, pmovsxwq_xmm, Vdq, Wd)
DEF_GEN_INSN2_HELPER_EPP(vpmovsxwq, pmovsxwq_xmm, Vqq, Wq)
DEF_GEN_INSN2_HELPER_EPP(pmovsxdq, pmovsxdq_xmm, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vpmovsxdq, pmovsxdq_xmm, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vpmovsxdq, pmovsxdq_xmm, Vqq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(pmovzxbw, pmovzxbw_xmm, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vpmovzxbw, pmovzxbw_xmm, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vpmovzxbw, pmovzxbw_xmm, Vqq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(pmovzxbd, pmovzxbd_xmm, Vdq, Wd)
DEF_GEN_INSN2_HELPER_EPP(vpmovzxbd, pmovzxbd_xmm, Vdq, Wd)
DEF_GEN_INSN2_HELPER_EPP(vpmovzxbd, pmovzxbd_xmm, Vqq, Wq)
DEF_GEN_INSN2_HELPER_EPP(pmovzxbq, pmovzxbq_xmm, Vdq, Ww)
DEF_GEN_INSN2_HELPER_EPP(vpmovzxbq, pmovzxbq_xmm, Vdq, Ww)
DEF_GEN_INSN2_HELPER_EPP(vpmovzxbq, pmovzxbq_xmm, Vqq, Wd)
DEF_GEN_INSN2_HELPER_EPP(pmovzxwd, pmovzxwd_xmm, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vpmovzxwd, pmovzxwd_xmm, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vpmovzxwd, pmovzxwd_xmm, Vqq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(pmovzxwq, pmovzxwq_xmm, Vdq, Wd)
DEF_GEN_INSN2_HELPER_EPP(vpmovzxwq, pmovzxwq_xmm, Vdq, Wd)
DEF_GEN_INSN2_HELPER_EPP(vpmovzxwq, pmovzxwq_xmm, Vqq, Wq)
DEF_GEN_INSN2_HELPER_EPP(pmovzxdq, pmovzxdq_xmm, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vpmovzxdq, pmovzxdq_xmm, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vpmovzxdq, pmovzxdq_xmm, Vqq, Wdq)

DEF_GEN_INSN2_HELPER_EPP(cvtpi2ps, cvtpi2ps, Vdq, Qq)
DEF_GEN_INSN2_HELPER_EPD(cvtsi2ss, cvtsi2ss, Vd, Ed)
DEF_GEN_INSN2_HELPER_EPQ(cvtsi2ss, cvtsq2ss, Vd, Eq)
DEF_GEN_INSN3_HELPER_EPD(vcvtsi2ss, cvtsi2ss, Vd, Hd, Ed)
DEF_GEN_INSN3_HELPER_EPQ(vcvtsi2ss, cvtsq2ss, Vd, Hd, Eq)
DEF_GEN_INSN2_HELPER_EPP(cvtpi2pd, cvtpi2pd, Vdq, Qq)
DEF_GEN_INSN2_HELPER_EPD(cvtsi2sd, cvtsi2sd, Vq, Ed)
DEF_GEN_INSN2_HELPER_EPQ(cvtsi2sd, cvtsq2sd, Vq, Eq)
DEF_GEN_INSN3_HELPER_EPD(vcvtsi2sd, cvtsi2sd, Vq, Hq, Ed)
DEF_GEN_INSN3_HELPER_EPQ(vcvtsi2sd, cvtsq2sd, Vq, Hq, Eq)
DEF_GEN_INSN2_HELPER_EPP(cvtps2pi, cvtps2pi, Pq, Wq)
DEF_GEN_INSN2_HELPER_DEP(cvtss2si, cvtss2si, Gd, Wd)
DEF_GEN_INSN2_HELPER_QEP(cvtss2si, cvtss2sq, Gq, Wd)
DEF_GEN_INSN2_HELPER_DEP(vcvtss2si, cvtss2si, Gd, Wd)
DEF_GEN_INSN2_HELPER_QEP(vcvtss2si, cvtss2sq, Gq, Wd)
DEF_GEN_INSN2_HELPER_EPP(cvtpd2pi, cvtpd2pi, Pq, Wdq)
DEF_GEN_INSN2_HELPER_DEP(cvtsd2si, cvtsd2si, Gd, Wq)
DEF_GEN_INSN2_HELPER_QEP(cvtsd2si, cvtsd2sq, Gq, Wq)
DEF_GEN_INSN2_HELPER_DEP(vcvtsd2si, cvtsd2si, Gd, Wq)
DEF_GEN_INSN2_HELPER_QEP(vcvtsd2si, cvtsd2sq, Gq, Wq)
DEF_GEN_INSN2_HELPER_EPP(cvttps2pi, cvttps2pi, Pq, Wq)
DEF_GEN_INSN2_HELPER_DEP(cvttss2si, cvttss2si, Gd, Wd)
DEF_GEN_INSN2_HELPER_QEP(cvttss2si, cvttss2sq, Gq, Wd)
DEF_GEN_INSN2_HELPER_DEP(vcvttss2si, cvttss2si, Gd, Wd)
DEF_GEN_INSN2_HELPER_QEP(vcvttss2si, cvttss2sq, Gq, Wd)
DEF_GEN_INSN2_HELPER_EPP(cvttpd2pi, cvttpd2pi, Pq, Wdq)
DEF_GEN_INSN2_HELPER_DEP(cvttsd2si, cvttsd2si, Gd, Wq)
DEF_GEN_INSN2_HELPER_QEP(cvttsd2si, cvttsd2sq, Gq, Wq)
DEF_GEN_INSN2_HELPER_DEP(vcvttsd2si, cvttsd2si, Gd, Wq)
DEF_GEN_INSN2_HELPER_QEP(vcvttsd2si, cvttsd2sq, Gq, Wq)

DEF_GEN_INSN2_HELPER_EPP(cvtpd2dq, cvtpd2dq, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vcvtpd2dq, cvtpd2dq, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vcvtpd2dq, cvtpd2dq, Vdq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(cvttpd2dq, cvttpd2dq, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vcvttpd2dq, cvttpd2dq, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vcvttpd2dq, cvttpd2dq, Vdq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(cvtdq2pd, cvtdq2pd, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vcvtdq2pd, cvtdq2pd, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vcvtdq2pd, cvtdq2pd, Vqq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(cvtps2pd, cvtps2pd, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vcvtps2pd, cvtps2pd, Vdq, Wq)
DEF_GEN_INSN2_HELPER_EPP(vcvtps2pd, cvtps2pd, Vqq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(cvtpd2ps, cvtpd2ps, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vcvtpd2ps, cvtpd2ps, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vcvtpd2ps, cvtpd2ps, Vdq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(cvtss2sd, cvtss2sd, Vq, Wd)
DEF_GEN_INSN3_HELPER_EPP(vcvtss2sd, cvtss2sd, Vq, Hq, Wd)
DEF_GEN_INSN2_HELPER_EPP(cvtsd2ss, cvtsd2ss, Vd, Wq)
DEF_GEN_INSN3_HELPER_EPP(vcvtsd2ss, cvtsd2ss, Vd, Hd, Wq)
DEF_GEN_INSN2_HELPER_EPP(cvtdq2ps, cvtdq2ps, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vcvtdq2ps, cvtdq2ps, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vcvtdq2ps, cvtdq2ps, Vqq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(cvtps2dq, cvtps2dq, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vcvtps2dq, cvtps2dq, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vcvtps2dq, cvtps2dq, Vqq, Wqq)
DEF_GEN_INSN2_HELPER_EPP(cvttps2dq, cvttps2dq, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vcvttps2dq, cvttps2dq, Vdq, Wdq)
DEF_GEN_INSN2_HELPER_EPP(vcvttps2dq, cvttps2dq, Vqq, Wqq)

GEN_INSN2(maskmovq, Pq, Nq)
{
    const TCGv_ptr arg1_ptr = tcg_temp_new_ptr();
    const TCGv_ptr arg2_ptr = tcg_temp_new_ptr();

    tcg_gen_mov_tl(s->A0, cpu_regs[R_EDI]);
    gen_extu(s->aflag, s->A0);
    gen_add_A0_ds_seg(s);

    tcg_gen_addi_ptr(arg1_ptr, cpu_env, arg1);
    tcg_gen_addi_ptr(arg2_ptr, cpu_env, arg2);
    gen_helper_maskmov_mmx(cpu_env, arg1_ptr, arg2_ptr, s->A0);

    tcg_temp_free_ptr(arg1_ptr);
    tcg_temp_free_ptr(arg2_ptr);
}
GEN_INSN2(maskmovdqu, Vdq, Udq)
{
    const TCGv_ptr arg1_ptr = tcg_temp_new_ptr();
    const TCGv_ptr arg2_ptr = tcg_temp_new_ptr();

    tcg_gen_mov_tl(s->A0, cpu_regs[R_EDI]);
    gen_extu(s->aflag, s->A0);
    gen_add_A0_ds_seg(s);

    tcg_gen_addi_ptr(arg1_ptr, cpu_env, arg1);
    tcg_gen_addi_ptr(arg2_ptr, cpu_env, arg2);
    gen_helper_maskmov_xmm(cpu_env, arg1_ptr, arg2_ptr, s->A0);

    tcg_temp_free_ptr(arg1_ptr);
    tcg_temp_free_ptr(arg2_ptr);
}
GEN_INSN2(vmaskmovdqu, Vdq, Udq)
{
    gen_insn2(maskmovdqu, Vdq, Udq)(env, s, arg1, arg2);
}

GEN_INSN3(vmaskmovps, Vdq, Hdq, Mdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vmaskmovps, Mdq, Hdq, Vdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vmaskmovps, Vqq, Hqq, Mqq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vmaskmovps, Mqq, Hqq, Vqq)
{
    /* XXX TODO implement this */
}

GEN_INSN3(vmaskmovpd, Vdq, Hdq, Mdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vmaskmovpd, Mdq, Hdq, Vdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vmaskmovpd, Vqq, Hqq, Mqq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vmaskmovpd, Mqq, Hqq, Vqq)
{
    /* XXX TODO implement this */
}

GEN_INSN3(vpmaskmovd, Vdq, Hdq, Mdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpmaskmovd, Mdq, Hdq, Vdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpmaskmovd, Vqq, Hqq, Mqq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpmaskmovd, Mqq, Hqq, Vqq)
{
    /* XXX TODO implement this */
}

GEN_INSN3(vpmaskmovq, Vdq, Hdq, Mdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpmaskmovq, Mdq, Hdq, Vdq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpmaskmovq, Vqq, Hqq, Mqq)
{
    /* XXX TODO implement this */
}
GEN_INSN3(vpmaskmovq, Mqq, Hqq, Vqq)
{
    /* XXX TODO implement this */
}

GEN_INSN2(movntps, Mdq, Vdq)
{
    insnop_ldst(xmm, Mdq)(env, s, 1, arg2, arg1);
}
GEN_INSN2(vmovntps, Mdq, Vdq)
{
    gen_insn2(movntps, Mdq, Vdq)(env, s, arg1, arg2);
}
GEN_INSN2(vmovntps, Mqq, Vqq)
{
    gen_insn2(vmovntps, Mdq, Vdq)(env, s, arg1, arg2);
}

GEN_INSN2(movntpd, Mdq, Vdq)
{
    insnop_ldst(xmm, Mdq)(env, s, 1, arg2, arg1);
}
GEN_INSN2(vmovntpd, Mdq, Vdq)
{
    gen_insn2(movntpd, Mdq, Vdq)(env, s, arg1, arg2);
}
GEN_INSN2(vmovntpd, Mqq, Vqq)
{
    gen_insn2(vmovntpd, Mdq, Vdq)(env, s, arg1, arg2);
}

GEN_INSN2(movnti, Md, Gd)
{
    insnop_ldst(tcg_i32, Md)(env, s, 1, arg2, arg1);
}
GEN_INSN2(movnti, Mq, Gq)
{
    insnop_ldst(tcg_i64, Mq)(env, s, 1, arg2, arg1);
}

GEN_INSN2(movntq, Mq, Pq)
{
    insnop_ldst(mm, Mq)(env, s, 1, arg2, arg1);
}
GEN_INSN2(movntdq, Mdq, Vdq)
{
    insnop_ldst(xmm, Mdq)(env, s, 1, arg2, arg1);
}
GEN_INSN2(vmovntdq, Mdq, Vdq)
{
    gen_insn2(movntdq, Mdq, Vdq)(env, s, arg1, arg2);
}
GEN_INSN2(vmovntdq, Mqq, Vqq)
{
    gen_insn2(vmovntdq, Mdq, Vdq)(env, s, arg1, arg2);
}

GEN_INSN2(movntdqa, Vdq, Mdq)
{
    insnop_ldst(xmm, Mdq)(env, s, 0, arg1, arg2);
}
GEN_INSN2(vmovntdqa, Vdq, Mdq)
{
    gen_insn2(movntdqa, Vdq, Mdq)(env, s, arg1, arg2);
}
GEN_INSN2(vmovntdqa, Vqq, Mqq)
{
    gen_insn2(movntdqa, Vdq, Mdq)(env, s, arg1, arg2);
}

GEN_INSN0(pause)
{
    /* handled in disas_insn at the moment */
    g_assert_not_reached();
}

DEF_GEN_INSN0_HELPER(emms, emms)

GEN_INSN0(vzeroupper)
{
    /* XXX TODO implement this */
}

GEN_INSN0(vzeroall)
{
    /* XXX TODO implement this */
}

GEN_INSN2(sfence_clflush, modrm_mod, modrm)
{
    if (arg1 == 3) {
        /* sfence */
        if (s->prefix & PREFIX_LOCK) {
            gen_illegal_opcode(s);
        } else {
            tcg_gen_mb(TCG_MO_ST_ST | TCG_BAR_SC);
        }
    } else {
        /* clflush */
        if (!check_cpuid(env, s, CHECK_CPUID_CLFLUSH)) {
            gen_illegal_opcode(s);
        } else {
            gen_nop_modrm(env, s, arg2);
        }
    }
}
GEN_INSN0(lfence)
{
    if (s->prefix & PREFIX_LOCK) {
        gen_illegal_opcode(s);
    } else {
        tcg_gen_mb(TCG_MO_LD_LD | TCG_BAR_SC);
    }
}
GEN_INSN0(mfence)
{
    if (s->prefix & PREFIX_LOCK) {
        gen_illegal_opcode(s);
    } else {
        tcg_gen_mb(TCG_MO_ALL | TCG_BAR_SC);
    }
}

GEN_INSN1(ldmxcsr, Md)
{
    if ((s->flags & HF_EM_MASK) || !(s->flags & HF_OSFXSR_MASK)) {
        gen_illegal_opcode(s);
    } else if (s->flags & HF_TS_MASK) {
        gen_exception(s, EXCP07_PREX);
    } else {
        tcg_gen_qemu_ld_i32(s->tmp2_i32, arg1, s->mem_index, MO_LEUL);
        gen_helper_ldmxcsr(cpu_env, s->tmp2_i32);
    }
}
GEN_INSN1(vldmxcsr, Md)
{
    gen_insn1(ldmxcsr, Md)(env, s, arg1);
}
GEN_INSN1(stmxcsr, Md)
{
    if ((s->flags & HF_EM_MASK) || !(s->flags & HF_OSFXSR_MASK)) {
        gen_illegal_opcode(s);
    } else if (s->flags & HF_TS_MASK) {
        gen_exception(s, EXCP07_PREX);
    } else {
        const size_t ofs = offsetof(CPUX86State, mxcsr);
        tcg_gen_ld_i32(s->tmp2_i32, cpu_env, ofs);
        tcg_gen_qemu_st_i32(s->tmp2_i32, arg1, s->mem_index, MO_LEUL);
    }
}
GEN_INSN1(vstmxcsr, Md)
{
    gen_insn1(stmxcsr, Md)(env, s, arg1);
}

GEN_INSN1(prefetcht0, Mb)
{
}
GEN_INSN1(prefetcht1, Mb)
{
}
GEN_INSN1(prefetcht2, Mb)
{
}
GEN_INSN1(prefetchnta, Mb)
{
}

/*
 * Instruction translators
 */
#define translate_insn(argc, ...)               \
    glue(translate_insn, argc)(__VA_ARGS__)
#define translate_insn0()                       \
    translate_insn_0
#define translate_insn1(opT1)                   \
    translate_insn_1 ## opT1
#define translate_insn2(opT1, opT2)             \
    translate_insn_2 ## opT1 ## opT2
#define translate_insn3(opT1, opT2, opT3)       \
    translate_insn_3 ## opT1 ## opT2 ## opT3
#define translate_insn4(opT1, opT2, opT3, opT4)         \
    translate_insn_4 ## opT1 ## opT2 ## opT3 ## opT4
#define translate_insn5(opT1, opT2, opT3, opT4, opT5)           \
    translate_insn_5 ## opT1 ## opT2 ## opT3 ## opT4 ## opT5
#define translate_group(grpname)                \
    translate_group_ ## grpname

static void translate_insn0()(
    CPUX86State *env, DisasContext *s, int modrm,
    CheckCpuidFeat feat, unsigned int argc_wr,
    void (*gen_insn_fp)(CPUX86State *, DisasContext *))
{
    if (!check_cpuid(env, s, feat)) {
        gen_illegal_opcode(s);
        return;
    }

    (*gen_insn_fp)(env, s);
}

#define DEF_TRANSLATE_INSN1(opT1)                                       \
    static void translate_insn1(opT1)(                                  \
        CPUX86State *env, DisasContext *s, int modrm,                   \
        CheckCpuidFeat feat, unsigned int argc_wr,                      \
        void (*gen_insn1_fp)(CPUX86State *, DisasContext *,             \
                             insnop_arg_t(opT1)))                       \
    {                                                                   \
        insnop_ctxt_t(opT1) ctxt1;                                      \
                                                                        \
        const bool is_write1 = (1 <= argc_wr);                          \
                                                                        \
        if (check_cpuid(env, s, feat)                                   \
            && insnop_init(opT1)(&ctxt1, env, s, modrm, is_write1)) {   \
                                                                        \
            const insnop_arg_t(opT1) arg1 =                             \
                insnop_prepare(opT1)(&ctxt1, env, s, modrm, is_write1); \
                                                                        \
            (*gen_insn1_fp)(env, s, arg1);                              \
                                                                        \
            insnop_finalize(opT1)(&ctxt1, env, s, modrm, is_write1, arg1); \
        } else {                                                        \
            gen_illegal_opcode(s);                                      \
        }                                                               \
    }

DEF_TRANSLATE_INSN1(Mb)
DEF_TRANSLATE_INSN1(Md)

#define DEF_TRANSLATE_INSN2(opT1, opT2)                                 \
    static void translate_insn2(opT1, opT2)(                            \
        CPUX86State *env, DisasContext *s, int modrm,                   \
        CheckCpuidFeat feat, unsigned int argc_wr,                      \
        void (*gen_insn2_fp)(CPUX86State *, DisasContext *,             \
                             insnop_arg_t(opT1), insnop_arg_t(opT2)))   \
    {                                                                   \
        insnop_ctxt_t(opT1) ctxt1;                                      \
        insnop_ctxt_t(opT2) ctxt2;                                      \
                                                                        \
        const bool is_write1 = (1 <= argc_wr);                          \
        const bool is_write2 = (2 <= argc_wr);                          \
                                                                        \
        if (check_cpuid(env, s, feat)                                   \
            && insnop_init(opT1)(&ctxt1, env, s, modrm, is_write1)      \
            && insnop_init(opT2)(&ctxt2, env, s, modrm, is_write2)) {   \
                                                                        \
            const insnop_arg_t(opT1) arg1 =                             \
                insnop_prepare(opT1)(&ctxt1, env, s, modrm, is_write1); \
            const insnop_arg_t(opT2) arg2 =                             \
                insnop_prepare(opT2)(&ctxt2, env, s, modrm, is_write2); \
                                                                        \
            (*gen_insn2_fp)(env, s, arg1, arg2);                        \
                                                                        \
            insnop_finalize(opT1)(&ctxt1, env, s, modrm, is_write1, arg1); \
            insnop_finalize(opT2)(&ctxt2, env, s, modrm, is_write2, arg2); \
        } else {                                                        \
            gen_illegal_opcode(s);                                      \
        }                                                               \
    }

DEF_TRANSLATE_INSN2(Ed, Pq)
DEF_TRANSLATE_INSN2(Ed, Vdq)
DEF_TRANSLATE_INSN2(Eq, Pq)
DEF_TRANSLATE_INSN2(Eq, Vdq)
DEF_TRANSLATE_INSN2(Gd, Nq)
DEF_TRANSLATE_INSN2(Gd, Udq)
DEF_TRANSLATE_INSN2(Gd, Wd)
DEF_TRANSLATE_INSN2(Gd, Wq)
DEF_TRANSLATE_INSN2(Gq, Nq)
DEF_TRANSLATE_INSN2(Gq, Udq)
DEF_TRANSLATE_INSN2(Gq, Wd)
DEF_TRANSLATE_INSN2(Gq, Wq)
DEF_TRANSLATE_INSN2(Md, Gd)
DEF_TRANSLATE_INSN2(Mdq, Vdq)
DEF_TRANSLATE_INSN2(Mq, Gq)
DEF_TRANSLATE_INSN2(Mq, Pq)
DEF_TRANSLATE_INSN2(Mq, Vdq)
DEF_TRANSLATE_INSN2(Mq, Vq)
DEF_TRANSLATE_INSN2(Pq, Ed)
DEF_TRANSLATE_INSN2(Pq, Eq)
DEF_TRANSLATE_INSN2(Pq, Nq)
DEF_TRANSLATE_INSN2(Pq, Qq)
DEF_TRANSLATE_INSN2(Pq, Uq)
DEF_TRANSLATE_INSN2(Pq, Wdq)
DEF_TRANSLATE_INSN2(Pq, Wq)
DEF_TRANSLATE_INSN2(Qq, Pq)
DEF_TRANSLATE_INSN2(UdqMq, Vq)
DEF_TRANSLATE_INSN2(Vd, Ed)
DEF_TRANSLATE_INSN2(Vd, Eq)
DEF_TRANSLATE_INSN2(Vd, Wd)
DEF_TRANSLATE_INSN2(Vd, Wq)
DEF_TRANSLATE_INSN2(Vdq, Ed)
DEF_TRANSLATE_INSN2(Vdq, Eq)
DEF_TRANSLATE_INSN2(Vdq, Mdq)
DEF_TRANSLATE_INSN2(Vdq, Nq)
DEF_TRANSLATE_INSN2(Vdq, Qq)
DEF_TRANSLATE_INSN2(Vdq, Udq)
DEF_TRANSLATE_INSN2(Vdq, Wd)
DEF_TRANSLATE_INSN2(Vdq, Wdq)
DEF_TRANSLATE_INSN2(Vdq, Wq)
DEF_TRANSLATE_INSN2(Vdq, Ww)
DEF_TRANSLATE_INSN2(Vq, Ed)
DEF_TRANSLATE_INSN2(Vq, Eq)
DEF_TRANSLATE_INSN2(Vq, Wd)
DEF_TRANSLATE_INSN2(Vq, Wq)
DEF_TRANSLATE_INSN2(Wd, Vd)
DEF_TRANSLATE_INSN2(Wdq, Vdq)
DEF_TRANSLATE_INSN2(Wq, Vq)
DEF_TRANSLATE_INSN2(modrm_mod, modrm)

#define DEF_TRANSLATE_INSN3(opT1, opT2, opT3)                           \
    static void translate_insn3(opT1, opT2, opT3)(                      \
        CPUX86State *env, DisasContext *s, int modrm,                   \
        CheckCpuidFeat feat, unsigned int argc_wr,                      \
        void (*gen_insn3_fp)(CPUX86State *, DisasContext *,             \
                             insnop_arg_t(opT1), insnop_arg_t(opT2),    \
                             insnop_arg_t(opT3)))                       \
    {                                                                   \
        insnop_ctxt_t(opT1) ctxt1;                                      \
        insnop_ctxt_t(opT2) ctxt2;                                      \
        insnop_ctxt_t(opT3) ctxt3;                                      \
                                                                        \
        const bool is_write1 = (1 <= argc_wr);                          \
        const bool is_write2 = (2 <= argc_wr);                          \
        const bool is_write3 = (3 <= argc_wr);                          \
                                                                        \
        if (check_cpuid(env, s, feat)                                   \
            && insnop_init(opT1)(&ctxt1, env, s, modrm, is_write1)      \
            && insnop_init(opT2)(&ctxt2, env, s, modrm, is_write2)      \
            && insnop_init(opT3)(&ctxt3, env, s, modrm, is_write3)) {   \
                                                                        \
            const insnop_arg_t(opT1) arg1 =                             \
                insnop_prepare(opT1)(&ctxt1, env, s, modrm, is_write1); \
            const insnop_arg_t(opT2) arg2 =                             \
                insnop_prepare(opT2)(&ctxt2, env, s, modrm, is_write2); \
            const insnop_arg_t(opT3) arg3 =                             \
                insnop_prepare(opT3)(&ctxt3, env, s, modrm, is_write3); \
                                                                        \
            (*gen_insn3_fp)(env, s, arg1, arg2, arg3);                  \
                                                                        \
            insnop_finalize(opT1)(&ctxt1, env, s, modrm, is_write1, arg1); \
            insnop_finalize(opT2)(&ctxt2, env, s, modrm, is_write2, arg2); \
            insnop_finalize(opT3)(&ctxt3, env, s, modrm, is_write3, arg3); \
        } else {                                                        \
            gen_illegal_opcode(s);                                      \
        }                                                               \
    }

DEF_TRANSLATE_INSN3(Ed, Vdq, Ib)
DEF_TRANSLATE_INSN3(Eq, Vdq, Ib)
DEF_TRANSLATE_INSN3(Gd, Nq, Ib)
DEF_TRANSLATE_INSN3(Gd, Udq, Ib)
DEF_TRANSLATE_INSN3(Gq, Nq, Ib)
DEF_TRANSLATE_INSN3(Gq, Udq, Ib)
DEF_TRANSLATE_INSN3(Nq, Nq, Ib)
DEF_TRANSLATE_INSN3(Pq, Pq, Qd)
DEF_TRANSLATE_INSN3(Pq, Pq, Qq)
DEF_TRANSLATE_INSN3(Pq, Qq, Ib)
DEF_TRANSLATE_INSN3(RdMb, Vdq, Ib)
DEF_TRANSLATE_INSN3(RdMw, Vdq, Ib)
DEF_TRANSLATE_INSN3(Udq, Udq, Ib)
DEF_TRANSLATE_INSN3(Vd, Vd, Wd)
DEF_TRANSLATE_INSN3(Vd, Wd, Ib)
DEF_TRANSLATE_INSN3(Vdq, Vdq, Mq)
DEF_TRANSLATE_INSN3(Vdq, Vdq, UdqMhq)
DEF_TRANSLATE_INSN3(Vdq, Vdq, Wdq)
DEF_TRANSLATE_INSN3(Vdq, Vq, Mq)
DEF_TRANSLATE_INSN3(Vdq, Vq, Wq)
DEF_TRANSLATE_INSN3(Vdq, Wdq, Ib)
DEF_TRANSLATE_INSN3(Vq, Vq, Wq)
DEF_TRANSLATE_INSN3(Vq, Wq, Ib)

#define DEF_TRANSLATE_INSN4(opT1, opT2, opT3, opT4)                     \
    static void translate_insn4(opT1, opT2, opT3, opT4)(                \
        CPUX86State *env, DisasContext *s, int modrm,                   \
        CheckCpuidFeat feat, unsigned int argc_wr,                      \
        void (*gen_insn4_fp)(CPUX86State *, DisasContext *,             \
                             insnop_arg_t(opT1), insnop_arg_t(opT2),    \
                             insnop_arg_t(opT3), insnop_arg_t(opT4)))   \
    {                                                                   \
        insnop_ctxt_t(opT1) ctxt1;                                      \
        insnop_ctxt_t(opT2) ctxt2;                                      \
        insnop_ctxt_t(opT3) ctxt3;                                      \
        insnop_ctxt_t(opT4) ctxt4;                                      \
                                                                        \
        const bool is_write1 = (1 <= argc_wr);                          \
        const bool is_write2 = (2 <= argc_wr);                          \
        const bool is_write3 = (3 <= argc_wr);                          \
        const bool is_write4 = (4 <= argc_wr);                          \
                                                                        \
        if (check_cpuid(env, s, feat)                                   \
            && insnop_init(opT1)(&ctxt1, env, s, modrm, is_write1)      \
            && insnop_init(opT2)(&ctxt2, env, s, modrm, is_write2)      \
            && insnop_init(opT3)(&ctxt3, env, s, modrm, is_write3)      \
            && insnop_init(opT4)(&ctxt4, env, s, modrm, is_write4)) {   \
                                                                        \
            const insnop_arg_t(opT1) arg1 =                             \
                insnop_prepare(opT1)(&ctxt1, env, s, modrm, is_write1); \
            const insnop_arg_t(opT2) arg2 =                             \
                insnop_prepare(opT2)(&ctxt2, env, s, modrm, is_write2); \
            const insnop_arg_t(opT3) arg3 =                             \
                insnop_prepare(opT3)(&ctxt3, env, s, modrm, is_write3); \
            const insnop_arg_t(opT4) arg4 =                             \
                insnop_prepare(opT4)(&ctxt4, env, s, modrm, is_write4); \
                                                                        \
            (*gen_insn4_fp)(env, s, arg1, arg2, arg3, arg4);            \
                                                                        \
            insnop_finalize(opT1)(&ctxt1, env, s, modrm, is_write1, arg1); \
            insnop_finalize(opT2)(&ctxt2, env, s, modrm, is_write2, arg2); \
            insnop_finalize(opT3)(&ctxt3, env, s, modrm, is_write3, arg3); \
            insnop_finalize(opT4)(&ctxt4, env, s, modrm, is_write4, arg4); \
        } else {                                                        \
            gen_illegal_opcode(s);                                      \
        }                                                               \
    }

DEF_TRANSLATE_INSN4(Pq, Pq, Qq, Ib)
DEF_TRANSLATE_INSN4(Pq, Pq, RdMw, Ib)
DEF_TRANSLATE_INSN4(Vd, Vd, Wd, Ib)
DEF_TRANSLATE_INSN4(Vdq, Vdq, Ed, Ib)
DEF_TRANSLATE_INSN4(Vdq, Vdq, Eq, Ib)
DEF_TRANSLATE_INSN4(Vdq, Vdq, RdMb, Ib)
DEF_TRANSLATE_INSN4(Vdq, Vdq, RdMw, Ib)
DEF_TRANSLATE_INSN4(Vdq, Vdq, Wd, Ib)
DEF_TRANSLATE_INSN4(Vdq, Vdq, Wd, modrm_mod)
DEF_TRANSLATE_INSN4(Vdq, Vdq, Wdq, Ib)
DEF_TRANSLATE_INSN4(Vdq, Vdq, Wq, modrm_mod)
DEF_TRANSLATE_INSN4(Vq, Vq, Wq, Ib)

#define DEF_TRANSLATE_INSN5(opT1, opT2, opT3, opT4, opT5)               \
    static void translate_insn5(opT1, opT2, opT3, opT4, opT5)(          \
        CPUX86State *env, DisasContext *s, int modrm,                   \
        CheckCpuidFeat feat, unsigned int argc_wr,                      \
        void (*gen_insn5_fp)(CPUX86State *, DisasContext *,             \
                             insnop_arg_t(opT1), insnop_arg_t(opT2),    \
                             insnop_arg_t(opT3), insnop_arg_t(opT4),    \
                             insnop_arg_t(opT5)))                       \
    {                                                                   \
        insnop_ctxt_t(opT1) ctxt1;                                      \
        insnop_ctxt_t(opT2) ctxt2;                                      \
        insnop_ctxt_t(opT3) ctxt3;                                      \
        insnop_ctxt_t(opT4) ctxt4;                                      \
        insnop_ctxt_t(opT5) ctxt5;                                      \
                                                                        \
        const bool is_write1 = (1 <= argc_wr);                          \
        const bool is_write2 = (2 <= argc_wr);                          \
        const bool is_write3 = (3 <= argc_wr);                          \
        const bool is_write4 = (4 <= argc_wr);                          \
        const bool is_write5 = (5 <= argc_wr);                          \
                                                                        \
        if (check_cpuid(env, s, feat)                                   \
            && insnop_init(opT1)(&ctxt1, env, s, modrm, is_write1)      \
            && insnop_init(opT2)(&ctxt2, env, s, modrm, is_write2)      \
            && insnop_init(opT3)(&ctxt3, env, s, modrm, is_write3)      \
            && insnop_init(opT4)(&ctxt4, env, s, modrm, is_write4)      \
            && insnop_init(opT5)(&ctxt5, env, s, modrm, is_write5)) {   \
                                                                        \
            const insnop_arg_t(opT1) arg1 =                             \
                insnop_prepare(opT1)(&ctxt1, env, s, modrm, is_write1); \
            const insnop_arg_t(opT2) arg2 =                             \
                insnop_prepare(opT2)(&ctxt2, env, s, modrm, is_write2); \
            const insnop_arg_t(opT3) arg3 =                             \
                insnop_prepare(opT3)(&ctxt3, env, s, modrm, is_write3); \
            const insnop_arg_t(opT4) arg4 =                             \
                insnop_prepare(opT4)(&ctxt4, env, s, modrm, is_write4); \
            const insnop_arg_t(opT5) arg5 =                             \
                insnop_prepare(opT5)(&ctxt5, env, s, modrm, is_write5); \
                                                                        \
            (*gen_insn5_fp)(env, s, arg1, arg2, arg3, arg4, arg5);      \
                                                                        \
            insnop_finalize(opT1)(&ctxt1, env, s, modrm, is_write1, arg1); \
            insnop_finalize(opT2)(&ctxt2, env, s, modrm, is_write2, arg2); \
            insnop_finalize(opT3)(&ctxt3, env, s, modrm, is_write3, arg3); \
            insnop_finalize(opT4)(&ctxt4, env, s, modrm, is_write4, arg4); \
            insnop_finalize(opT5)(&ctxt5, env, s, modrm, is_write5, arg5); \
        } else {                                                        \
            gen_illegal_opcode(s);                                      \
        }                                                               \
    }

#define OPCODE_GRP_BEGIN(grpname)                                       \
    static void translate_group(grpname)(                               \
        CPUX86State *env, DisasContext *s, int modrm)                   \
    {                                                                   \
        bool ret;                                                       \
        insnop_ctxt_t(modrm_reg) regctxt;                               \
                                                                        \
        ret = insnop_init(modrm_reg)(&regctxt, env, s, modrm, 0);       \
        if (ret) {                                                      \
            const insnop_arg_t(modrm_reg) reg =                         \
                insnop_prepare(modrm_reg)(&regctxt, env, s, modrm, 0);  \
                                                                        \
            switch (reg & 7) {
#define OPCODE_GRPMEMB(grpname, mnem, opcode, feat, fmt, ...)           \
            case opcode:                                                \
                translate_insn(FMT_ARGC(fmt), ## __VA_ARGS__)(          \
                    env, s, modrm, CHECK_CPUID_ ## feat, FMT_ARGC_WR(fmt), \
                    gen_insn(mnem, FMT_ARGC(fmt), ## __VA_ARGS__));     \
                break;
#define OPCODE_GRP_END(grpname)                                         \
            default:                                                    \
                ret = false;                                            \
                break;                                                  \
            }                                                           \
                                                                        \
            insnop_finalize(modrm_reg)(&regctxt, env, s, modrm, 0, reg); \
        }                                                               \
                                                                        \
        if (!ret) {                                                     \
            gen_illegal_opcode(s);                                      \
        }                                                               \
    }
#include "sse-opcode.inc.h"

static void gen_sse_ng(CPUX86State *env, DisasContext *s, int b)
{
    enum {
        P_NP = 0,
        P_66 = 1 << 8,
        P_F3 = 1 << 9,
        P_F2 = 1 << 10,

        M_NA   = 0,
        M_0F   = 1 << 11,
        M_0F38 = 1 << 12,
        M_0F3A = 1 << 13,

        W_0 = 0,
        W_1 = 1 << 14,

        VEX_128 = 1 << 15,
        VEX_256 = 1 << 16,
    };

    const int p =
        (s->prefix & PREFIX_DATA ? P_66 : 0)
        | (s->prefix & PREFIX_REPZ ? P_F3 : 0)
        | (s->prefix & PREFIX_REPNZ ? P_F2 : 0)
        | (!(s->prefix & PREFIX_VEX) ? 0 :
           (s->vex_l ? VEX_256 : VEX_128));
    int m =
        M_0F;
    const int w =
        REX_W(s) > 0 ? W_1 : W_0;
    int op =
        b & 0xff;

    while (1) {
        switch (p | m | w | op) {

#define CASES_0(e)         case (e):
#define CASES_1(e, A, ...) CASES_ ## A(e, 0, ## __VA_ARGS__)
#define CASES_2(e, A, ...) CASES_ ## A(e, 1, ## __VA_ARGS__)
#define CASES_3(e, A, ...) CASES_ ## A(e, 2, ## __VA_ARGS__)
#define CASES_4(e, A, ...) CASES_ ## A(e, 3, ## __VA_ARGS__)
#define CASES(e, N, ...)   CASES_ ## N(e, ## __VA_ARGS__)

#define CASES_P(e, N, p, ...) CASES_P ## p(e, N, ## __VA_ARGS__)
#define CASES_PNP(e, N, ...)  CASES_ ## N(P_NP | e, ## __VA_ARGS__)
#define CASES_P66(e, N, ...)  CASES_ ## N(P_66 | e, ## __VA_ARGS__)
#define CASES_PF3(e, N, ...)  CASES_ ## N(P_F3 | e, ## __VA_ARGS__)
#define CASES_PF2(e, N, ...)  CASES_ ## N(P_F2 | e, ## __VA_ARGS__)
#define CASES_PIG(e, N, ...)  CASES_PNP(e, N, ## __VA_ARGS__)   \
                              CASES_P66(e, N, ## __VA_ARGS__)   \
                              CASES_PF3(e, N, ## __VA_ARGS__)   \
                              CASES_PF2(e, N, ## __VA_ARGS__)

#define CASES_M(e, N, m, ...) CASES_ ## N(M_ ## m | e, ## __VA_ARGS__)

#define CASES_W(e, N, w, ...) CASES_W ## w(e, N, ## __VA_ARGS__)
#define CASES_W0(e, N, ...)   CASES_ ## N(W_0 | e, ## __VA_ARGS__)
#define CASES_W1(e, N, ...)   CASES_ ## N(W_1 | e, ## __VA_ARGS__)
#define CASES_WIG(e, N, ...)  CASES_W0(e, N, ## __VA_ARGS__)    \
                              CASES_W1(e, N, ## __VA_ARGS__)

#define CASES_VEX_L(e, N, l, ...) CASES_VEX_L ## l(e, N, ## __VA_ARGS__)
#define CASES_VEX_L128(e, N, ...) CASES_ ## N(VEX_128 | e, ## __VA_ARGS__)
#define CASES_VEX_L256(e, N, ...) CASES_ ## N(VEX_256 | e, ## __VA_ARGS__)
#define CASES_VEX_LZ              CASES_VEX_L128
#define CASES_VEX_LIG(e, N, ...)  CASES_VEX_L128(e, N, ## __VA_ARGS__)  \
                                  CASES_VEX_L256(e, N, ## __VA_ARGS__)

        CASES(0x38, 3, W, IG, M, 0F, P, IG)
        CASES(0x38, 4, W, IG, M, 0F, P, IG, VEX_L, IG) {
            m = M_0F38;
            op = x86_ldub_code(env, s);
        } break;

        CASES(0x3a, 3, W, IG, M, 0F, P, IG)
        CASES(0x3a, 4, W, IG, M, 0F, P, IG, VEX_L, IG) {
            m = M_0F3A;
            op = x86_ldub_code(env, s);
        } break;

#define LEG(p, m, w, op)    CASES(op, 3, W, w, M, m, P, p)
#define VEX(l, p, m, w, op) CASES(op, 4, W, w, M, m, P, p, VEX_L, l)
#define OPCODE(mnem, cases, feat, fmt, ...)                             \
        cases {                                                         \
            const int modrm = 0 < FMT_ARGC(fmt) ? x86_ldub_code(env, s) : -1; \
            translate_insn(FMT_ARGC(fmt), ## __VA_ARGS__)(              \
                env, s, modrm, CHECK_CPUID_ ## feat, FMT_ARGC_WR(fmt),  \
                gen_insn(mnem, FMT_ARGC(fmt), ## __VA_ARGS__));         \
        } return;
#define OPCODE_GRP(grpname, cases)                      \
        cases {                                         \
            const int modrm = x86_ldub_code(env, s);    \
            translate_group(grpname)(env, s, modrm);    \
        } return;
#include "sse-opcode.inc.h"

        default: {
            if (m == M_0F38 || m == M_0F3A) {
                /* rewind the advance_pc() x86_ldub_code() did */
                advance_pc(env, s, -1);
            }
            gen_sse(env, s, b);
        } return;

#undef CASES_0
#undef CASES_1
#undef CASES_2
#undef CASES_3
#undef CASES_4
#undef CASES
#undef CASES_P
#undef CASES_PNP
#undef CASES_P66
#undef CASES_PF3
#undef CASES_PF2
#undef CASES_PIG
#undef CASES_M
#undef CASES_W
#undef CASES_W0
#undef CASES_W1
#undef CASES_WIG
#undef CASES_VEX_L
#undef CASES_VEX_L128
#undef CASES_VEX_L256
#undef CASES_VEX_LZ
#undef CASES_VEX_LIG
        }
    }
}

static int disas_insn_prefix(DisasContext *s, CPUX86State *env)
{
    int b, prefixes;
    TCGMemOp aflag, dflag;

    s->override = -1;
#ifdef TARGET_X86_64
    s->rex_x = 0;
    s->rex_b = 0;
    s->rex_r = 0;
    s->rex_w = -1;
    s->x86_64_hregs = false;
#endif
    s->rip_offset = 0; /* for relative ip address */
    s->vex_l = 0;
    s->vex_v = 0;

    prefixes = 0;

 next_byte:
    b = x86_ldub_code(env, s);
    /* Collect prefixes.  */
    switch (b) {
    case 0xf3:
        prefixes |= PREFIX_REPZ;
        goto next_byte;
    case 0xf2:
        prefixes |= PREFIX_REPNZ;
        goto next_byte;
    case 0xf0:
        prefixes |= PREFIX_LOCK;
        goto next_byte;
    case 0x2e:
        s->override = R_CS;
        goto next_byte;
    case 0x36:
        s->override = R_SS;
        goto next_byte;
    case 0x3e:
        s->override = R_DS;
        goto next_byte;
    case 0x26:
        s->override = R_ES;
        goto next_byte;
    case 0x64:
        s->override = R_FS;
        goto next_byte;
    case 0x65:
        s->override = R_GS;
        goto next_byte;
    case 0x66:
        prefixes |= PREFIX_DATA;
        goto next_byte;
    case 0x67:
        prefixes |= PREFIX_ADR;
        goto next_byte;
#ifdef TARGET_X86_64
    case 0x40 ... 0x4f:
        if (CODE64(s)) {
            /* REX prefix */
            s->rex_w = (b >> 3) & 1;
            s->rex_r = (b & 0x4) << 1;
            s->rex_x = (b & 0x2) << 2;
            s->rex_b = (b & 0x1) << 3;
            /* select uniform byte register addressing */
            s->x86_64_hregs = true;
            goto next_byte;
        }
        break;
#endif
    case 0xc5: /* 2-byte VEX */
    case 0xc4: /* 3-byte VEX */
        /* VEX prefixes cannot be used except in 32-bit mode.
           Otherwise the instruction is LES or LDS.  */
        if (s->code32 && !s->vm86) {
            static const int pp_prefix[4] = {
                0, PREFIX_DATA, PREFIX_REPZ, PREFIX_REPNZ
            };
            int vex3, vex2 = x86_ldub_code(env, s);

            if (!CODE64(s) && (vex2 & 0xc0) != 0xc0) {
                /* 4.1.4.6: In 32-bit mode, bits [7:6] must be 11b,
                   otherwise the instruction is LES or LDS.  */
                s->pc--; /* rewind the advance_pc() x86_ldub_code() did */
                break;
            }

            /* 4.1.1-4.1.3: No preceding lock, 66, f2, f3, or rex prefixes. */
            if (prefixes & (PREFIX_REPZ | PREFIX_REPNZ
                            | PREFIX_LOCK | PREFIX_DATA)) {
                goto illegal_op;
            }
#ifdef TARGET_X86_64
            if (s->x86_64_hregs) {
                goto illegal_op;
            }
            s->rex_r = (~vex2 >> 4) & 8;
#endif
            if (b == 0xc5) {
                /* 2-byte VEX prefix: RVVVVlpp, implied 0f leading opcode byte */
                vex3 = vex2;
                b = x86_ldub_code(env, s) | 0x100;
            } else {
                /* 3-byte VEX prefix: RXBmmmmm wVVVVlpp */
#ifdef TARGET_X86_64
                s->rex_x = (~vex2 >> 3) & 8;
                s->rex_b = (~vex2 >> 2) & 8;
#endif
                vex3 = x86_ldub_code(env, s);
#ifdef TARGET_X86_64
                s->rex_w = (vex3 >> 7) & 1;
#endif
                switch (vex2 & 0x1f) {
                case 0x01: /* Implied 0f leading opcode bytes.  */
                    b = x86_ldub_code(env, s) | 0x100;
                    break;
                case 0x02: /* Implied 0f 38 leading opcode bytes.  */
                    b = 0x138;
                    break;
                case 0x03: /* Implied 0f 3a leading opcode bytes.  */
                    b = 0x13a;
                    break;
                default:   /* Reserved for future use.  */
                    goto unknown_op;
                }
            }
            s->vex_v = (~vex3 >> 3) & 0xf;
            s->vex_l = (vex3 >> 2) & 1;
            prefixes |= pp_prefix[vex3 & 3] | PREFIX_VEX;
        }
        break;
    }

    /* Post-process prefixes.  */
    if (CODE64(s)) {
        /* In 64-bit mode, the default data size is 32-bit.  Select 64-bit
           data with REX_W, and 16-bit data with 0x66; REX_W takes precedence
           over 0x66 if both are present.  */
        dflag = (REX_W(s) > 0 ? MO_64 : prefixes & PREFIX_DATA ? MO_16 : MO_32);
        /* In 64-bit mode, 0x67 selects 32-bit addressing.  */
        aflag = (prefixes & PREFIX_ADR ? MO_32 : MO_64);
    } else {
        /* In 16/32-bit mode, 0x66 selects the opposite data size.  */
        if (s->code32 ^ ((prefixes & PREFIX_DATA) != 0)) {
            dflag = MO_32;
        } else {
            dflag = MO_16;
        }
        /* In 16/32-bit mode, 0x67 selects the opposite addressing.  */
        if (s->code32 ^ ((prefixes & PREFIX_ADR) != 0)) {
            aflag = MO_32;
        }  else {
            aflag = MO_16;
        }
    }

    s->prefix = prefixes;
    s->aflag = aflag;
    s->dflag = dflag;
    return b;
illegal_op:
    gen_illegal_opcode(s);
    return -1;
unknown_op:
    gen_unknown_opcode(env, s);
    return -1;
}

/*
 * convert one instruction. s->base.is_jmp is set if the translation must
 * be stopped. Return the next pc value.
 */
static target_ulong disas_insn(DisasContext *s, CPUState *cpu)
{
    CPUX86State *env = cpu->env_ptr;
    int b, shift;
    TCGMemOp ot;
    int modrm, reg, rm, mod, op, opreg, val;
    target_ulong next_eip, tval;

    s->pc_start = s->pc = s->base.pc_next;
    if (sigsetjmp(s->jmpbuf, 0) != 0) {
        gen_exception(s, EXCP0D_GPF);
        return s->pc;
    }

    b = disas_insn_prefix(s, env);
    if (b < 0) {
        return s->pc;
    }

    /* now check op code */
 reswitch:
    switch(b) {
    case 0x0f:
        /**************************/
        /* extended op code */
        b = x86_ldub_code(env, s) | 0x100;
        goto reswitch;

        /**************************/
        /* arith & logic */
    case 0x00 ... 0x05:
    case 0x08 ... 0x0d:
    case 0x10 ... 0x15:
    case 0x18 ... 0x1d:
    case 0x20 ... 0x25:
    case 0x28 ... 0x2d:
    case 0x30 ... 0x35:
    case 0x38 ... 0x3d:
        {
            int op, f, val;
            op = (b >> 3) & 7;
            f = (b >> 1) & 3;

            ot = mo_b_d(b, s->dflag);

            switch(f) {
            case 0: /* OP Ev, Gv */
                modrm = x86_ldub_code(env, s);
                reg = ((modrm >> 3) & 7) | REX_R(s);
                mod = (modrm >> 6) & 3;
                rm = (modrm & 7) | REX_B(s);
                if (mod != 3) {
                    gen_lea_modrm(env, s, modrm);
                    opreg = OR_TMP0;
                } else if (op == OP_XORL && rm == reg) {
                xor_zero:
                    /* xor reg, reg optimisation */
                    set_cc_op(s, CC_OP_CLR);
                    tcg_gen_movi_tl(s->T0, 0);
                    gen_op_mov_reg_v(s, ot, reg, s->T0);
                    break;
                } else {
                    opreg = rm;
                }
                gen_op_mov_v_reg(s, ot, s->T1, reg);
                gen_op(s, op, ot, opreg);
                break;
            case 1: /* OP Gv, Ev */
                modrm = x86_ldub_code(env, s);
                mod = (modrm >> 6) & 3;
                reg = ((modrm >> 3) & 7) | REX_R(s);
                rm = (modrm & 7) | REX_B(s);
                if (mod != 3) {
                    gen_lea_modrm(env, s, modrm);
                    gen_op_ld_v(s, ot, s->T1, s->A0);
                } else if (op == OP_XORL && rm == reg) {
                    goto xor_zero;
                } else {
                    gen_op_mov_v_reg(s, ot, s->T1, rm);
                }
                gen_op(s, op, ot, reg);
                break;
            case 2: /* OP A, Iv */
                val = insn_get(env, s, ot);
                tcg_gen_movi_tl(s->T1, val);
                gen_op(s, op, ot, OR_EAX);
                break;
            }
        }
        break;

    case 0x82:
        if (CODE64(s))
            goto illegal_op;
        /* fall through */
    case 0x80: /* GRP1 */
    case 0x81:
    case 0x83:
        {
            int val;

            ot = mo_b_d(b, s->dflag);

            modrm = x86_ldub_code(env, s);
            mod = (modrm >> 6) & 3;
            rm = (modrm & 7) | REX_B(s);
            op = (modrm >> 3) & 7;

            if (mod != 3) {
                if (b == 0x83)
                    s->rip_offset = 1;
                else
                    s->rip_offset = insn_const_size(ot);
                gen_lea_modrm(env, s, modrm);
                opreg = OR_TMP0;
            } else {
                opreg = rm;
            }

            switch(b) {
            default:
            case 0x80:
            case 0x81:
            case 0x82:
                val = insn_get(env, s, ot);
                break;
            case 0x83:
                val = (int8_t)insn_get(env, s, MO_8);
                break;
            }
            tcg_gen_movi_tl(s->T1, val);
            gen_op(s, op, ot, opreg);
        }
        break;

        /**************************/
        /* inc, dec, and other misc arith */
    case 0x40 ... 0x47: /* inc Gv */
        ot = s->dflag;
        gen_inc(s, ot, OR_EAX + (b & 7), 1);
        break;
    case 0x48 ... 0x4f: /* dec Gv */
        ot = s->dflag;
        gen_inc(s, ot, OR_EAX + (b & 7), -1);
        break;
    case 0xf6: /* GRP3 */
    case 0xf7:
        ot = mo_b_d(b, s->dflag);

        modrm = x86_ldub_code(env, s);
        mod = (modrm >> 6) & 3;
        rm = (modrm & 7) | REX_B(s);
        op = (modrm >> 3) & 7;
        if (mod != 3) {
            if (op == 0) {
                s->rip_offset = insn_const_size(ot);
            }
            gen_lea_modrm(env, s, modrm);
            /* For those below that handle locked memory, don't load here.  */
            if (!(s->prefix & PREFIX_LOCK)
                || op != 2) {
                gen_op_ld_v(s, ot, s->T0, s->A0);
            }
        } else {
            gen_op_mov_v_reg(s, ot, s->T0, rm);
        }

        switch(op) {
        case 0: /* test */
            val = insn_get(env, s, ot);
            tcg_gen_movi_tl(s->T1, val);
            gen_op_testl_T0_T1_cc(s);
            set_cc_op(s, CC_OP_LOGICB + ot);
            break;
        case 2: /* not */
            if (s->prefix & PREFIX_LOCK) {
                if (mod == 3) {
                    goto illegal_op;
                }
                tcg_gen_movi_tl(s->T0, ~0);
                tcg_gen_atomic_xor_fetch_tl(s->T0, s->A0, s->T0,
                                            s->mem_index, ot | MO_LE);
            } else {
                tcg_gen_not_tl(s->T0, s->T0);
                if (mod != 3) {
                    gen_op_st_v(s, ot, s->T0, s->A0);
                } else {
                    gen_op_mov_reg_v(s, ot, rm, s->T0);
                }
            }
            break;
        case 3: /* neg */
            if (s->prefix & PREFIX_LOCK) {
                TCGLabel *label1;
                TCGv a0, t0, t1, t2;

                if (mod == 3) {
                    goto illegal_op;
                }
                a0 = tcg_temp_local_new();
                t0 = tcg_temp_local_new();
                label1 = gen_new_label();

                tcg_gen_mov_tl(a0, s->A0);
                tcg_gen_mov_tl(t0, s->T0);

                gen_set_label(label1);
                t1 = tcg_temp_new();
                t2 = tcg_temp_new();
                tcg_gen_mov_tl(t2, t0);
                tcg_gen_neg_tl(t1, t0);
                tcg_gen_atomic_cmpxchg_tl(t0, a0, t0, t1,
                                          s->mem_index, ot | MO_LE);
                tcg_temp_free(t1);
                tcg_gen_brcond_tl(TCG_COND_NE, t0, t2, label1);

                tcg_temp_free(t2);
                tcg_temp_free(a0);
                tcg_gen_mov_tl(s->T0, t0);
                tcg_temp_free(t0);
            } else {
                tcg_gen_neg_tl(s->T0, s->T0);
                if (mod != 3) {
                    gen_op_st_v(s, ot, s->T0, s->A0);
                } else {
                    gen_op_mov_reg_v(s, ot, rm, s->T0);
                }
            }
            gen_op_update_neg_cc(s);
            set_cc_op(s, CC_OP_SUBB + ot);
            break;
        case 4: /* mul */
            switch(ot) {
            case MO_8:
                gen_op_mov_v_reg(s, MO_8, s->T1, R_EAX);
                tcg_gen_ext8u_tl(s->T0, s->T0);
                tcg_gen_ext8u_tl(s->T1, s->T1);
                /* XXX: use 32 bit mul which could be faster */
                tcg_gen_mul_tl(s->T0, s->T0, s->T1);
                gen_op_mov_reg_v(s, MO_16, R_EAX, s->T0);
                tcg_gen_mov_tl(cpu_cc_dst, s->T0);
                tcg_gen_andi_tl(cpu_cc_src, s->T0, 0xff00);
                set_cc_op(s, CC_OP_MULB);
                break;
            case MO_16:
                gen_op_mov_v_reg(s, MO_16, s->T1, R_EAX);
                tcg_gen_ext16u_tl(s->T0, s->T0);
                tcg_gen_ext16u_tl(s->T1, s->T1);
                /* XXX: use 32 bit mul which could be faster */
                tcg_gen_mul_tl(s->T0, s->T0, s->T1);
                gen_op_mov_reg_v(s, MO_16, R_EAX, s->T0);
                tcg_gen_mov_tl(cpu_cc_dst, s->T0);
                tcg_gen_shri_tl(s->T0, s->T0, 16);
                gen_op_mov_reg_v(s, MO_16, R_EDX, s->T0);
                tcg_gen_mov_tl(cpu_cc_src, s->T0);
                set_cc_op(s, CC_OP_MULW);
                break;
            default:
            case MO_32:
                tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
                tcg_gen_trunc_tl_i32(s->tmp3_i32, cpu_regs[R_EAX]);
                tcg_gen_mulu2_i32(s->tmp2_i32, s->tmp3_i32,
                                  s->tmp2_i32, s->tmp3_i32);
                tcg_gen_extu_i32_tl(cpu_regs[R_EAX], s->tmp2_i32);
                tcg_gen_extu_i32_tl(cpu_regs[R_EDX], s->tmp3_i32);
                tcg_gen_mov_tl(cpu_cc_dst, cpu_regs[R_EAX]);
                tcg_gen_mov_tl(cpu_cc_src, cpu_regs[R_EDX]);
                set_cc_op(s, CC_OP_MULL);
                break;
#ifdef TARGET_X86_64
            case MO_64:
                tcg_gen_mulu2_i64(cpu_regs[R_EAX], cpu_regs[R_EDX],
                                  s->T0, cpu_regs[R_EAX]);
                tcg_gen_mov_tl(cpu_cc_dst, cpu_regs[R_EAX]);
                tcg_gen_mov_tl(cpu_cc_src, cpu_regs[R_EDX]);
                set_cc_op(s, CC_OP_MULQ);
                break;
#endif
            }
            break;
        case 5: /* imul */
            switch(ot) {
            case MO_8:
                gen_op_mov_v_reg(s, MO_8, s->T1, R_EAX);
                tcg_gen_ext8s_tl(s->T0, s->T0);
                tcg_gen_ext8s_tl(s->T1, s->T1);
                /* XXX: use 32 bit mul which could be faster */
                tcg_gen_mul_tl(s->T0, s->T0, s->T1);
                gen_op_mov_reg_v(s, MO_16, R_EAX, s->T0);
                tcg_gen_mov_tl(cpu_cc_dst, s->T0);
                tcg_gen_ext8s_tl(s->tmp0, s->T0);
                tcg_gen_sub_tl(cpu_cc_src, s->T0, s->tmp0);
                set_cc_op(s, CC_OP_MULB);
                break;
            case MO_16:
                gen_op_mov_v_reg(s, MO_16, s->T1, R_EAX);
                tcg_gen_ext16s_tl(s->T0, s->T0);
                tcg_gen_ext16s_tl(s->T1, s->T1);
                /* XXX: use 32 bit mul which could be faster */
                tcg_gen_mul_tl(s->T0, s->T0, s->T1);
                gen_op_mov_reg_v(s, MO_16, R_EAX, s->T0);
                tcg_gen_mov_tl(cpu_cc_dst, s->T0);
                tcg_gen_ext16s_tl(s->tmp0, s->T0);
                tcg_gen_sub_tl(cpu_cc_src, s->T0, s->tmp0);
                tcg_gen_shri_tl(s->T0, s->T0, 16);
                gen_op_mov_reg_v(s, MO_16, R_EDX, s->T0);
                set_cc_op(s, CC_OP_MULW);
                break;
            default:
            case MO_32:
                tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
                tcg_gen_trunc_tl_i32(s->tmp3_i32, cpu_regs[R_EAX]);
                tcg_gen_muls2_i32(s->tmp2_i32, s->tmp3_i32,
                                  s->tmp2_i32, s->tmp3_i32);
                tcg_gen_extu_i32_tl(cpu_regs[R_EAX], s->tmp2_i32);
                tcg_gen_extu_i32_tl(cpu_regs[R_EDX], s->tmp3_i32);
                tcg_gen_sari_i32(s->tmp2_i32, s->tmp2_i32, 31);
                tcg_gen_mov_tl(cpu_cc_dst, cpu_regs[R_EAX]);
                tcg_gen_sub_i32(s->tmp2_i32, s->tmp2_i32, s->tmp3_i32);
                tcg_gen_extu_i32_tl(cpu_cc_src, s->tmp2_i32);
                set_cc_op(s, CC_OP_MULL);
                break;
#ifdef TARGET_X86_64
            case MO_64:
                tcg_gen_muls2_i64(cpu_regs[R_EAX], cpu_regs[R_EDX],
                                  s->T0, cpu_regs[R_EAX]);
                tcg_gen_mov_tl(cpu_cc_dst, cpu_regs[R_EAX]);
                tcg_gen_sari_tl(cpu_cc_src, cpu_regs[R_EAX], 63);
                tcg_gen_sub_tl(cpu_cc_src, cpu_cc_src, cpu_regs[R_EDX]);
                set_cc_op(s, CC_OP_MULQ);
                break;
#endif
            }
            break;
        case 6: /* div */
            switch(ot) {
            case MO_8:
                gen_helper_divb_AL(cpu_env, s->T0);
                break;
            case MO_16:
                gen_helper_divw_AX(cpu_env, s->T0);
                break;
            default:
            case MO_32:
                gen_helper_divl_EAX(cpu_env, s->T0);
                break;
#ifdef TARGET_X86_64
            case MO_64:
                gen_helper_divq_EAX(cpu_env, s->T0);
                break;
#endif
            }
            break;
        case 7: /* idiv */
            switch(ot) {
            case MO_8:
                gen_helper_idivb_AL(cpu_env, s->T0);
                break;
            case MO_16:
                gen_helper_idivw_AX(cpu_env, s->T0);
                break;
            default:
            case MO_32:
                gen_helper_idivl_EAX(cpu_env, s->T0);
                break;
#ifdef TARGET_X86_64
            case MO_64:
                gen_helper_idivq_EAX(cpu_env, s->T0);
                break;
#endif
            }
            break;
        default:
            goto unknown_op;
        }
        break;

    case 0xfe: /* GRP4 */
    case 0xff: /* GRP5 */
        ot = mo_b_d(b, s->dflag);

        modrm = x86_ldub_code(env, s);
        mod = (modrm >> 6) & 3;
        rm = (modrm & 7) | REX_B(s);
        op = (modrm >> 3) & 7;
        if (op >= 2 && b == 0xfe) {
            goto unknown_op;
        }
        if (CODE64(s)) {
            if (op == 2 || op == 4) {
                /* operand size for jumps is 64 bit */
                ot = MO_64;
            } else if (op == 3 || op == 5) {
                ot = s->dflag != MO_16 ? MO_32 + (REX_W(s) == 1) : MO_16;
            } else if (op == 6) {
                /* default push size is 64 bit */
                ot = mo_pushpop(s, s->dflag);
            }
        }
        if (mod != 3) {
            gen_lea_modrm(env, s, modrm);
            if (op >= 2 && op != 3 && op != 5)
                gen_op_ld_v(s, ot, s->T0, s->A0);
        } else {
            gen_op_mov_v_reg(s, ot, s->T0, rm);
        }

        switch(op) {
        case 0: /* inc Ev */
            if (mod != 3)
                opreg = OR_TMP0;
            else
                opreg = rm;
            gen_inc(s, ot, opreg, 1);
            break;
        case 1: /* dec Ev */
            if (mod != 3)
                opreg = OR_TMP0;
            else
                opreg = rm;
            gen_inc(s, ot, opreg, -1);
            break;
        case 2: /* call Ev */
            /* XXX: optimize if memory (no 'and' is necessary) */
            if (s->dflag == MO_16) {
                tcg_gen_ext16u_tl(s->T0, s->T0);
            }
            next_eip = s->pc - s->cs_base;
            tcg_gen_movi_tl(s->T1, next_eip);
            gen_push_v(s, s->T1);
            gen_op_jmp_v(s->T0);
            gen_bnd_jmp(s);
            gen_jr(s, s->T0);
            break;
        case 3: /* lcall Ev */
            gen_op_ld_v(s, ot, s->T1, s->A0);
            gen_add_A0_im(s, 1 << ot);
            gen_op_ld_v(s, MO_16, s->T0, s->A0);
        do_lcall:
            if (s->pe && !s->vm86) {
                tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
                gen_helper_lcall_protected(cpu_env, s->tmp2_i32, s->T1,
                                           tcg_const_i32(s->dflag - 1),
                                           tcg_const_tl(s->pc - s->cs_base));
            } else {
                tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
                gen_helper_lcall_real(cpu_env, s->tmp2_i32, s->T1,
                                      tcg_const_i32(s->dflag - 1),
                                      tcg_const_i32(s->pc - s->cs_base));
            }
            tcg_gen_ld_tl(s->tmp4, cpu_env, offsetof(CPUX86State, eip));
            gen_jr(s, s->tmp4);
            break;
        case 4: /* jmp Ev */
            if (s->dflag == MO_16) {
                tcg_gen_ext16u_tl(s->T0, s->T0);
            }
            gen_op_jmp_v(s->T0);
            gen_bnd_jmp(s);
            gen_jr(s, s->T0);
            break;
        case 5: /* ljmp Ev */
            gen_op_ld_v(s, ot, s->T1, s->A0);
            gen_add_A0_im(s, 1 << ot);
            gen_op_ld_v(s, MO_16, s->T0, s->A0);
        do_ljmp:
            if (s->pe && !s->vm86) {
                tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
                gen_helper_ljmp_protected(cpu_env, s->tmp2_i32, s->T1,
                                          tcg_const_tl(s->pc - s->cs_base));
            } else {
                gen_op_movl_seg_T0_vm(s, R_CS);
                gen_op_jmp_v(s->T1);
            }
            tcg_gen_ld_tl(s->tmp4, cpu_env, offsetof(CPUX86State, eip));
            gen_jr(s, s->tmp4);
            break;
        case 6: /* push Ev */
            gen_push_v(s, s->T0);
            break;
        default:
            goto unknown_op;
        }
        break;

    case 0x84: /* test Ev, Gv */
    case 0x85:
        ot = mo_b_d(b, s->dflag);

        modrm = x86_ldub_code(env, s);
        reg = ((modrm >> 3) & 7) | REX_R(s);

        gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
        gen_op_mov_v_reg(s, ot, s->T1, reg);
        gen_op_testl_T0_T1_cc(s);
        set_cc_op(s, CC_OP_LOGICB + ot);
        break;

    case 0xa8: /* test eAX, Iv */
    case 0xa9:
        ot = mo_b_d(b, s->dflag);
        val = insn_get(env, s, ot);

        gen_op_mov_v_reg(s, ot, s->T0, OR_EAX);
        tcg_gen_movi_tl(s->T1, val);
        gen_op_testl_T0_T1_cc(s);
        set_cc_op(s, CC_OP_LOGICB + ot);
        break;

    case 0x98: /* CWDE/CBW */
        switch (s->dflag) {
#ifdef TARGET_X86_64
        case MO_64:
            gen_op_mov_v_reg(s, MO_32, s->T0, R_EAX);
            tcg_gen_ext32s_tl(s->T0, s->T0);
            gen_op_mov_reg_v(s, MO_64, R_EAX, s->T0);
            break;
#endif
        case MO_32:
            gen_op_mov_v_reg(s, MO_16, s->T0, R_EAX);
            tcg_gen_ext16s_tl(s->T0, s->T0);
            gen_op_mov_reg_v(s, MO_32, R_EAX, s->T0);
            break;
        case MO_16:
            gen_op_mov_v_reg(s, MO_8, s->T0, R_EAX);
            tcg_gen_ext8s_tl(s->T0, s->T0);
            gen_op_mov_reg_v(s, MO_16, R_EAX, s->T0);
            break;
        default:
            tcg_abort();
        }
        break;
    case 0x99: /* CDQ/CWD */
        switch (s->dflag) {
#ifdef TARGET_X86_64
        case MO_64:
            gen_op_mov_v_reg(s, MO_64, s->T0, R_EAX);
            tcg_gen_sari_tl(s->T0, s->T0, 63);
            gen_op_mov_reg_v(s, MO_64, R_EDX, s->T0);
            break;
#endif
        case MO_32:
            gen_op_mov_v_reg(s, MO_32, s->T0, R_EAX);
            tcg_gen_ext32s_tl(s->T0, s->T0);
            tcg_gen_sari_tl(s->T0, s->T0, 31);
            gen_op_mov_reg_v(s, MO_32, R_EDX, s->T0);
            break;
        case MO_16:
            gen_op_mov_v_reg(s, MO_16, s->T0, R_EAX);
            tcg_gen_ext16s_tl(s->T0, s->T0);
            tcg_gen_sari_tl(s->T0, s->T0, 15);
            gen_op_mov_reg_v(s, MO_16, R_EDX, s->T0);
            break;
        default:
            tcg_abort();
        }
        break;
    case 0x1af: /* imul Gv, Ev */
    case 0x69: /* imul Gv, Ev, I */
    case 0x6b:
        ot = s->dflag;
        modrm = x86_ldub_code(env, s);
        reg = ((modrm >> 3) & 7) | REX_R(s);
        if (b == 0x69)
            s->rip_offset = insn_const_size(ot);
        else if (b == 0x6b)
            s->rip_offset = 1;
        gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
        if (b == 0x69) {
            val = insn_get(env, s, ot);
            tcg_gen_movi_tl(s->T1, val);
        } else if (b == 0x6b) {
            val = (int8_t)insn_get(env, s, MO_8);
            tcg_gen_movi_tl(s->T1, val);
        } else {
            gen_op_mov_v_reg(s, ot, s->T1, reg);
        }
        switch (ot) {
#ifdef TARGET_X86_64
        case MO_64:
            tcg_gen_muls2_i64(cpu_regs[reg], s->T1, s->T0, s->T1);
            tcg_gen_mov_tl(cpu_cc_dst, cpu_regs[reg]);
            tcg_gen_sari_tl(cpu_cc_src, cpu_cc_dst, 63);
            tcg_gen_sub_tl(cpu_cc_src, cpu_cc_src, s->T1);
            break;
#endif
        case MO_32:
            tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
            tcg_gen_trunc_tl_i32(s->tmp3_i32, s->T1);
            tcg_gen_muls2_i32(s->tmp2_i32, s->tmp3_i32,
                              s->tmp2_i32, s->tmp3_i32);
            tcg_gen_extu_i32_tl(cpu_regs[reg], s->tmp2_i32);
            tcg_gen_sari_i32(s->tmp2_i32, s->tmp2_i32, 31);
            tcg_gen_mov_tl(cpu_cc_dst, cpu_regs[reg]);
            tcg_gen_sub_i32(s->tmp2_i32, s->tmp2_i32, s->tmp3_i32);
            tcg_gen_extu_i32_tl(cpu_cc_src, s->tmp2_i32);
            break;
        default:
            tcg_gen_ext16s_tl(s->T0, s->T0);
            tcg_gen_ext16s_tl(s->T1, s->T1);
            /* XXX: use 32 bit mul which could be faster */
            tcg_gen_mul_tl(s->T0, s->T0, s->T1);
            tcg_gen_mov_tl(cpu_cc_dst, s->T0);
            tcg_gen_ext16s_tl(s->tmp0, s->T0);
            tcg_gen_sub_tl(cpu_cc_src, s->T0, s->tmp0);
            gen_op_mov_reg_v(s, ot, reg, s->T0);
            break;
        }
        set_cc_op(s, CC_OP_MULB + ot);
        break;
    case 0x1c0:
    case 0x1c1: /* xadd Ev, Gv */
        ot = mo_b_d(b, s->dflag);
        modrm = x86_ldub_code(env, s);
        reg = ((modrm >> 3) & 7) | REX_R(s);
        mod = (modrm >> 6) & 3;
        gen_op_mov_v_reg(s, ot, s->T0, reg);
        if (mod == 3) {
            rm = (modrm & 7) | REX_B(s);
            gen_op_mov_v_reg(s, ot, s->T1, rm);
            tcg_gen_add_tl(s->T0, s->T0, s->T1);
            gen_op_mov_reg_v(s, ot, reg, s->T1);
            gen_op_mov_reg_v(s, ot, rm, s->T0);
        } else {
            gen_lea_modrm(env, s, modrm);
            if (s->prefix & PREFIX_LOCK) {
                tcg_gen_atomic_fetch_add_tl(s->T1, s->A0, s->T0,
                                            s->mem_index, ot | MO_LE);
                tcg_gen_add_tl(s->T0, s->T0, s->T1);
            } else {
                gen_op_ld_v(s, ot, s->T1, s->A0);
                tcg_gen_add_tl(s->T0, s->T0, s->T1);
                gen_op_st_v(s, ot, s->T0, s->A0);
            }
            gen_op_mov_reg_v(s, ot, reg, s->T1);
        }
        gen_op_update2_cc(s);
        set_cc_op(s, CC_OP_ADDB + ot);
        break;
    case 0x1b0:
    case 0x1b1: /* cmpxchg Ev, Gv */
        {
            TCGv oldv, newv, cmpv;

            ot = mo_b_d(b, s->dflag);
            modrm = x86_ldub_code(env, s);
            reg = ((modrm >> 3) & 7) | REX_R(s);
            mod = (modrm >> 6) & 3;
            oldv = tcg_temp_new();
            newv = tcg_temp_new();
            cmpv = tcg_temp_new();
            gen_op_mov_v_reg(s, ot, newv, reg);
            tcg_gen_mov_tl(cmpv, cpu_regs[R_EAX]);

            if (s->prefix & PREFIX_LOCK) {
                if (mod == 3) {
                    goto illegal_op;
                }
                gen_lea_modrm(env, s, modrm);
                tcg_gen_atomic_cmpxchg_tl(oldv, s->A0, cmpv, newv,
                                          s->mem_index, ot | MO_LE);
                gen_op_mov_reg_v(s, ot, R_EAX, oldv);
            } else {
                if (mod == 3) {
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_mov_v_reg(s, ot, oldv, rm);
                } else {
                    gen_lea_modrm(env, s, modrm);
                    gen_op_ld_v(s, ot, oldv, s->A0);
                    rm = 0; /* avoid warning */
                }
                gen_extu(ot, oldv);
                gen_extu(ot, cmpv);
                /* store value = (old == cmp ? new : old);  */
                tcg_gen_movcond_tl(TCG_COND_EQ, newv, oldv, cmpv, newv, oldv);
                if (mod == 3) {
                    gen_op_mov_reg_v(s, ot, R_EAX, oldv);
                    gen_op_mov_reg_v(s, ot, rm, newv);
                } else {
                    /* Perform an unconditional store cycle like physical cpu;
                       must be before changing accumulator to ensure
                       idempotency if the store faults and the instruction
                       is restarted */
                    gen_op_st_v(s, ot, newv, s->A0);
                    gen_op_mov_reg_v(s, ot, R_EAX, oldv);
                }
            }
            tcg_gen_mov_tl(cpu_cc_src, oldv);
            tcg_gen_mov_tl(s->cc_srcT, cmpv);
            tcg_gen_sub_tl(cpu_cc_dst, cmpv, oldv);
            set_cc_op(s, CC_OP_SUBB + ot);
            tcg_temp_free(oldv);
            tcg_temp_free(newv);
            tcg_temp_free(cmpv);
        }
        break;
    case 0x1c7: /* cmpxchg8b */
        modrm = x86_ldub_code(env, s);
        mod = (modrm >> 6) & 3;
        switch ((modrm >> 3) & 7) {
        case 1: /* CMPXCHG8, CMPXCHG16 */
            if (mod == 3) {
                goto illegal_op;
            }
#ifdef TARGET_X86_64
            if (s->dflag == MO_64) {
                if (!(s->cpuid_ext_features & CPUID_EXT_CX16)) {
                    goto illegal_op;
                }
                gen_lea_modrm(env, s, modrm);
                if ((s->prefix & PREFIX_LOCK) &&
                    (tb_cflags(s->base.tb) & CF_PARALLEL)) {
                    gen_helper_cmpxchg16b(cpu_env, s->A0);
                } else {
                    gen_helper_cmpxchg16b_unlocked(cpu_env, s->A0);
                }
                set_cc_op(s, CC_OP_EFLAGS);
                break;
            }
#endif        
            if (!(s->cpuid_features & CPUID_CX8)) {
                goto illegal_op;
            }
            gen_lea_modrm(env, s, modrm);
            if ((s->prefix & PREFIX_LOCK) &&
                (tb_cflags(s->base.tb) & CF_PARALLEL)) {
                gen_helper_cmpxchg8b(cpu_env, s->A0);
            } else {
                gen_helper_cmpxchg8b_unlocked(cpu_env, s->A0);
            }
            set_cc_op(s, CC_OP_EFLAGS);
            break;

        case 7: /* RDSEED */
        case 6: /* RDRAND */
            if (mod != 3 ||
                (s->prefix & (PREFIX_LOCK | PREFIX_REPZ | PREFIX_REPNZ)) ||
                !(s->cpuid_ext_features & CPUID_EXT_RDRAND)) {
                goto illegal_op;
            }
            if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
                gen_io_start();
            }
            gen_helper_rdrand(s->T0, cpu_env);
            rm = (modrm & 7) | REX_B(s);
            gen_op_mov_reg_v(s, s->dflag, rm, s->T0);
            set_cc_op(s, CC_OP_EFLAGS);
            if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
                gen_jmp(s, s->pc - s->cs_base);
            }
            break;

        default:
            goto illegal_op;
        }
        break;

        /**************************/
        /* push/pop */
    case 0x50 ... 0x57: /* push */
        gen_op_mov_v_reg(s, MO_32, s->T0, (b & 7) | REX_B(s));
        gen_push_v(s, s->T0);
        break;
    case 0x58 ... 0x5f: /* pop */
        ot = gen_pop_T0(s);
        /* NOTE: order is important for pop %sp */
        gen_pop_update(s, ot);
        gen_op_mov_reg_v(s, ot, (b & 7) | REX_B(s), s->T0);
        break;
    case 0x60: /* pusha */
        if (CODE64(s))
            goto illegal_op;
        gen_pusha(s);
        break;
    case 0x61: /* popa */
        if (CODE64(s))
            goto illegal_op;
        gen_popa(s);
        break;
    case 0x68: /* push Iv */
    case 0x6a:
        ot = mo_pushpop(s, s->dflag);
        if (b == 0x68)
            val = insn_get(env, s, ot);
        else
            val = (int8_t)insn_get(env, s, MO_8);
        tcg_gen_movi_tl(s->T0, val);
        gen_push_v(s, s->T0);
        break;
    case 0x8f: /* pop Ev */
        modrm = x86_ldub_code(env, s);
        mod = (modrm >> 6) & 3;
        ot = gen_pop_T0(s);
        if (mod == 3) {
            /* NOTE: order is important for pop %sp */
            gen_pop_update(s, ot);
            rm = (modrm & 7) | REX_B(s);
            gen_op_mov_reg_v(s, ot, rm, s->T0);
        } else {
            /* NOTE: order is important too for MMU exceptions */
            s->popl_esp_hack = 1 << ot;
            gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 1);
            s->popl_esp_hack = 0;
            gen_pop_update(s, ot);
        }
        break;
    case 0xc8: /* enter */
        {
            int level;
            val = x86_lduw_code(env, s);
            level = x86_ldub_code(env, s);
            gen_enter(s, val, level);
        }
        break;
    case 0xc9: /* leave */
        gen_leave(s);
        break;
    case 0x06: /* push es */
    case 0x0e: /* push cs */
    case 0x16: /* push ss */
    case 0x1e: /* push ds */
        if (CODE64(s))
            goto illegal_op;
        gen_op_movl_T0_seg(s, b >> 3);
        gen_push_v(s, s->T0);
        break;
    case 0x1a0: /* push fs */
    case 0x1a8: /* push gs */
        gen_op_movl_T0_seg(s, (b >> 3) & 7);
        gen_push_v(s, s->T0);
        break;
    case 0x07: /* pop es */
    case 0x17: /* pop ss */
    case 0x1f: /* pop ds */
        if (CODE64(s))
            goto illegal_op;
        reg = b >> 3;
        ot = gen_pop_T0(s);
        gen_movl_seg_T0(s, reg);
        gen_pop_update(s, ot);
        /* Note that reg == R_SS in gen_movl_seg_T0 always sets is_jmp.  */
        if (s->base.is_jmp) {
            gen_jmp_im(s, s->pc - s->cs_base);
            if (reg == R_SS) {
                s->tf = 0;
                gen_eob_inhibit_irq(s, true);
            } else {
                gen_eob(s);
            }
        }
        break;
    case 0x1a1: /* pop fs */
    case 0x1a9: /* pop gs */
        ot = gen_pop_T0(s);
        gen_movl_seg_T0(s, (b >> 3) & 7);
        gen_pop_update(s, ot);
        if (s->base.is_jmp) {
            gen_jmp_im(s, s->pc - s->cs_base);
            gen_eob(s);
        }
        break;

        /**************************/
        /* mov */
    case 0x88:
    case 0x89: /* mov Gv, Ev */
        ot = mo_b_d(b, s->dflag);
        modrm = x86_ldub_code(env, s);
        reg = ((modrm >> 3) & 7) | REX_R(s);

        /* generate a generic store */
        gen_ldst_modrm(env, s, modrm, ot, reg, 1);
        break;
    case 0xc6:
    case 0xc7: /* mov Ev, Iv */
        ot = mo_b_d(b, s->dflag);
        modrm = x86_ldub_code(env, s);
        mod = (modrm >> 6) & 3;
        if (mod != 3) {
            s->rip_offset = insn_const_size(ot);
            gen_lea_modrm(env, s, modrm);
        }
        val = insn_get(env, s, ot);
        tcg_gen_movi_tl(s->T0, val);
        if (mod != 3) {
            gen_op_st_v(s, ot, s->T0, s->A0);
        } else {
            gen_op_mov_reg_v(s, ot, (modrm & 7) | REX_B(s), s->T0);
        }
        break;
    case 0x8a:
    case 0x8b: /* mov Ev, Gv */
        ot = mo_b_d(b, s->dflag);
        modrm = x86_ldub_code(env, s);
        reg = ((modrm >> 3) & 7) | REX_R(s);

        gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
        gen_op_mov_reg_v(s, ot, reg, s->T0);
        break;
    case 0x8e: /* mov seg, Gv */
        modrm = x86_ldub_code(env, s);
        reg = (modrm >> 3) & 7;
        if (reg >= 6 || reg == R_CS)
            goto illegal_op;
        gen_ldst_modrm(env, s, modrm, MO_16, OR_TMP0, 0);
        gen_movl_seg_T0(s, reg);
        /* Note that reg == R_SS in gen_movl_seg_T0 always sets is_jmp.  */
        if (s->base.is_jmp) {
            gen_jmp_im(s, s->pc - s->cs_base);
            if (reg == R_SS) {
                s->tf = 0;
                gen_eob_inhibit_irq(s, true);
            } else {
                gen_eob(s);
            }
        }
        break;
    case 0x8c: /* mov Gv, seg */
        modrm = x86_ldub_code(env, s);
        reg = (modrm >> 3) & 7;
        mod = (modrm >> 6) & 3;
        if (reg >= 6)
            goto illegal_op;
        gen_op_movl_T0_seg(s, reg);
        ot = mod == 3 ? s->dflag : MO_16;
        gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 1);
        break;

    case 0x1b6: /* movzbS Gv, Eb */
    case 0x1b7: /* movzwS Gv, Eb */
    case 0x1be: /* movsbS Gv, Eb */
    case 0x1bf: /* movswS Gv, Eb */
        {
            TCGMemOp d_ot;
            TCGMemOp s_ot;

            /* d_ot is the size of destination */
            d_ot = s->dflag;
            /* ot is the size of source */
            ot = (b & 1) + MO_8;
            /* s_ot is the sign+size of source */
            s_ot = b & 8 ? MO_SIGN | ot : ot;

            modrm = x86_ldub_code(env, s);
            reg = ((modrm >> 3) & 7) | REX_R(s);
            mod = (modrm >> 6) & 3;
            rm = (modrm & 7) | REX_B(s);

            if (mod == 3) {
                if (s_ot == MO_SB && byte_reg_is_xH(s, rm)) {
                    tcg_gen_sextract_tl(s->T0, cpu_regs[rm - 4], 8, 8);
                } else {
                    gen_op_mov_v_reg(s, ot, s->T0, rm);
                    switch (s_ot) {
                    case MO_UB:
                        tcg_gen_ext8u_tl(s->T0, s->T0);
                        break;
                    case MO_SB:
                        tcg_gen_ext8s_tl(s->T0, s->T0);
                        break;
                    case MO_UW:
                        tcg_gen_ext16u_tl(s->T0, s->T0);
                        break;
                    default:
                    case MO_SW:
                        tcg_gen_ext16s_tl(s->T0, s->T0);
                        break;
                    }
                }
                gen_op_mov_reg_v(s, d_ot, reg, s->T0);
            } else {
                gen_lea_modrm(env, s, modrm);
                gen_op_ld_v(s, s_ot, s->T0, s->A0);
                gen_op_mov_reg_v(s, d_ot, reg, s->T0);
            }
        }
        break;

    case 0x8d: /* lea */
        modrm = x86_ldub_code(env, s);
        mod = (modrm >> 6) & 3;
        if (mod == 3)
            goto illegal_op;
        reg = ((modrm >> 3) & 7) | REX_R(s);
        {
            AddressParts a = gen_lea_modrm_0(env, s, modrm);
            TCGv ea = gen_lea_modrm_1(s, a);
            gen_lea_v_seg(s, s->aflag, ea, -1, -1);
            gen_op_mov_reg_v(s, s->dflag, reg, s->A0);
        }
        break;

    case 0xa0: /* mov EAX, Ov */
    case 0xa1:
    case 0xa2: /* mov Ov, EAX */
    case 0xa3:
        {
            target_ulong offset_addr;

            ot = mo_b_d(b, s->dflag);
            switch (s->aflag) {
#ifdef TARGET_X86_64
            case MO_64:
                offset_addr = x86_ldq_code(env, s);
                break;
#endif
            default:
                offset_addr = insn_get(env, s, s->aflag);
                break;
            }
            tcg_gen_movi_tl(s->A0, offset_addr);
            gen_add_A0_ds_seg(s);
            if ((b & 2) == 0) {
                gen_op_ld_v(s, ot, s->T0, s->A0);
                gen_op_mov_reg_v(s, ot, R_EAX, s->T0);
            } else {
                gen_op_mov_v_reg(s, ot, s->T0, R_EAX);
                gen_op_st_v(s, ot, s->T0, s->A0);
            }
        }
        break;
    case 0xd7: /* xlat */
        tcg_gen_mov_tl(s->A0, cpu_regs[R_EBX]);
        tcg_gen_ext8u_tl(s->T0, cpu_regs[R_EAX]);
        tcg_gen_add_tl(s->A0, s->A0, s->T0);
        gen_extu(s->aflag, s->A0);
        gen_add_A0_ds_seg(s);
        gen_op_ld_v(s, MO_8, s->T0, s->A0);
        gen_op_mov_reg_v(s, MO_8, R_EAX, s->T0);
        break;
    case 0xb0 ... 0xb7: /* mov R, Ib */
        val = insn_get(env, s, MO_8);
        tcg_gen_movi_tl(s->T0, val);
        gen_op_mov_reg_v(s, MO_8, (b & 7) | REX_B(s), s->T0);
        break;
    case 0xb8 ... 0xbf: /* mov R, Iv */
#ifdef TARGET_X86_64
        if (s->dflag == MO_64) {
            uint64_t tmp;
            /* 64 bit case */
            tmp = x86_ldq_code(env, s);
            reg = (b & 7) | REX_B(s);
            tcg_gen_movi_tl(s->T0, tmp);
            gen_op_mov_reg_v(s, MO_64, reg, s->T0);
        } else
#endif
        {
            ot = s->dflag;
            val = insn_get(env, s, ot);
            reg = (b & 7) | REX_B(s);
            tcg_gen_movi_tl(s->T0, val);
            gen_op_mov_reg_v(s, ot, reg, s->T0);
        }
        break;

    case 0x91 ... 0x97: /* xchg R, EAX */
    do_xchg_reg_eax:
        ot = s->dflag;
        reg = (b & 7) | REX_B(s);
        rm = R_EAX;
        goto do_xchg_reg;
    case 0x86:
    case 0x87: /* xchg Ev, Gv */
        ot = mo_b_d(b, s->dflag);
        modrm = x86_ldub_code(env, s);
        reg = ((modrm >> 3) & 7) | REX_R(s);
        mod = (modrm >> 6) & 3;
        if (mod == 3) {
            rm = (modrm & 7) | REX_B(s);
        do_xchg_reg:
            gen_op_mov_v_reg(s, ot, s->T0, reg);
            gen_op_mov_v_reg(s, ot, s->T1, rm);
            gen_op_mov_reg_v(s, ot, rm, s->T0);
            gen_op_mov_reg_v(s, ot, reg, s->T1);
        } else {
            gen_lea_modrm(env, s, modrm);
            gen_op_mov_v_reg(s, ot, s->T0, reg);
            /* for xchg, lock is implicit */
            tcg_gen_atomic_xchg_tl(s->T1, s->A0, s->T0,
                                   s->mem_index, ot | MO_LE);
            gen_op_mov_reg_v(s, ot, reg, s->T1);
        }
        break;
    case 0xc4: /* les Gv */
        /* In CODE64 this is VEX3; see above.  */
        op = R_ES;
        goto do_lxx;
    case 0xc5: /* lds Gv */
        /* In CODE64 this is VEX2; see above.  */
        op = R_DS;
        goto do_lxx;
    case 0x1b2: /* lss Gv */
        op = R_SS;
        goto do_lxx;
    case 0x1b4: /* lfs Gv */
        op = R_FS;
        goto do_lxx;
    case 0x1b5: /* lgs Gv */
        op = R_GS;
    do_lxx:
        ot = s->dflag != MO_16 ? MO_32 : MO_16;
        modrm = x86_ldub_code(env, s);
        reg = ((modrm >> 3) & 7) | REX_R(s);
        mod = (modrm >> 6) & 3;
        if (mod == 3)
            goto illegal_op;
        gen_lea_modrm(env, s, modrm);
        gen_op_ld_v(s, ot, s->T1, s->A0);
        gen_add_A0_im(s, 1 << ot);
        /* load the segment first to handle exceptions properly */
        gen_op_ld_v(s, MO_16, s->T0, s->A0);
        gen_movl_seg_T0(s, op);
        /* then put the data */
        gen_op_mov_reg_v(s, ot, reg, s->T1);
        if (s->base.is_jmp) {
            gen_jmp_im(s, s->pc - s->cs_base);
            gen_eob(s);
        }
        break;

        /************************/
        /* shifts */
    case 0xc0:
    case 0xc1:
        /* shift Ev,Ib */
        shift = 2;
    grp2:
        {
            ot = mo_b_d(b, s->dflag);
            modrm = x86_ldub_code(env, s);
            mod = (modrm >> 6) & 3;
            op = (modrm >> 3) & 7;

            if (mod != 3) {
                if (shift == 2) {
                    s->rip_offset = 1;
                }
                gen_lea_modrm(env, s, modrm);
                opreg = OR_TMP0;
            } else {
                opreg = (modrm & 7) | REX_B(s);
            }

            /* simpler op */
            if (shift == 0) {
                gen_shift(s, op, ot, opreg, OR_ECX);
            } else {
                if (shift == 2) {
                    shift = x86_ldub_code(env, s);
                }
                gen_shifti(s, op, ot, opreg, shift);
            }
        }
        break;
    case 0xd0:
    case 0xd1:
        /* shift Ev,1 */
        shift = 1;
        goto grp2;
    case 0xd2:
    case 0xd3:
        /* shift Ev,cl */
        shift = 0;
        goto grp2;

    case 0x1a4: /* shld imm */
        op = 0;
        shift = 1;
        goto do_shiftd;
    case 0x1a5: /* shld cl */
        op = 0;
        shift = 0;
        goto do_shiftd;
    case 0x1ac: /* shrd imm */
        op = 1;
        shift = 1;
        goto do_shiftd;
    case 0x1ad: /* shrd cl */
        op = 1;
        shift = 0;
    do_shiftd:
        ot = s->dflag;
        modrm = x86_ldub_code(env, s);
        mod = (modrm >> 6) & 3;
        rm = (modrm & 7) | REX_B(s);
        reg = ((modrm >> 3) & 7) | REX_R(s);
        if (mod != 3) {
            gen_lea_modrm(env, s, modrm);
            opreg = OR_TMP0;
        } else {
            opreg = rm;
        }
        gen_op_mov_v_reg(s, ot, s->T1, reg);

        if (shift) {
            TCGv imm = tcg_const_tl(x86_ldub_code(env, s));
            gen_shiftd_rm_T1(s, ot, opreg, op, imm);
            tcg_temp_free(imm);
        } else {
            gen_shiftd_rm_T1(s, ot, opreg, op, cpu_regs[R_ECX]);
        }
        break;

        /************************/
        /* floats */
    case 0xd8 ... 0xdf:
        if (s->flags & (HF_EM_MASK | HF_TS_MASK)) {
            /* if CR0.EM or CR0.TS are set, generate an FPU exception */
            /* XXX: what to do if illegal op ? */
            gen_exception(s, EXCP07_PREX);
            break;
        }
        modrm = x86_ldub_code(env, s);
        mod = (modrm >> 6) & 3;
        rm = modrm & 7;
        op = ((b & 7) << 3) | ((modrm >> 3) & 7);
        if (mod != 3) {
            /* memory op */
            gen_lea_modrm(env, s, modrm);
            switch(op) {
            case 0x00 ... 0x07: /* fxxxs */
            case 0x10 ... 0x17: /* fixxxl */
            case 0x20 ... 0x27: /* fxxxl */
            case 0x30 ... 0x37: /* fixxx */
                {
                    int op1;
                    op1 = op & 7;

                    switch(op >> 4) {
                    case 0:
                        tcg_gen_qemu_ld_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LEUL);
                        gen_helper_flds_FT0(cpu_env, s->tmp2_i32);
                        break;
                    case 1:
                        tcg_gen_qemu_ld_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LEUL);
                        gen_helper_fildl_FT0(cpu_env, s->tmp2_i32);
                        break;
                    case 2:
                        tcg_gen_qemu_ld_i64(s->tmp1_i64, s->A0,
                                            s->mem_index, MO_LEQ);
                        gen_helper_fldl_FT0(cpu_env, s->tmp1_i64);
                        break;
                    case 3:
                    default:
                        tcg_gen_qemu_ld_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LESW);
                        gen_helper_fildl_FT0(cpu_env, s->tmp2_i32);
                        break;
                    }

                    gen_helper_fp_arith_ST0_FT0(op1);
                    if (op1 == 3) {
                        /* fcomp needs pop */
                        gen_helper_fpop(cpu_env);
                    }
                }
                break;
            case 0x08: /* flds */
            case 0x0a: /* fsts */
            case 0x0b: /* fstps */
            case 0x18 ... 0x1b: /* fildl, fisttpl, fistl, fistpl */
            case 0x28 ... 0x2b: /* fldl, fisttpll, fstl, fstpl */
            case 0x38 ... 0x3b: /* filds, fisttps, fists, fistps */
                switch(op & 7) {
                case 0:
                    switch(op >> 4) {
                    case 0:
                        tcg_gen_qemu_ld_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LEUL);
                        gen_helper_flds_ST0(cpu_env, s->tmp2_i32);
                        break;
                    case 1:
                        tcg_gen_qemu_ld_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LEUL);
                        gen_helper_fildl_ST0(cpu_env, s->tmp2_i32);
                        break;
                    case 2:
                        tcg_gen_qemu_ld_i64(s->tmp1_i64, s->A0,
                                            s->mem_index, MO_LEQ);
                        gen_helper_fldl_ST0(cpu_env, s->tmp1_i64);
                        break;
                    case 3:
                    default:
                        tcg_gen_qemu_ld_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LESW);
                        gen_helper_fildl_ST0(cpu_env, s->tmp2_i32);
                        break;
                    }
                    break;
                case 1:
                    /* XXX: the corresponding CPUID bit must be tested ! */
                    switch(op >> 4) {
                    case 1:
                        gen_helper_fisttl_ST0(s->tmp2_i32, cpu_env);
                        tcg_gen_qemu_st_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LEUL);
                        break;
                    case 2:
                        gen_helper_fisttll_ST0(s->tmp1_i64, cpu_env);
                        tcg_gen_qemu_st_i64(s->tmp1_i64, s->A0,
                                            s->mem_index, MO_LEQ);
                        break;
                    case 3:
                    default:
                        gen_helper_fistt_ST0(s->tmp2_i32, cpu_env);
                        tcg_gen_qemu_st_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LEUW);
                        break;
                    }
                    gen_helper_fpop(cpu_env);
                    break;
                default:
                    switch(op >> 4) {
                    case 0:
                        gen_helper_fsts_ST0(s->tmp2_i32, cpu_env);
                        tcg_gen_qemu_st_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LEUL);
                        break;
                    case 1:
                        gen_helper_fistl_ST0(s->tmp2_i32, cpu_env);
                        tcg_gen_qemu_st_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LEUL);
                        break;
                    case 2:
                        gen_helper_fstl_ST0(s->tmp1_i64, cpu_env);
                        tcg_gen_qemu_st_i64(s->tmp1_i64, s->A0,
                                            s->mem_index, MO_LEQ);
                        break;
                    case 3:
                    default:
                        gen_helper_fist_ST0(s->tmp2_i32, cpu_env);
                        tcg_gen_qemu_st_i32(s->tmp2_i32, s->A0,
                                            s->mem_index, MO_LEUW);
                        break;
                    }
                    if ((op & 7) == 3)
                        gen_helper_fpop(cpu_env);
                    break;
                }
                break;
            case 0x0c: /* fldenv mem */
                gen_helper_fldenv(cpu_env, s->A0, tcg_const_i32(s->dflag - 1));
                break;
            case 0x0d: /* fldcw mem */
                tcg_gen_qemu_ld_i32(s->tmp2_i32, s->A0,
                                    s->mem_index, MO_LEUW);
                gen_helper_fldcw(cpu_env, s->tmp2_i32);
                break;
            case 0x0e: /* fnstenv mem */
                gen_helper_fstenv(cpu_env, s->A0, tcg_const_i32(s->dflag - 1));
                break;
            case 0x0f: /* fnstcw mem */
                gen_helper_fnstcw(s->tmp2_i32, cpu_env);
                tcg_gen_qemu_st_i32(s->tmp2_i32, s->A0,
                                    s->mem_index, MO_LEUW);
                break;
            case 0x1d: /* fldt mem */
                gen_helper_fldt_ST0(cpu_env, s->A0);
                break;
            case 0x1f: /* fstpt mem */
                gen_helper_fstt_ST0(cpu_env, s->A0);
                gen_helper_fpop(cpu_env);
                break;
            case 0x2c: /* frstor mem */
                gen_helper_frstor(cpu_env, s->A0, tcg_const_i32(s->dflag - 1));
                break;
            case 0x2e: /* fnsave mem */
                gen_helper_fsave(cpu_env, s->A0, tcg_const_i32(s->dflag - 1));
                break;
            case 0x2f: /* fnstsw mem */
                gen_helper_fnstsw(s->tmp2_i32, cpu_env);
                tcg_gen_qemu_st_i32(s->tmp2_i32, s->A0,
                                    s->mem_index, MO_LEUW);
                break;
            case 0x3c: /* fbld */
                gen_helper_fbld_ST0(cpu_env, s->A0);
                break;
            case 0x3e: /* fbstp */
                gen_helper_fbst_ST0(cpu_env, s->A0);
                gen_helper_fpop(cpu_env);
                break;
            case 0x3d: /* fildll */
                tcg_gen_qemu_ld_i64(s->tmp1_i64, s->A0, s->mem_index, MO_LEQ);
                gen_helper_fildll_ST0(cpu_env, s->tmp1_i64);
                break;
            case 0x3f: /* fistpll */
                gen_helper_fistll_ST0(s->tmp1_i64, cpu_env);
                tcg_gen_qemu_st_i64(s->tmp1_i64, s->A0, s->mem_index, MO_LEQ);
                gen_helper_fpop(cpu_env);
                break;
            default:
                goto unknown_op;
            }
        } else {
            /* register float ops */
            opreg = rm;

            switch(op) {
            case 0x08: /* fld sti */
                gen_helper_fpush(cpu_env);
                gen_helper_fmov_ST0_STN(cpu_env,
                                        tcg_const_i32((opreg + 1) & 7));
                break;
            case 0x09: /* fxchg sti */
            case 0x29: /* fxchg4 sti, undocumented op */
            case 0x39: /* fxchg7 sti, undocumented op */
                gen_helper_fxchg_ST0_STN(cpu_env, tcg_const_i32(opreg));
                break;
            case 0x0a: /* grp d9/2 */
                switch(rm) {
                case 0: /* fnop */
                    /* check exceptions (FreeBSD FPU probe) */
                    gen_helper_fwait(cpu_env);
                    break;
                default:
                    goto unknown_op;
                }
                break;
            case 0x0c: /* grp d9/4 */
                switch(rm) {
                case 0: /* fchs */
                    gen_helper_fchs_ST0(cpu_env);
                    break;
                case 1: /* fabs */
                    gen_helper_fabs_ST0(cpu_env);
                    break;
                case 4: /* ftst */
                    gen_helper_fldz_FT0(cpu_env);
                    gen_helper_fcom_ST0_FT0(cpu_env);
                    break;
                case 5: /* fxam */
                    gen_helper_fxam_ST0(cpu_env);
                    break;
                default:
                    goto unknown_op;
                }
                break;
            case 0x0d: /* grp d9/5 */
                {
                    switch(rm) {
                    case 0:
                        gen_helper_fpush(cpu_env);
                        gen_helper_fld1_ST0(cpu_env);
                        break;
                    case 1:
                        gen_helper_fpush(cpu_env);
                        gen_helper_fldl2t_ST0(cpu_env);
                        break;
                    case 2:
                        gen_helper_fpush(cpu_env);
                        gen_helper_fldl2e_ST0(cpu_env);
                        break;
                    case 3:
                        gen_helper_fpush(cpu_env);
                        gen_helper_fldpi_ST0(cpu_env);
                        break;
                    case 4:
                        gen_helper_fpush(cpu_env);
                        gen_helper_fldlg2_ST0(cpu_env);
                        break;
                    case 5:
                        gen_helper_fpush(cpu_env);
                        gen_helper_fldln2_ST0(cpu_env);
                        break;
                    case 6:
                        gen_helper_fpush(cpu_env);
                        gen_helper_fldz_ST0(cpu_env);
                        break;
                    default:
                        goto unknown_op;
                    }
                }
                break;
            case 0x0e: /* grp d9/6 */
                switch(rm) {
                case 0: /* f2xm1 */
                    gen_helper_f2xm1(cpu_env);
                    break;
                case 1: /* fyl2x */
                    gen_helper_fyl2x(cpu_env);
                    break;
                case 2: /* fptan */
                    gen_helper_fptan(cpu_env);
                    break;
                case 3: /* fpatan */
                    gen_helper_fpatan(cpu_env);
                    break;
                case 4: /* fxtract */
                    gen_helper_fxtract(cpu_env);
                    break;
                case 5: /* fprem1 */
                    gen_helper_fprem1(cpu_env);
                    break;
                case 6: /* fdecstp */
                    gen_helper_fdecstp(cpu_env);
                    break;
                default:
                case 7: /* fincstp */
                    gen_helper_fincstp(cpu_env);
                    break;
                }
                break;
            case 0x0f: /* grp d9/7 */
                switch(rm) {
                case 0: /* fprem */
                    gen_helper_fprem(cpu_env);
                    break;
                case 1: /* fyl2xp1 */
                    gen_helper_fyl2xp1(cpu_env);
                    break;
                case 2: /* fsqrt */
                    gen_helper_fsqrt(cpu_env);
                    break;
                case 3: /* fsincos */
                    gen_helper_fsincos(cpu_env);
                    break;
                case 5: /* fscale */
                    gen_helper_fscale(cpu_env);
                    break;
                case 4: /* frndint */
                    gen_helper_frndint(cpu_env);
                    break;
                case 6: /* fsin */
                    gen_helper_fsin(cpu_env);
                    break;
                default:
                case 7: /* fcos */
                    gen_helper_fcos(cpu_env);
                    break;
                }
                break;
            case 0x00: case 0x01: case 0x04 ... 0x07: /* fxxx st, sti */
            case 0x20: case 0x21: case 0x24 ... 0x27: /* fxxx sti, st */
            case 0x30: case 0x31: case 0x34 ... 0x37: /* fxxxp sti, st */
                {
                    int op1;

                    op1 = op & 7;
                    if (op >= 0x20) {
                        gen_helper_fp_arith_STN_ST0(op1, opreg);
                        if (op >= 0x30)
                            gen_helper_fpop(cpu_env);
                    } else {
                        gen_helper_fmov_FT0_STN(cpu_env, tcg_const_i32(opreg));
                        gen_helper_fp_arith_ST0_FT0(op1);
                    }
                }
                break;
            case 0x02: /* fcom */
            case 0x22: /* fcom2, undocumented op */
                gen_helper_fmov_FT0_STN(cpu_env, tcg_const_i32(opreg));
                gen_helper_fcom_ST0_FT0(cpu_env);
                break;
            case 0x03: /* fcomp */
            case 0x23: /* fcomp3, undocumented op */
            case 0x32: /* fcomp5, undocumented op */
                gen_helper_fmov_FT0_STN(cpu_env, tcg_const_i32(opreg));
                gen_helper_fcom_ST0_FT0(cpu_env);
                gen_helper_fpop(cpu_env);
                break;
            case 0x15: /* da/5 */
                switch(rm) {
                case 1: /* fucompp */
                    gen_helper_fmov_FT0_STN(cpu_env, tcg_const_i32(1));
                    gen_helper_fucom_ST0_FT0(cpu_env);
                    gen_helper_fpop(cpu_env);
                    gen_helper_fpop(cpu_env);
                    break;
                default:
                    goto unknown_op;
                }
                break;
            case 0x1c:
                switch(rm) {
                case 0: /* feni (287 only, just do nop here) */
                    break;
                case 1: /* fdisi (287 only, just do nop here) */
                    break;
                case 2: /* fclex */
                    gen_helper_fclex(cpu_env);
                    break;
                case 3: /* fninit */
                    gen_helper_fninit(cpu_env);
                    break;
                case 4: /* fsetpm (287 only, just do nop here) */
                    break;
                default:
                    goto unknown_op;
                }
                break;
            case 0x1d: /* fucomi */
                if (!(s->cpuid_features & CPUID_CMOV)) {
                    goto illegal_op;
                }
                gen_update_cc_op(s);
                gen_helper_fmov_FT0_STN(cpu_env, tcg_const_i32(opreg));
                gen_helper_fucomi_ST0_FT0(cpu_env);
                set_cc_op(s, CC_OP_EFLAGS);
                break;
            case 0x1e: /* fcomi */
                if (!(s->cpuid_features & CPUID_CMOV)) {
                    goto illegal_op;
                }
                gen_update_cc_op(s);
                gen_helper_fmov_FT0_STN(cpu_env, tcg_const_i32(opreg));
                gen_helper_fcomi_ST0_FT0(cpu_env);
                set_cc_op(s, CC_OP_EFLAGS);
                break;
            case 0x28: /* ffree sti */
                gen_helper_ffree_STN(cpu_env, tcg_const_i32(opreg));
                break;
            case 0x2a: /* fst sti */
                gen_helper_fmov_STN_ST0(cpu_env, tcg_const_i32(opreg));
                break;
            case 0x2b: /* fstp sti */
            case 0x0b: /* fstp1 sti, undocumented op */
            case 0x3a: /* fstp8 sti, undocumented op */
            case 0x3b: /* fstp9 sti, undocumented op */
                gen_helper_fmov_STN_ST0(cpu_env, tcg_const_i32(opreg));
                gen_helper_fpop(cpu_env);
                break;
            case 0x2c: /* fucom st(i) */
                gen_helper_fmov_FT0_STN(cpu_env, tcg_const_i32(opreg));
                gen_helper_fucom_ST0_FT0(cpu_env);
                break;
            case 0x2d: /* fucomp st(i) */
                gen_helper_fmov_FT0_STN(cpu_env, tcg_const_i32(opreg));
                gen_helper_fucom_ST0_FT0(cpu_env);
                gen_helper_fpop(cpu_env);
                break;
            case 0x33: /* de/3 */
                switch(rm) {
                case 1: /* fcompp */
                    gen_helper_fmov_FT0_STN(cpu_env, tcg_const_i32(1));
                    gen_helper_fcom_ST0_FT0(cpu_env);
                    gen_helper_fpop(cpu_env);
                    gen_helper_fpop(cpu_env);
                    break;
                default:
                    goto unknown_op;
                }
                break;
            case 0x38: /* ffreep sti, undocumented op */
                gen_helper_ffree_STN(cpu_env, tcg_const_i32(opreg));
                gen_helper_fpop(cpu_env);
                break;
            case 0x3c: /* df/4 */
                switch(rm) {
                case 0:
                    gen_helper_fnstsw(s->tmp2_i32, cpu_env);
                    tcg_gen_extu_i32_tl(s->T0, s->tmp2_i32);
                    gen_op_mov_reg_v(s, MO_16, R_EAX, s->T0);
                    break;
                default:
                    goto unknown_op;
                }
                break;
            case 0x3d: /* fucomip */
                if (!(s->cpuid_features & CPUID_CMOV)) {
                    goto illegal_op;
                }
                gen_update_cc_op(s);
                gen_helper_fmov_FT0_STN(cpu_env, tcg_const_i32(opreg));
                gen_helper_fucomi_ST0_FT0(cpu_env);
                gen_helper_fpop(cpu_env);
                set_cc_op(s, CC_OP_EFLAGS);
                break;
            case 0x3e: /* fcomip */
                if (!(s->cpuid_features & CPUID_CMOV)) {
                    goto illegal_op;
                }
                gen_update_cc_op(s);
                gen_helper_fmov_FT0_STN(cpu_env, tcg_const_i32(opreg));
                gen_helper_fcomi_ST0_FT0(cpu_env);
                gen_helper_fpop(cpu_env);
                set_cc_op(s, CC_OP_EFLAGS);
                break;
            case 0x10 ... 0x13: /* fcmovxx */
            case 0x18 ... 0x1b:
                {
                    int op1;
                    TCGLabel *l1;
                    static const uint8_t fcmov_cc[8] = {
                        (JCC_B << 1),
                        (JCC_Z << 1),
                        (JCC_BE << 1),
                        (JCC_P << 1),
                    };

                    if (!(s->cpuid_features & CPUID_CMOV)) {
                        goto illegal_op;
                    }
                    op1 = fcmov_cc[op & 3] | (((op >> 3) & 1) ^ 1);
                    l1 = gen_new_label();
                    gen_jcc1_noeob(s, op1, l1);
                    gen_helper_fmov_ST0_STN(cpu_env, tcg_const_i32(opreg));
                    gen_set_label(l1);
                }
                break;
            default:
                goto unknown_op;
            }
        }
        break;
        /************************/
        /* string ops */

    case 0xa4: /* movsS */
    case 0xa5:
        ot = mo_b_d(b, s->dflag);
        if (s->prefix & (PREFIX_REPZ | PREFIX_REPNZ)) {
            gen_repz_movs(s, ot, s->pc_start - s->cs_base, s->pc - s->cs_base);
        } else {
            gen_movs(s, ot);
        }
        break;

    case 0xaa: /* stosS */
    case 0xab:
        ot = mo_b_d(b, s->dflag);
        if (s->prefix & (PREFIX_REPZ | PREFIX_REPNZ)) {
            gen_repz_stos(s, ot, s->pc_start - s->cs_base, s->pc - s->cs_base);
        } else {
            gen_stos(s, ot);
        }
        break;
    case 0xac: /* lodsS */
    case 0xad:
        ot = mo_b_d(b, s->dflag);
        if (s->prefix & (PREFIX_REPZ | PREFIX_REPNZ)) {
            gen_repz_lods(s, ot, s->pc_start - s->cs_base, s->pc - s->cs_base);
        } else {
            gen_lods(s, ot);
        }
        break;
    case 0xae: /* scasS */
    case 0xaf:
        ot = mo_b_d(b, s->dflag);
        if (s->prefix & PREFIX_REPNZ) {
            gen_repz_scas(s, ot, s->pc_start - s->cs_base, s->pc - s->cs_base, 1);
        } else if (s->prefix & PREFIX_REPZ) {
            gen_repz_scas(s, ot, s->pc_start - s->cs_base, s->pc - s->cs_base, 0);
        } else {
            gen_scas(s, ot);
        }
        break;

    case 0xa6: /* cmpsS */
    case 0xa7:
        ot = mo_b_d(b, s->dflag);
        if (s->prefix & PREFIX_REPNZ) {
            gen_repz_cmps(s, ot, s->pc_start - s->cs_base, s->pc - s->cs_base, 1);
        } else if (s->prefix & PREFIX_REPZ) {
            gen_repz_cmps(s, ot, s->pc_start - s->cs_base, s->pc - s->cs_base, 0);
        } else {
            gen_cmps(s, ot);
        }
        break;
    case 0x6c: /* insS */
    case 0x6d:
        ot = mo_b_d32(b, s->dflag);
        tcg_gen_ext16u_tl(s->T0, cpu_regs[R_EDX]);
        gen_check_io(s, ot, s->pc_start - s->cs_base,
                     SVM_IOIO_TYPE_MASK | svm_is_rep(s->prefix) | 4);
        if (s->prefix & (PREFIX_REPZ | PREFIX_REPNZ)) {
            gen_repz_ins(s, ot, s->pc_start - s->cs_base, s->pc - s->cs_base);
        } else {
            gen_ins(s, ot);
            if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
                gen_jmp(s, s->pc - s->cs_base);
            }
        }
        break;
    case 0x6e: /* outsS */
    case 0x6f:
        ot = mo_b_d32(b, s->dflag);
        tcg_gen_ext16u_tl(s->T0, cpu_regs[R_EDX]);
        gen_check_io(s, ot, s->pc_start - s->cs_base,
                     svm_is_rep(s->prefix) | 4);
        if (s->prefix & (PREFIX_REPZ | PREFIX_REPNZ)) {
            gen_repz_outs(s, ot, s->pc_start - s->cs_base, s->pc - s->cs_base);
        } else {
            gen_outs(s, ot);
            if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
                gen_jmp(s, s->pc - s->cs_base);
            }
        }
        break;

        /************************/
        /* port I/O */

    case 0xe4:
    case 0xe5:
        ot = mo_b_d32(b, s->dflag);
        val = x86_ldub_code(env, s);
        tcg_gen_movi_tl(s->T0, val);
        gen_check_io(s, ot, s->pc_start - s->cs_base,
                     SVM_IOIO_TYPE_MASK | svm_is_rep(s->prefix));
        if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
            gen_io_start();
        }
        tcg_gen_movi_i32(s->tmp2_i32, val);
        gen_helper_in_func(ot, s->T1, s->tmp2_i32);
        gen_op_mov_reg_v(s, ot, R_EAX, s->T1);
        gen_bpt_io(s, s->tmp2_i32, ot);
        if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
            gen_jmp(s, s->pc - s->cs_base);
        }
        break;
    case 0xe6:
    case 0xe7:
        ot = mo_b_d32(b, s->dflag);
        val = x86_ldub_code(env, s);
        tcg_gen_movi_tl(s->T0, val);
        gen_check_io(s, ot, s->pc_start - s->cs_base,
                     svm_is_rep(s->prefix));
        gen_op_mov_v_reg(s, ot, s->T1, R_EAX);

        if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
            gen_io_start();
        }
        tcg_gen_movi_i32(s->tmp2_i32, val);
        tcg_gen_trunc_tl_i32(s->tmp3_i32, s->T1);
        gen_helper_out_func(ot, s->tmp2_i32, s->tmp3_i32);
        gen_bpt_io(s, s->tmp2_i32, ot);
        if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
            gen_jmp(s, s->pc - s->cs_base);
        }
        break;
    case 0xec:
    case 0xed:
        ot = mo_b_d32(b, s->dflag);
        tcg_gen_ext16u_tl(s->T0, cpu_regs[R_EDX]);
        gen_check_io(s, ot, s->pc_start - s->cs_base,
                     SVM_IOIO_TYPE_MASK | svm_is_rep(s->prefix));
        if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
            gen_io_start();
        }
        tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
        gen_helper_in_func(ot, s->T1, s->tmp2_i32);
        gen_op_mov_reg_v(s, ot, R_EAX, s->T1);
        gen_bpt_io(s, s->tmp2_i32, ot);
        if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
            gen_jmp(s, s->pc - s->cs_base);
        }
        break;
    case 0xee:
    case 0xef:
        ot = mo_b_d32(b, s->dflag);
        tcg_gen_ext16u_tl(s->T0, cpu_regs[R_EDX]);
        gen_check_io(s, ot, s->pc_start - s->cs_base,
                     svm_is_rep(s->prefix));
        gen_op_mov_v_reg(s, ot, s->T1, R_EAX);

        if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
            gen_io_start();
        }
        tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
        tcg_gen_trunc_tl_i32(s->tmp3_i32, s->T1);
        gen_helper_out_func(ot, s->tmp2_i32, s->tmp3_i32);
        gen_bpt_io(s, s->tmp2_i32, ot);
        if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
            gen_jmp(s, s->pc - s->cs_base);
        }
        break;

        /************************/
        /* control */
    case 0xc2: /* ret im */
        val = x86_ldsw_code(env, s);
        ot = gen_pop_T0(s);
        gen_stack_update(s, val + (1 << ot));
        /* Note that gen_pop_T0 uses a zero-extending load.  */
        gen_op_jmp_v(s->T0);
        gen_bnd_jmp(s);
        gen_jr(s, s->T0);
        break;
    case 0xc3: /* ret */
        ot = gen_pop_T0(s);
        gen_pop_update(s, ot);
        /* Note that gen_pop_T0 uses a zero-extending load.  */
        gen_op_jmp_v(s->T0);
        gen_bnd_jmp(s);
        gen_jr(s, s->T0);
        break;
    case 0xca: /* lret im */
        val = x86_ldsw_code(env, s);
    do_lret:
        if (s->pe && !s->vm86) {
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            gen_helper_lret_protected(cpu_env, tcg_const_i32(s->dflag - 1),
                                      tcg_const_i32(val));
        } else {
            gen_stack_A0(s);
            /* pop offset */
            gen_op_ld_v(s, s->dflag, s->T0, s->A0);
            /* NOTE: keeping EIP updated is not a problem in case of
               exception */
            gen_op_jmp_v(s->T0);
            /* pop selector */
            gen_add_A0_im(s, 1 << s->dflag);
            gen_op_ld_v(s, s->dflag, s->T0, s->A0);
            gen_op_movl_seg_T0_vm(s, R_CS);
            /* add stack offset */
            gen_stack_update(s, val + (2 << s->dflag));
        }
        gen_eob(s);
        break;
    case 0xcb: /* lret */
        val = 0;
        goto do_lret;
    case 0xcf: /* iret */
        gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_IRET);
        if (!s->pe) {
            /* real mode */
            gen_helper_iret_real(cpu_env, tcg_const_i32(s->dflag - 1));
            set_cc_op(s, CC_OP_EFLAGS);
        } else if (s->vm86) {
            if (s->iopl != 3) {
                gen_exception(s, EXCP0D_GPF);
            } else {
                gen_helper_iret_real(cpu_env, tcg_const_i32(s->dflag - 1));
                set_cc_op(s, CC_OP_EFLAGS);
            }
        } else {
            gen_helper_iret_protected(cpu_env, tcg_const_i32(s->dflag - 1),
                                      tcg_const_i32(s->pc - s->cs_base));
            set_cc_op(s, CC_OP_EFLAGS);
        }
        gen_eob(s);
        break;
    case 0xe8: /* call im */
        {
            if (s->dflag != MO_16) {
                tval = (int32_t)insn_get(env, s, MO_32);
            } else {
                tval = (int16_t)insn_get(env, s, MO_16);
            }
            next_eip = s->pc - s->cs_base;
            tval += next_eip;
            if (s->dflag == MO_16) {
                tval &= 0xffff;
            } else if (!CODE64(s)) {
                tval &= 0xffffffff;
            }
            tcg_gen_movi_tl(s->T0, next_eip);
            gen_push_v(s, s->T0);
            gen_bnd_jmp(s);
            gen_jmp(s, tval);
        }
        break;
    case 0x9a: /* lcall im */
        {
            unsigned int selector, offset;

            if (CODE64(s))
                goto illegal_op;
            ot = s->dflag;
            offset = insn_get(env, s, ot);
            selector = insn_get(env, s, MO_16);

            tcg_gen_movi_tl(s->T0, selector);
            tcg_gen_movi_tl(s->T1, offset);
        }
        goto do_lcall;
    case 0xe9: /* jmp im */
        if (s->dflag != MO_16) {
            tval = (int32_t)insn_get(env, s, MO_32);
        } else {
            tval = (int16_t)insn_get(env, s, MO_16);
        }
        tval += s->pc - s->cs_base;
        if (s->dflag == MO_16) {
            tval &= 0xffff;
        } else if (!CODE64(s)) {
            tval &= 0xffffffff;
        }
        gen_bnd_jmp(s);
        gen_jmp(s, tval);
        break;
    case 0xea: /* ljmp im */
        {
            unsigned int selector, offset;

            if (CODE64(s))
                goto illegal_op;
            ot = s->dflag;
            offset = insn_get(env, s, ot);
            selector = insn_get(env, s, MO_16);

            tcg_gen_movi_tl(s->T0, selector);
            tcg_gen_movi_tl(s->T1, offset);
        }
        goto do_ljmp;
    case 0xeb: /* jmp Jb */
        tval = (int8_t)insn_get(env, s, MO_8);
        tval += s->pc - s->cs_base;
        if (s->dflag == MO_16) {
            tval &= 0xffff;
        }
        gen_jmp(s, tval);
        break;
    case 0x70 ... 0x7f: /* jcc Jb */
        tval = (int8_t)insn_get(env, s, MO_8);
        goto do_jcc;
    case 0x180 ... 0x18f: /* jcc Jv */
        if (s->dflag != MO_16) {
            tval = (int32_t)insn_get(env, s, MO_32);
        } else {
            tval = (int16_t)insn_get(env, s, MO_16);
        }
    do_jcc:
        next_eip = s->pc - s->cs_base;
        tval += next_eip;
        if (s->dflag == MO_16) {
            tval &= 0xffff;
        }
        gen_bnd_jmp(s);
        gen_jcc(s, b, tval, next_eip);
        break;

    case 0x190 ... 0x19f: /* setcc Gv */
        modrm = x86_ldub_code(env, s);
        gen_setcc1(s, b, s->T0);
        gen_ldst_modrm(env, s, modrm, MO_8, OR_TMP0, 1);
        break;
    case 0x140 ... 0x14f: /* cmov Gv, Ev */
        if (!(s->cpuid_features & CPUID_CMOV)) {
            goto illegal_op;
        }
        ot = s->dflag;
        modrm = x86_ldub_code(env, s);
        reg = ((modrm >> 3) & 7) | REX_R(s);
        gen_cmovcc1(env, s, ot, b, modrm, reg);
        break;

        /************************/
        /* flags */
    case 0x9c: /* pushf */
        gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_PUSHF);
        if (s->vm86 && s->iopl != 3) {
            gen_exception(s, EXCP0D_GPF);
        } else {
            gen_update_cc_op(s);
            gen_helper_read_eflags(s->T0, cpu_env);
            gen_push_v(s, s->T0);
        }
        break;
    case 0x9d: /* popf */
        gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_POPF);
        if (s->vm86 && s->iopl != 3) {
            gen_exception(s, EXCP0D_GPF);
        } else {
            ot = gen_pop_T0(s);
            if (s->cpl == 0) {
                if (s->dflag != MO_16) {
                    gen_helper_write_eflags(cpu_env, s->T0,
                                            tcg_const_i32((TF_MASK | AC_MASK |
                                                           ID_MASK | NT_MASK |
                                                           IF_MASK |
                                                           IOPL_MASK)));
                } else {
                    gen_helper_write_eflags(cpu_env, s->T0,
                                            tcg_const_i32((TF_MASK | AC_MASK |
                                                           ID_MASK | NT_MASK |
                                                           IF_MASK | IOPL_MASK)
                                                          & 0xffff));
                }
            } else {
                if (s->cpl <= s->iopl) {
                    if (s->dflag != MO_16) {
                        gen_helper_write_eflags(cpu_env, s->T0,
                                                tcg_const_i32((TF_MASK |
                                                               AC_MASK |
                                                               ID_MASK |
                                                               NT_MASK |
                                                               IF_MASK)));
                    } else {
                        gen_helper_write_eflags(cpu_env, s->T0,
                                                tcg_const_i32((TF_MASK |
                                                               AC_MASK |
                                                               ID_MASK |
                                                               NT_MASK |
                                                               IF_MASK)
                                                              & 0xffff));
                    }
                } else {
                    if (s->dflag != MO_16) {
                        gen_helper_write_eflags(cpu_env, s->T0,
                                           tcg_const_i32((TF_MASK | AC_MASK |
                                                          ID_MASK | NT_MASK)));
                    } else {
                        gen_helper_write_eflags(cpu_env, s->T0,
                                           tcg_const_i32((TF_MASK | AC_MASK |
                                                          ID_MASK | NT_MASK)
                                                         & 0xffff));
                    }
                }
            }
            gen_pop_update(s, ot);
            set_cc_op(s, CC_OP_EFLAGS);
            /* abort translation because TF/AC flag may change */
            gen_jmp_im(s, s->pc - s->cs_base);
            gen_eob(s);
        }
        break;
    case 0x9e: /* sahf */
        if (CODE64(s) && !(s->cpuid_ext3_features & CPUID_EXT3_LAHF_LM))
            goto illegal_op;
        gen_op_mov_v_reg(s, MO_8, s->T0, R_AH);
        gen_compute_eflags(s);
        tcg_gen_andi_tl(cpu_cc_src, cpu_cc_src, CC_O);
        tcg_gen_andi_tl(s->T0, s->T0, CC_S | CC_Z | CC_A | CC_P | CC_C);
        tcg_gen_or_tl(cpu_cc_src, cpu_cc_src, s->T0);
        break;
    case 0x9f: /* lahf */
        if (CODE64(s) && !(s->cpuid_ext3_features & CPUID_EXT3_LAHF_LM))
            goto illegal_op;
        gen_compute_eflags(s);
        /* Note: gen_compute_eflags() only gives the condition codes */
        tcg_gen_ori_tl(s->T0, cpu_cc_src, 0x02);
        gen_op_mov_reg_v(s, MO_8, R_AH, s->T0);
        break;
    case 0xf5: /* cmc */
        gen_compute_eflags(s);
        tcg_gen_xori_tl(cpu_cc_src, cpu_cc_src, CC_C);
        break;
    case 0xf8: /* clc */
        gen_compute_eflags(s);
        tcg_gen_andi_tl(cpu_cc_src, cpu_cc_src, ~CC_C);
        break;
    case 0xf9: /* stc */
        gen_compute_eflags(s);
        tcg_gen_ori_tl(cpu_cc_src, cpu_cc_src, CC_C);
        break;
    case 0xfc: /* cld */
        tcg_gen_movi_i32(s->tmp2_i32, 1);
        tcg_gen_st_i32(s->tmp2_i32, cpu_env, offsetof(CPUX86State, df));
        break;
    case 0xfd: /* std */
        tcg_gen_movi_i32(s->tmp2_i32, -1);
        tcg_gen_st_i32(s->tmp2_i32, cpu_env, offsetof(CPUX86State, df));
        break;

        /************************/
        /* bit operations */
    case 0x1ba: /* bt/bts/btr/btc Gv, im */
        ot = s->dflag;
        modrm = x86_ldub_code(env, s);
        op = (modrm >> 3) & 7;
        mod = (modrm >> 6) & 3;
        rm = (modrm & 7) | REX_B(s);
        if (mod != 3) {
            s->rip_offset = 1;
            gen_lea_modrm(env, s, modrm);
            if (!(s->prefix & PREFIX_LOCK)) {
                gen_op_ld_v(s, ot, s->T0, s->A0);
            }
        } else {
            gen_op_mov_v_reg(s, ot, s->T0, rm);
        }
        /* load shift */
        val = x86_ldub_code(env, s);
        tcg_gen_movi_tl(s->T1, val);
        if (op < 4)
            goto unknown_op;
        op -= 4;
        goto bt_op;
    case 0x1a3: /* bt Gv, Ev */
        op = 0;
        goto do_btx;
    case 0x1ab: /* bts */
        op = 1;
        goto do_btx;
    case 0x1b3: /* btr */
        op = 2;
        goto do_btx;
    case 0x1bb: /* btc */
        op = 3;
    do_btx:
        ot = s->dflag;
        modrm = x86_ldub_code(env, s);
        reg = ((modrm >> 3) & 7) | REX_R(s);
        mod = (modrm >> 6) & 3;
        rm = (modrm & 7) | REX_B(s);
        gen_op_mov_v_reg(s, MO_32, s->T1, reg);
        if (mod != 3) {
            AddressParts a = gen_lea_modrm_0(env, s, modrm);
            /* specific case: we need to add a displacement */
            gen_exts(ot, s->T1);
            tcg_gen_sari_tl(s->tmp0, s->T1, 3 + ot);
            tcg_gen_shli_tl(s->tmp0, s->tmp0, ot);
            tcg_gen_add_tl(s->A0, gen_lea_modrm_1(s, a), s->tmp0);
            gen_lea_v_seg(s, s->aflag, s->A0, a.def_seg, s->override);
            if (!(s->prefix & PREFIX_LOCK)) {
                gen_op_ld_v(s, ot, s->T0, s->A0);
            }
        } else {
            gen_op_mov_v_reg(s, ot, s->T0, rm);
        }
    bt_op:
        tcg_gen_andi_tl(s->T1, s->T1, (1 << (3 + ot)) - 1);
        tcg_gen_movi_tl(s->tmp0, 1);
        tcg_gen_shl_tl(s->tmp0, s->tmp0, s->T1);
        if (s->prefix & PREFIX_LOCK) {
            switch (op) {
            case 0: /* bt */
                /* Needs no atomic ops; we surpressed the normal
                   memory load for LOCK above so do it now.  */
                gen_op_ld_v(s, ot, s->T0, s->A0);
                break;
            case 1: /* bts */
                tcg_gen_atomic_fetch_or_tl(s->T0, s->A0, s->tmp0,
                                           s->mem_index, ot | MO_LE);
                break;
            case 2: /* btr */
                tcg_gen_not_tl(s->tmp0, s->tmp0);
                tcg_gen_atomic_fetch_and_tl(s->T0, s->A0, s->tmp0,
                                            s->mem_index, ot | MO_LE);
                break;
            default:
            case 3: /* btc */
                tcg_gen_atomic_fetch_xor_tl(s->T0, s->A0, s->tmp0,
                                            s->mem_index, ot | MO_LE);
                break;
            }
            tcg_gen_shr_tl(s->tmp4, s->T0, s->T1);
        } else {
            tcg_gen_shr_tl(s->tmp4, s->T0, s->T1);
            switch (op) {
            case 0: /* bt */
                /* Data already loaded; nothing to do.  */
                break;
            case 1: /* bts */
                tcg_gen_or_tl(s->T0, s->T0, s->tmp0);
                break;
            case 2: /* btr */
                tcg_gen_andc_tl(s->T0, s->T0, s->tmp0);
                break;
            default:
            case 3: /* btc */
                tcg_gen_xor_tl(s->T0, s->T0, s->tmp0);
                break;
            }
            if (op != 0) {
                if (mod != 3) {
                    gen_op_st_v(s, ot, s->T0, s->A0);
                } else {
                    gen_op_mov_reg_v(s, ot, rm, s->T0);
                }
            }
        }

        /* Delay all CC updates until after the store above.  Note that
           C is the result of the test, Z is unchanged, and the others
           are all undefined.  */
        switch (s->cc_op) {
        case CC_OP_MULB ... CC_OP_MULQ:
        case CC_OP_ADDB ... CC_OP_ADDQ:
        case CC_OP_ADCB ... CC_OP_ADCQ:
        case CC_OP_SUBB ... CC_OP_SUBQ:
        case CC_OP_SBBB ... CC_OP_SBBQ:
        case CC_OP_LOGICB ... CC_OP_LOGICQ:
        case CC_OP_INCB ... CC_OP_INCQ:
        case CC_OP_DECB ... CC_OP_DECQ:
        case CC_OP_SHLB ... CC_OP_SHLQ:
        case CC_OP_SARB ... CC_OP_SARQ:
        case CC_OP_BMILGB ... CC_OP_BMILGQ:
            /* Z was going to be computed from the non-zero status of CC_DST.
               We can get that same Z value (and the new C value) by leaving
               CC_DST alone, setting CC_SRC, and using a CC_OP_SAR of the
               same width.  */
            tcg_gen_mov_tl(cpu_cc_src, s->tmp4);
            set_cc_op(s, ((s->cc_op - CC_OP_MULB) & 3) + CC_OP_SARB);
            break;
        default:
            /* Otherwise, generate EFLAGS and replace the C bit.  */
            gen_compute_eflags(s);
            tcg_gen_deposit_tl(cpu_cc_src, cpu_cc_src, s->tmp4,
                               ctz32(CC_C), 1);
            break;
        }
        break;
    case 0x1bc: /* bsf / tzcnt */
    case 0x1bd: /* bsr / lzcnt */
        ot = s->dflag;
        modrm = x86_ldub_code(env, s);
        reg = ((modrm >> 3) & 7) | REX_R(s);
        gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
        gen_extu(ot, s->T0);

        /* Note that lzcnt and tzcnt are in different extensions.  */
        if ((s->prefix & PREFIX_REPZ)
            && (b & 1
                ? s->cpuid_ext3_features & CPUID_EXT3_ABM
                : s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_BMI1)) {
            int size = 8 << ot;
            /* For lzcnt/tzcnt, C bit is defined related to the input. */
            tcg_gen_mov_tl(cpu_cc_src, s->T0);
            if (b & 1) {
                /* For lzcnt, reduce the target_ulong result by the
                   number of zeros that we expect to find at the top.  */
                tcg_gen_clzi_tl(s->T0, s->T0, TARGET_LONG_BITS);
                tcg_gen_subi_tl(s->T0, s->T0, TARGET_LONG_BITS - size);
            } else {
                /* For tzcnt, a zero input must return the operand size.  */
                tcg_gen_ctzi_tl(s->T0, s->T0, size);
            }
            /* For lzcnt/tzcnt, Z bit is defined related to the result.  */
            gen_op_update1_cc(s);
            set_cc_op(s, CC_OP_BMILGB + ot);
        } else {
            /* For bsr/bsf, only the Z bit is defined and it is related
               to the input and not the result.  */
            tcg_gen_mov_tl(cpu_cc_dst, s->T0);
            set_cc_op(s, CC_OP_LOGICB + ot);

            /* ??? The manual says that the output is undefined when the
               input is zero, but real hardware leaves it unchanged, and
               real programs appear to depend on that.  Accomplish this
               by passing the output as the value to return upon zero.  */
            if (b & 1) {
                /* For bsr, return the bit index of the first 1 bit,
                   not the count of leading zeros.  */
                tcg_gen_xori_tl(s->T1, cpu_regs[reg], TARGET_LONG_BITS - 1);
                tcg_gen_clz_tl(s->T0, s->T0, s->T1);
                tcg_gen_xori_tl(s->T0, s->T0, TARGET_LONG_BITS - 1);
            } else {
                tcg_gen_ctz_tl(s->T0, s->T0, cpu_regs[reg]);
            }
        }
        gen_op_mov_reg_v(s, ot, reg, s->T0);
        break;
        /************************/
        /* bcd */
    case 0x27: /* daa */
        if (CODE64(s))
            goto illegal_op;
        gen_update_cc_op(s);
        gen_helper_daa(cpu_env);
        set_cc_op(s, CC_OP_EFLAGS);
        break;
    case 0x2f: /* das */
        if (CODE64(s))
            goto illegal_op;
        gen_update_cc_op(s);
        gen_helper_das(cpu_env);
        set_cc_op(s, CC_OP_EFLAGS);
        break;
    case 0x37: /* aaa */
        if (CODE64(s))
            goto illegal_op;
        gen_update_cc_op(s);
        gen_helper_aaa(cpu_env);
        set_cc_op(s, CC_OP_EFLAGS);
        break;
    case 0x3f: /* aas */
        if (CODE64(s))
            goto illegal_op;
        gen_update_cc_op(s);
        gen_helper_aas(cpu_env);
        set_cc_op(s, CC_OP_EFLAGS);
        break;
    case 0xd4: /* aam */
        if (CODE64(s))
            goto illegal_op;
        val = x86_ldub_code(env, s);
        if (val == 0) {
            gen_exception(s, EXCP00_DIVZ);
        } else {
            gen_helper_aam(cpu_env, tcg_const_i32(val));
            set_cc_op(s, CC_OP_LOGICB);
        }
        break;
    case 0xd5: /* aad */
        if (CODE64(s))
            goto illegal_op;
        val = x86_ldub_code(env, s);
        gen_helper_aad(cpu_env, tcg_const_i32(val));
        set_cc_op(s, CC_OP_LOGICB);
        break;
        /************************/
        /* misc */
    case 0x90: /* nop */
        /* XXX: correct lock test for all insn */
        if (s->prefix & PREFIX_LOCK) {
            goto illegal_op;
        }
        /* If REX_B is set, then this is xchg eax, r8d, not a nop.  */
        if (REX_B(s)) {
            goto do_xchg_reg_eax;
        }
        if (s->prefix & PREFIX_REPZ) {
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            gen_helper_pause(cpu_env, tcg_const_i32(s->pc - s->pc_start));
            s->base.is_jmp = DISAS_NORETURN;
        }
        break;
    case 0x9b: /* fwait */
        if ((s->flags & (HF_MP_MASK | HF_TS_MASK)) ==
            (HF_MP_MASK | HF_TS_MASK)) {
            gen_exception(s, EXCP07_PREX);
        } else {
            gen_helper_fwait(cpu_env);
        }
        break;
    case 0xcc: /* int3 */
        gen_interrupt(s, EXCP03_INT3, s->pc_start - s->cs_base, s->pc - s->cs_base);
        break;
    case 0xcd: /* int N */
        val = x86_ldub_code(env, s);
        if (s->vm86 && s->iopl != 3) {
            gen_exception(s, EXCP0D_GPF);
        } else {
            gen_interrupt(s, val, s->pc_start - s->cs_base, s->pc - s->cs_base);
        }
        break;
    case 0xce: /* into */
        if (CODE64(s))
            goto illegal_op;
        gen_update_cc_op(s);
        gen_jmp_im(s, s->pc_start - s->cs_base);
        gen_helper_into(cpu_env, tcg_const_i32(s->pc - s->pc_start));
        break;
#ifdef WANT_ICEBP
    case 0xf1: /* icebp (undocumented, exits to external debugger) */
        gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_ICEBP);
        gen_debug(s, s->pc_start - s->cs_base);
        break;
#endif
    case 0xfa: /* cli */
        if (!s->vm86) {
            if (s->cpl <= s->iopl) {
                gen_helper_cli(cpu_env);
            } else {
                gen_exception(s, EXCP0D_GPF);
            }
        } else {
            if (s->iopl == 3) {
                gen_helper_cli(cpu_env);
            } else {
                gen_exception(s, EXCP0D_GPF);
            }
        }
        break;
    case 0xfb: /* sti */
        if (s->vm86 ? s->iopl == 3 : s->cpl <= s->iopl) {
            gen_helper_sti(cpu_env);
            /* interruptions are enabled only the first insn after sti */
            gen_jmp_im(s, s->pc - s->cs_base);
            gen_eob_inhibit_irq(s, true);
        } else {
            gen_exception(s, EXCP0D_GPF);
        }
        break;
    case 0x62: /* bound */
        if (CODE64(s))
            goto illegal_op;
        ot = s->dflag;
        modrm = x86_ldub_code(env, s);
        reg = (modrm >> 3) & 7;
        mod = (modrm >> 6) & 3;
        if (mod == 3)
            goto illegal_op;
        gen_op_mov_v_reg(s, ot, s->T0, reg);
        gen_lea_modrm(env, s, modrm);
        tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
        if (ot == MO_16) {
            gen_helper_boundw(cpu_env, s->A0, s->tmp2_i32);
        } else {
            gen_helper_boundl(cpu_env, s->A0, s->tmp2_i32);
        }
        break;
    case 0x1c8 ... 0x1cf: /* bswap reg */
        reg = (b & 7) | REX_B(s);
#ifdef TARGET_X86_64
        if (s->dflag == MO_64) {
            gen_op_mov_v_reg(s, MO_64, s->T0, reg);
            tcg_gen_bswap64_i64(s->T0, s->T0);
            gen_op_mov_reg_v(s, MO_64, reg, s->T0);
        } else
#endif
        {
            gen_op_mov_v_reg(s, MO_32, s->T0, reg);
            tcg_gen_ext32u_tl(s->T0, s->T0);
            tcg_gen_bswap32_tl(s->T0, s->T0);
            gen_op_mov_reg_v(s, MO_32, reg, s->T0);
        }
        break;
    case 0xd6: /* salc */
        if (CODE64(s))
            goto illegal_op;
        gen_compute_eflags_c(s, s->T0);
        tcg_gen_neg_tl(s->T0, s->T0);
        gen_op_mov_reg_v(s, MO_8, R_EAX, s->T0);
        break;
    case 0xe0: /* loopnz */
    case 0xe1: /* loopz */
    case 0xe2: /* loop */
    case 0xe3: /* jecxz */
        {
            TCGLabel *l1, *l2, *l3;

            tval = (int8_t)insn_get(env, s, MO_8);
            next_eip = s->pc - s->cs_base;
            tval += next_eip;
            if (s->dflag == MO_16) {
                tval &= 0xffff;
            }

            l1 = gen_new_label();
            l2 = gen_new_label();
            l3 = gen_new_label();
            b &= 3;
            switch(b) {
            case 0: /* loopnz */
            case 1: /* loopz */
                gen_op_add_reg_im(s, s->aflag, R_ECX, -1);
                gen_op_jz_ecx(s, s->aflag, l3);
                gen_jcc1(s, (JCC_Z << 1) | (b ^ 1), l1);
                break;
            case 2: /* loop */
                gen_op_add_reg_im(s, s->aflag, R_ECX, -1);
                gen_op_jnz_ecx(s, s->aflag, l1);
                break;
            default:
            case 3: /* jcxz */
                gen_op_jz_ecx(s, s->aflag, l1);
                break;
            }

            gen_set_label(l3);
            gen_jmp_im(s, next_eip);
            tcg_gen_br(l2);

            gen_set_label(l1);
            gen_jmp_im(s, tval);
            gen_set_label(l2);
            gen_eob(s);
        }
        break;
    case 0x130: /* wrmsr */
    case 0x132: /* rdmsr */
        if (s->cpl != 0) {
            gen_exception(s, EXCP0D_GPF);
        } else {
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            if (b & 2) {
                gen_helper_rdmsr(cpu_env);
            } else {
                gen_helper_wrmsr(cpu_env);
            }
        }
        break;
    case 0x131: /* rdtsc */
        gen_update_cc_op(s);
        gen_jmp_im(s, s->pc_start - s->cs_base);
        if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
            gen_io_start();
        }
        gen_helper_rdtsc(cpu_env);
        if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
            gen_jmp(s, s->pc - s->cs_base);
        }
        break;
    case 0x133: /* rdpmc */
        gen_update_cc_op(s);
        gen_jmp_im(s, s->pc_start - s->cs_base);
        gen_helper_rdpmc(cpu_env);
        break;
    case 0x134: /* sysenter */
        /* For Intel SYSENTER is valid on 64-bit */
        if (CODE64(s) && env->cpuid_vendor1 != CPUID_VENDOR_INTEL_1)
            goto illegal_op;
        if (!s->pe) {
            gen_exception(s, EXCP0D_GPF);
        } else {
            gen_helper_sysenter(cpu_env);
            gen_eob(s);
        }
        break;
    case 0x135: /* sysexit */
        /* For Intel SYSEXIT is valid on 64-bit */
        if (CODE64(s) && env->cpuid_vendor1 != CPUID_VENDOR_INTEL_1)
            goto illegal_op;
        if (!s->pe) {
            gen_exception(s, EXCP0D_GPF);
        } else {
            gen_helper_sysexit(cpu_env, tcg_const_i32(s->dflag - 1));
            gen_eob(s);
        }
        break;
#ifdef TARGET_X86_64
    case 0x105: /* syscall */
        /* XXX: is it usable in real mode ? */
        gen_update_cc_op(s);
        gen_jmp_im(s, s->pc_start - s->cs_base);
        gen_helper_syscall(cpu_env, tcg_const_i32(s->pc - s->pc_start));
        /* TF handling for the syscall insn is different. The TF bit is  checked
           after the syscall insn completes. This allows #DB to not be
           generated after one has entered CPL0 if TF is set in FMASK.  */
        gen_eob_worker(s, false, true);
        break;
    case 0x107: /* sysret */
        if (!s->pe) {
            gen_exception(s, EXCP0D_GPF);
        } else {
            gen_helper_sysret(cpu_env, tcg_const_i32(s->dflag - 1));
            /* condition codes are modified only in long mode */
            if (s->lma) {
                set_cc_op(s, CC_OP_EFLAGS);
            }
            /* TF handling for the sysret insn is different. The TF bit is
               checked after the sysret insn completes. This allows #DB to be
               generated "as if" the syscall insn in userspace has just
               completed.  */
            gen_eob_worker(s, false, true);
        }
        break;
#endif
    case 0x1a2: /* cpuid */
        gen_update_cc_op(s);
        gen_jmp_im(s, s->pc_start - s->cs_base);
        gen_helper_cpuid(cpu_env);
        break;
    case 0xf4: /* hlt */
        if (s->cpl != 0) {
            gen_exception(s, EXCP0D_GPF);
        } else {
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            gen_helper_hlt(cpu_env, tcg_const_i32(s->pc - s->pc_start));
            s->base.is_jmp = DISAS_NORETURN;
        }
        break;
    case 0x100:
        modrm = x86_ldub_code(env, s);
        mod = (modrm >> 6) & 3;
        op = (modrm >> 3) & 7;
        switch(op) {
        case 0: /* sldt */
            if (!s->pe || s->vm86)
                goto illegal_op;
            gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_LDTR_READ);
            tcg_gen_ld32u_tl(s->T0, cpu_env,
                             offsetof(CPUX86State, ldt.selector));
            ot = mod == 3 ? s->dflag : MO_16;
            gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 1);
            break;
        case 2: /* lldt */
            if (!s->pe || s->vm86)
                goto illegal_op;
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
            } else {
                gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_LDTR_WRITE);
                gen_ldst_modrm(env, s, modrm, MO_16, OR_TMP0, 0);
                tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
                gen_helper_lldt(cpu_env, s->tmp2_i32);
            }
            break;
        case 1: /* str */
            if (!s->pe || s->vm86)
                goto illegal_op;
            gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_TR_READ);
            tcg_gen_ld32u_tl(s->T0, cpu_env,
                             offsetof(CPUX86State, tr.selector));
            ot = mod == 3 ? s->dflag : MO_16;
            gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 1);
            break;
        case 3: /* ltr */
            if (!s->pe || s->vm86)
                goto illegal_op;
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
            } else {
                gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_TR_WRITE);
                gen_ldst_modrm(env, s, modrm, MO_16, OR_TMP0, 0);
                tcg_gen_trunc_tl_i32(s->tmp2_i32, s->T0);
                gen_helper_ltr(cpu_env, s->tmp2_i32);
            }
            break;
        case 4: /* verr */
        case 5: /* verw */
            if (!s->pe || s->vm86)
                goto illegal_op;
            gen_ldst_modrm(env, s, modrm, MO_16, OR_TMP0, 0);
            gen_update_cc_op(s);
            if (op == 4) {
                gen_helper_verr(cpu_env, s->T0);
            } else {
                gen_helper_verw(cpu_env, s->T0);
            }
            set_cc_op(s, CC_OP_EFLAGS);
            break;
        default:
            goto unknown_op;
        }
        break;

    case 0x101:
        modrm = x86_ldub_code(env, s);
        switch (modrm) {
        CASE_MODRM_MEM_OP(0): /* sgdt */
            gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_GDTR_READ);
            gen_lea_modrm(env, s, modrm);
            tcg_gen_ld32u_tl(s->T0,
                             cpu_env, offsetof(CPUX86State, gdt.limit));
            gen_op_st_v(s, MO_16, s->T0, s->A0);
            gen_add_A0_im(s, 2);
            tcg_gen_ld_tl(s->T0, cpu_env, offsetof(CPUX86State, gdt.base));
            if (s->dflag == MO_16) {
                tcg_gen_andi_tl(s->T0, s->T0, 0xffffff);
            }
            gen_op_st_v(s, CODE64(s) + MO_32, s->T0, s->A0);
            break;

        case 0xc8: /* monitor */
            if (!(s->cpuid_ext_features & CPUID_EXT_MONITOR) || s->cpl != 0) {
                goto illegal_op;
            }
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            tcg_gen_mov_tl(s->A0, cpu_regs[R_EAX]);
            gen_extu(s->aflag, s->A0);
            gen_add_A0_ds_seg(s);
            gen_helper_monitor(cpu_env, s->A0);
            break;

        case 0xc9: /* mwait */
            if (!(s->cpuid_ext_features & CPUID_EXT_MONITOR) || s->cpl != 0) {
                goto illegal_op;
            }
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            gen_helper_mwait(cpu_env, tcg_const_i32(s->pc - s->pc_start));
            gen_eob(s);
            break;

        case 0xca: /* clac */
            if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_SMAP)
                || s->cpl != 0) {
                goto illegal_op;
            }
            gen_helper_clac(cpu_env);
            gen_jmp_im(s, s->pc - s->cs_base);
            gen_eob(s);
            break;

        case 0xcb: /* stac */
            if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_SMAP)
                || s->cpl != 0) {
                goto illegal_op;
            }
            gen_helper_stac(cpu_env);
            gen_jmp_im(s, s->pc - s->cs_base);
            gen_eob(s);
            break;

        CASE_MODRM_MEM_OP(1): /* sidt */
            gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_IDTR_READ);
            gen_lea_modrm(env, s, modrm);
            tcg_gen_ld32u_tl(s->T0, cpu_env, offsetof(CPUX86State, idt.limit));
            gen_op_st_v(s, MO_16, s->T0, s->A0);
            gen_add_A0_im(s, 2);
            tcg_gen_ld_tl(s->T0, cpu_env, offsetof(CPUX86State, idt.base));
            if (s->dflag == MO_16) {
                tcg_gen_andi_tl(s->T0, s->T0, 0xffffff);
            }
            gen_op_st_v(s, CODE64(s) + MO_32, s->T0, s->A0);
            break;

        case 0xd0: /* xgetbv */
            if ((s->cpuid_ext_features & CPUID_EXT_XSAVE) == 0
                || (s->prefix & (PREFIX_LOCK | PREFIX_DATA
                                 | PREFIX_REPZ | PREFIX_REPNZ))) {
                goto illegal_op;
            }
            tcg_gen_trunc_tl_i32(s->tmp2_i32, cpu_regs[R_ECX]);
            gen_helper_xgetbv(s->tmp1_i64, cpu_env, s->tmp2_i32);
            tcg_gen_extr_i64_tl(cpu_regs[R_EAX], cpu_regs[R_EDX], s->tmp1_i64);
            break;

        case 0xd1: /* xsetbv */
            if ((s->cpuid_ext_features & CPUID_EXT_XSAVE) == 0
                || (s->prefix & (PREFIX_LOCK | PREFIX_DATA
                                 | PREFIX_REPZ | PREFIX_REPNZ))) {
                goto illegal_op;
            }
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
                break;
            }
            tcg_gen_concat_tl_i64(s->tmp1_i64, cpu_regs[R_EAX],
                                  cpu_regs[R_EDX]);
            tcg_gen_trunc_tl_i32(s->tmp2_i32, cpu_regs[R_ECX]);
            gen_helper_xsetbv(cpu_env, s->tmp2_i32, s->tmp1_i64);
            /* End TB because translation flags may change.  */
            gen_jmp_im(s, s->pc - s->cs_base);
            gen_eob(s);
            break;

        case 0xd8: /* VMRUN */
            if (!(s->flags & HF_SVME_MASK) || !s->pe) {
                goto illegal_op;
            }
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
                break;
            }
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            gen_helper_vmrun(cpu_env, tcg_const_i32(s->aflag - 1),
                             tcg_const_i32(s->pc - s->pc_start));
            tcg_gen_exit_tb(NULL, 0);
            s->base.is_jmp = DISAS_NORETURN;
            break;

        case 0xd9: /* VMMCALL */
            if (!(s->flags & HF_SVME_MASK)) {
                goto illegal_op;
            }
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            gen_helper_vmmcall(cpu_env);
            break;

        case 0xda: /* VMLOAD */
            if (!(s->flags & HF_SVME_MASK) || !s->pe) {
                goto illegal_op;
            }
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
                break;
            }
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            gen_helper_vmload(cpu_env, tcg_const_i32(s->aflag - 1));
            break;

        case 0xdb: /* VMSAVE */
            if (!(s->flags & HF_SVME_MASK) || !s->pe) {
                goto illegal_op;
            }
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
                break;
            }
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            gen_helper_vmsave(cpu_env, tcg_const_i32(s->aflag - 1));
            break;

        case 0xdc: /* STGI */
            if ((!(s->flags & HF_SVME_MASK)
                   && !(s->cpuid_ext3_features & CPUID_EXT3_SKINIT))
                || !s->pe) {
                goto illegal_op;
            }
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
                break;
            }
            gen_update_cc_op(s);
            gen_helper_stgi(cpu_env);
            gen_jmp_im(s, s->pc - s->cs_base);
            gen_eob(s);
            break;

        case 0xdd: /* CLGI */
            if (!(s->flags & HF_SVME_MASK) || !s->pe) {
                goto illegal_op;
            }
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
                break;
            }
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            gen_helper_clgi(cpu_env);
            break;

        case 0xde: /* SKINIT */
            if ((!(s->flags & HF_SVME_MASK)
                 && !(s->cpuid_ext3_features & CPUID_EXT3_SKINIT))
                || !s->pe) {
                goto illegal_op;
            }
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            gen_helper_skinit(cpu_env);
            break;

        case 0xdf: /* INVLPGA */
            if (!(s->flags & HF_SVME_MASK) || !s->pe) {
                goto illegal_op;
            }
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
                break;
            }
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            gen_helper_invlpga(cpu_env, tcg_const_i32(s->aflag - 1));
            break;

        CASE_MODRM_MEM_OP(2): /* lgdt */
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
                break;
            }
            gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_GDTR_WRITE);
            gen_lea_modrm(env, s, modrm);
            gen_op_ld_v(s, MO_16, s->T1, s->A0);
            gen_add_A0_im(s, 2);
            gen_op_ld_v(s, CODE64(s) + MO_32, s->T0, s->A0);
            if (s->dflag == MO_16) {
                tcg_gen_andi_tl(s->T0, s->T0, 0xffffff);
            }
            tcg_gen_st_tl(s->T0, cpu_env, offsetof(CPUX86State, gdt.base));
            tcg_gen_st32_tl(s->T1, cpu_env, offsetof(CPUX86State, gdt.limit));
            break;

        CASE_MODRM_MEM_OP(3): /* lidt */
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
                break;
            }
            gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_IDTR_WRITE);
            gen_lea_modrm(env, s, modrm);
            gen_op_ld_v(s, MO_16, s->T1, s->A0);
            gen_add_A0_im(s, 2);
            gen_op_ld_v(s, CODE64(s) + MO_32, s->T0, s->A0);
            if (s->dflag == MO_16) {
                tcg_gen_andi_tl(s->T0, s->T0, 0xffffff);
            }
            tcg_gen_st_tl(s->T0, cpu_env, offsetof(CPUX86State, idt.base));
            tcg_gen_st32_tl(s->T1, cpu_env, offsetof(CPUX86State, idt.limit));
            break;

        CASE_MODRM_OP(4): /* smsw */
            gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_READ_CR0);
            tcg_gen_ld_tl(s->T0, cpu_env, offsetof(CPUX86State, cr[0]));
            if (CODE64(s)) {
                mod = (modrm >> 6) & 3;
                ot = (mod != 3 ? MO_16 : s->dflag);
            } else {
                ot = MO_16;
            }
            gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 1);
            break;
        case 0xee: /* rdpkru */
            if (s->prefix & PREFIX_LOCK) {
                goto illegal_op;
            }
            tcg_gen_trunc_tl_i32(s->tmp2_i32, cpu_regs[R_ECX]);
            gen_helper_rdpkru(s->tmp1_i64, cpu_env, s->tmp2_i32);
            tcg_gen_extr_i64_tl(cpu_regs[R_EAX], cpu_regs[R_EDX], s->tmp1_i64);
            break;
        case 0xef: /* wrpkru */
            if (s->prefix & PREFIX_LOCK) {
                goto illegal_op;
            }
            tcg_gen_concat_tl_i64(s->tmp1_i64, cpu_regs[R_EAX],
                                  cpu_regs[R_EDX]);
            tcg_gen_trunc_tl_i32(s->tmp2_i32, cpu_regs[R_ECX]);
            gen_helper_wrpkru(cpu_env, s->tmp2_i32, s->tmp1_i64);
            break;
        CASE_MODRM_OP(6): /* lmsw */
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
                break;
            }
            gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_WRITE_CR0);
            gen_ldst_modrm(env, s, modrm, MO_16, OR_TMP0, 0);
            gen_helper_lmsw(cpu_env, s->T0);
            gen_jmp_im(s, s->pc - s->cs_base);
            gen_eob(s);
            break;

        CASE_MODRM_MEM_OP(7): /* invlpg */
            if (s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF);
                break;
            }
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            gen_lea_modrm(env, s, modrm);
            gen_helper_invlpg(cpu_env, s->A0);
            gen_jmp_im(s, s->pc - s->cs_base);
            gen_eob(s);
            break;

        case 0xf8: /* swapgs */
#ifdef TARGET_X86_64
            if (CODE64(s)) {
                if (s->cpl != 0) {
                    gen_exception(s, EXCP0D_GPF);
                } else {
                    tcg_gen_mov_tl(s->T0, cpu_seg_base[R_GS]);
                    tcg_gen_ld_tl(cpu_seg_base[R_GS], cpu_env,
                                  offsetof(CPUX86State, kernelgsbase));
                    tcg_gen_st_tl(s->T0, cpu_env,
                                  offsetof(CPUX86State, kernelgsbase));
                }
                break;
            }
#endif
            goto illegal_op;

        case 0xf9: /* rdtscp */
            if (!(s->cpuid_ext2_features & CPUID_EXT2_RDTSCP)) {
                goto illegal_op;
            }
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc_start - s->cs_base);
            if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
                gen_io_start();
            }
            gen_helper_rdtscp(cpu_env);
            if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
                gen_jmp(s, s->pc - s->cs_base);
            }
            break;

        default:
            goto unknown_op;
        }
        break;

    case 0x108: /* invd */
    case 0x109: /* wbinvd */
        if (s->cpl != 0) {
            gen_exception(s, EXCP0D_GPF);
        } else {
            gen_svm_check_intercept(s, s->pc_start, (b & 2) ? SVM_EXIT_INVD : SVM_EXIT_WBINVD);
            /* nothing to do */
        }
        break;
    case 0x63: /* arpl or movslS (x86_64) */
#ifdef TARGET_X86_64
        if (CODE64(s)) {
            int d_ot;
            /* d_ot is the size of destination */
            d_ot = s->dflag;

            modrm = x86_ldub_code(env, s);
            reg = ((modrm >> 3) & 7) | REX_R(s);
            mod = (modrm >> 6) & 3;
            rm = (modrm & 7) | REX_B(s);

            if (mod == 3) {
                gen_op_mov_v_reg(s, MO_32, s->T0, rm);
                /* sign extend */
                if (d_ot == MO_64) {
                    tcg_gen_ext32s_tl(s->T0, s->T0);
                }
                gen_op_mov_reg_v(s, d_ot, reg, s->T0);
            } else {
                gen_lea_modrm(env, s, modrm);
                gen_op_ld_v(s, MO_32 | MO_SIGN, s->T0, s->A0);
                gen_op_mov_reg_v(s, d_ot, reg, s->T0);
            }
        } else
#endif
        {
            TCGLabel *label1;
            TCGv t0, t1, t2, a0;

            if (!s->pe || s->vm86)
                goto illegal_op;
            t0 = tcg_temp_local_new();
            t1 = tcg_temp_local_new();
            t2 = tcg_temp_local_new();
            ot = MO_16;
            modrm = x86_ldub_code(env, s);
            reg = (modrm >> 3) & 7;
            mod = (modrm >> 6) & 3;
            rm = modrm & 7;
            if (mod != 3) {
                gen_lea_modrm(env, s, modrm);
                gen_op_ld_v(s, ot, t0, s->A0);
                a0 = tcg_temp_local_new();
                tcg_gen_mov_tl(a0, s->A0);
            } else {
                gen_op_mov_v_reg(s, ot, t0, rm);
                a0 = NULL;
            }
            gen_op_mov_v_reg(s, ot, t1, reg);
            tcg_gen_andi_tl(s->tmp0, t0, 3);
            tcg_gen_andi_tl(t1, t1, 3);
            tcg_gen_movi_tl(t2, 0);
            label1 = gen_new_label();
            tcg_gen_brcond_tl(TCG_COND_GE, s->tmp0, t1, label1);
            tcg_gen_andi_tl(t0, t0, ~3);
            tcg_gen_or_tl(t0, t0, t1);
            tcg_gen_movi_tl(t2, CC_Z);
            gen_set_label(label1);
            if (mod != 3) {
                gen_op_st_v(s, ot, t0, a0);
                tcg_temp_free(a0);
           } else {
                gen_op_mov_reg_v(s, ot, rm, t0);
            }
            gen_compute_eflags(s);
            tcg_gen_andi_tl(cpu_cc_src, cpu_cc_src, ~CC_Z);
            tcg_gen_or_tl(cpu_cc_src, cpu_cc_src, t2);
            tcg_temp_free(t0);
            tcg_temp_free(t1);
            tcg_temp_free(t2);
        }
        break;
    case 0x102: /* lar */
    case 0x103: /* lsl */
        {
            TCGLabel *label1;
            TCGv t0;
            if (!s->pe || s->vm86)
                goto illegal_op;
            ot = s->dflag != MO_16 ? MO_32 : MO_16;
            modrm = x86_ldub_code(env, s);
            reg = ((modrm >> 3) & 7) | REX_R(s);
            gen_ldst_modrm(env, s, modrm, MO_16, OR_TMP0, 0);
            t0 = tcg_temp_local_new();
            gen_update_cc_op(s);
            if (b == 0x102) {
                gen_helper_lar(t0, cpu_env, s->T0);
            } else {
                gen_helper_lsl(t0, cpu_env, s->T0);
            }
            tcg_gen_andi_tl(s->tmp0, cpu_cc_src, CC_Z);
            label1 = gen_new_label();
            tcg_gen_brcondi_tl(TCG_COND_EQ, s->tmp0, 0, label1);
            gen_op_mov_reg_v(s, ot, reg, t0);
            gen_set_label(label1);
            set_cc_op(s, CC_OP_EFLAGS);
            tcg_temp_free(t0);
        }
        break;
    case 0x118:
        modrm = x86_ldub_code(env, s);
        mod = (modrm >> 6) & 3;
        op = (modrm >> 3) & 7;
        switch(op) {
        case 0: /* prefetchnta */
        case 1: /* prefetchnt0 */
        case 2: /* prefetchnt0 */
        case 3: /* prefetchnt0 */
            if (mod == 3)
                goto illegal_op;
            gen_nop_modrm(env, s, modrm);
            /* nothing more to do */
            break;
        default: /* nop (multi byte) */
            gen_nop_modrm(env, s, modrm);
            break;
        }
        break;
    case 0x11a:
        modrm = x86_ldub_code(env, s);
        if (s->flags & HF_MPX_EN_MASK) {
            mod = (modrm >> 6) & 3;
            reg = ((modrm >> 3) & 7) | REX_R(s);
            if (s->prefix & PREFIX_REPZ) {
                /* bndcl */
                if (reg >= 4
                    || (s->prefix & PREFIX_LOCK)
                    || s->aflag == MO_16) {
                    goto illegal_op;
                }
                gen_bndck(env, s, modrm, TCG_COND_LTU, cpu_bndl[reg]);
            } else if (s->prefix & PREFIX_REPNZ) {
                /* bndcu */
                if (reg >= 4
                    || (s->prefix & PREFIX_LOCK)
                    || s->aflag == MO_16) {
                    goto illegal_op;
                }
                TCGv_i64 notu = tcg_temp_new_i64();
                tcg_gen_not_i64(notu, cpu_bndu[reg]);
                gen_bndck(env, s, modrm, TCG_COND_GTU, notu);
                tcg_temp_free_i64(notu);
            } else if (s->prefix & PREFIX_DATA) {
                /* bndmov -- from reg/mem */
                if (reg >= 4 || s->aflag == MO_16) {
                    goto illegal_op;
                }
                if (mod == 3) {
                    int reg2 = (modrm & 7) | REX_B(s);
                    if (reg2 >= 4 || (s->prefix & PREFIX_LOCK)) {
                        goto illegal_op;
                    }
                    if (s->flags & HF_MPX_IU_MASK) {
                        tcg_gen_mov_i64(cpu_bndl[reg], cpu_bndl[reg2]);
                        tcg_gen_mov_i64(cpu_bndu[reg], cpu_bndu[reg2]);
                    }
                } else {
                    gen_lea_modrm(env, s, modrm);
                    if (CODE64(s)) {
                        tcg_gen_qemu_ld_i64(cpu_bndl[reg], s->A0,
                                            s->mem_index, MO_LEQ);
                        tcg_gen_addi_tl(s->A0, s->A0, 8);
                        tcg_gen_qemu_ld_i64(cpu_bndu[reg], s->A0,
                                            s->mem_index, MO_LEQ);
                    } else {
                        tcg_gen_qemu_ld_i64(cpu_bndl[reg], s->A0,
                                            s->mem_index, MO_LEUL);
                        tcg_gen_addi_tl(s->A0, s->A0, 4);
                        tcg_gen_qemu_ld_i64(cpu_bndu[reg], s->A0,
                                            s->mem_index, MO_LEUL);
                    }
                    /* bnd registers are now in-use */
                    gen_set_hflag(s, HF_MPX_IU_MASK);
                }
            } else if (mod != 3) {
                /* bndldx */
                AddressParts a = gen_lea_modrm_0(env, s, modrm);
                if (reg >= 4
                    || (s->prefix & PREFIX_LOCK)
                    || s->aflag == MO_16
                    || a.base < -1) {
                    goto illegal_op;
                }
                if (a.base >= 0) {
                    tcg_gen_addi_tl(s->A0, cpu_regs[a.base], a.disp);
                } else {
                    tcg_gen_movi_tl(s->A0, 0);
                }
                gen_lea_v_seg(s, s->aflag, s->A0, a.def_seg, s->override);
                if (a.index >= 0) {
                    tcg_gen_mov_tl(s->T0, cpu_regs[a.index]);
                } else {
                    tcg_gen_movi_tl(s->T0, 0);
                }
                if (CODE64(s)) {
                    gen_helper_bndldx64(cpu_bndl[reg], cpu_env, s->A0, s->T0);
                    tcg_gen_ld_i64(cpu_bndu[reg], cpu_env,
                                   offsetof(CPUX86State, mmx_t0.MMX_Q(0)));
                } else {
                    gen_helper_bndldx32(cpu_bndu[reg], cpu_env, s->A0, s->T0);
                    tcg_gen_ext32u_i64(cpu_bndl[reg], cpu_bndu[reg]);
                    tcg_gen_shri_i64(cpu_bndu[reg], cpu_bndu[reg], 32);
                }
                gen_set_hflag(s, HF_MPX_IU_MASK);
            }
        }
        gen_nop_modrm(env, s, modrm);
        break;
    case 0x11b:
        modrm = x86_ldub_code(env, s);
        if (s->flags & HF_MPX_EN_MASK) {
            mod = (modrm >> 6) & 3;
            reg = ((modrm >> 3) & 7) | REX_R(s);
            if (mod != 3 && (s->prefix & PREFIX_REPZ)) {
                /* bndmk */
                if (reg >= 4
                    || (s->prefix & PREFIX_LOCK)
                    || s->aflag == MO_16) {
                    goto illegal_op;
                }
                AddressParts a = gen_lea_modrm_0(env, s, modrm);
                if (a.base >= 0) {
                    tcg_gen_extu_tl_i64(cpu_bndl[reg], cpu_regs[a.base]);
                    if (!CODE64(s)) {
                        tcg_gen_ext32u_i64(cpu_bndl[reg], cpu_bndl[reg]);
                    }
                } else if (a.base == -1) {
                    /* no base register has lower bound of 0 */
                    tcg_gen_movi_i64(cpu_bndl[reg], 0);
                } else {
                    /* rip-relative generates #ud */
                    goto illegal_op;
                }
                tcg_gen_not_tl(s->A0, gen_lea_modrm_1(s, a));
                if (!CODE64(s)) {
                    tcg_gen_ext32u_tl(s->A0, s->A0);
                }
                tcg_gen_extu_tl_i64(cpu_bndu[reg], s->A0);
                /* bnd registers are now in-use */
                gen_set_hflag(s, HF_MPX_IU_MASK);
                break;
            } else if (s->prefix & PREFIX_REPNZ) {
                /* bndcn */
                if (reg >= 4
                    || (s->prefix & PREFIX_LOCK)
                    || s->aflag == MO_16) {
                    goto illegal_op;
                }
                gen_bndck(env, s, modrm, TCG_COND_GTU, cpu_bndu[reg]);
            } else if (s->prefix & PREFIX_DATA) {
                /* bndmov -- to reg/mem */
                if (reg >= 4 || s->aflag == MO_16) {
                    goto illegal_op;
                }
                if (mod == 3) {
                    int reg2 = (modrm & 7) | REX_B(s);
                    if (reg2 >= 4 || (s->prefix & PREFIX_LOCK)) {
                        goto illegal_op;
                    }
                    if (s->flags & HF_MPX_IU_MASK) {
                        tcg_gen_mov_i64(cpu_bndl[reg2], cpu_bndl[reg]);
                        tcg_gen_mov_i64(cpu_bndu[reg2], cpu_bndu[reg]);
                    }
                } else {
                    gen_lea_modrm(env, s, modrm);
                    if (CODE64(s)) {
                        tcg_gen_qemu_st_i64(cpu_bndl[reg], s->A0,
                                            s->mem_index, MO_LEQ);
                        tcg_gen_addi_tl(s->A0, s->A0, 8);
                        tcg_gen_qemu_st_i64(cpu_bndu[reg], s->A0,
                                            s->mem_index, MO_LEQ);
                    } else {
                        tcg_gen_qemu_st_i64(cpu_bndl[reg], s->A0,
                                            s->mem_index, MO_LEUL);
                        tcg_gen_addi_tl(s->A0, s->A0, 4);
                        tcg_gen_qemu_st_i64(cpu_bndu[reg], s->A0,
                                            s->mem_index, MO_LEUL);
                    }
                }
            } else if (mod != 3) {
                /* bndstx */
                AddressParts a = gen_lea_modrm_0(env, s, modrm);
                if (reg >= 4
                    || (s->prefix & PREFIX_LOCK)
                    || s->aflag == MO_16
                    || a.base < -1) {
                    goto illegal_op;
                }
                if (a.base >= 0) {
                    tcg_gen_addi_tl(s->A0, cpu_regs[a.base], a.disp);
                } else {
                    tcg_gen_movi_tl(s->A0, 0);
                }
                gen_lea_v_seg(s, s->aflag, s->A0, a.def_seg, s->override);
                if (a.index >= 0) {
                    tcg_gen_mov_tl(s->T0, cpu_regs[a.index]);
                } else {
                    tcg_gen_movi_tl(s->T0, 0);
                }
                if (CODE64(s)) {
                    gen_helper_bndstx64(cpu_env, s->A0, s->T0,
                                        cpu_bndl[reg], cpu_bndu[reg]);
                } else {
                    gen_helper_bndstx32(cpu_env, s->A0, s->T0,
                                        cpu_bndl[reg], cpu_bndu[reg]);
                }
            }
        }
        gen_nop_modrm(env, s, modrm);
        break;
    case 0x119: case 0x11c ... 0x11f: /* nop (multi byte) */
        modrm = x86_ldub_code(env, s);
        gen_nop_modrm(env, s, modrm);
        break;
    case 0x120: /* mov reg, crN */
    case 0x122: /* mov crN, reg */
        if (s->cpl != 0) {
            gen_exception(s, EXCP0D_GPF);
        } else {
            modrm = x86_ldub_code(env, s);
            /* Ignore the mod bits (assume (modrm&0xc0)==0xc0).
             * AMD documentation (24594.pdf) and testing of
             * intel 386 and 486 processors all show that the mod bits
             * are assumed to be 1's, regardless of actual values.
             */
            rm = (modrm & 7) | REX_B(s);
            reg = ((modrm >> 3) & 7) | REX_R(s);
            if (CODE64(s))
                ot = MO_64;
            else
                ot = MO_32;
            if ((s->prefix & PREFIX_LOCK) && (reg == 0) &&
                (s->cpuid_ext3_features & CPUID_EXT3_CR8LEG)) {
                reg = 8;
            }
            switch(reg) {
            case 0:
            case 2:
            case 3:
            case 4:
            case 8:
                gen_update_cc_op(s);
                gen_jmp_im(s, s->pc_start - s->cs_base);
                if (b & 2) {
                    if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
                        gen_io_start();
                    }
                    gen_op_mov_v_reg(s, ot, s->T0, rm);
                    gen_helper_write_crN(cpu_env, tcg_const_i32(reg),
                                         s->T0);
                    gen_jmp_im(s, s->pc - s->cs_base);
                    gen_eob(s);
                } else {
                    if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
                        gen_io_start();
                    }
                    gen_helper_read_crN(s->T0, cpu_env, tcg_const_i32(reg));
                    gen_op_mov_reg_v(s, ot, rm, s->T0);
                    if (tb_cflags(s->base.tb) & CF_USE_ICOUNT) {
                        gen_io_end();
                    }
                }
                break;
            default:
                goto unknown_op;
            }
        }
        break;
    case 0x121: /* mov reg, drN */
    case 0x123: /* mov drN, reg */
        if (s->cpl != 0) {
            gen_exception(s, EXCP0D_GPF);
        } else {
            modrm = x86_ldub_code(env, s);
            /* Ignore the mod bits (assume (modrm&0xc0)==0xc0).
             * AMD documentation (24594.pdf) and testing of
             * intel 386 and 486 processors all show that the mod bits
             * are assumed to be 1's, regardless of actual values.
             */
            rm = (modrm & 7) | REX_B(s);
            reg = ((modrm >> 3) & 7) | REX_R(s);
            if (CODE64(s))
                ot = MO_64;
            else
                ot = MO_32;
            if (reg >= 8) {
                goto illegal_op;
            }
            if (b & 2) {
                gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_WRITE_DR0 + reg);
                gen_op_mov_v_reg(s, ot, s->T0, rm);
                tcg_gen_movi_i32(s->tmp2_i32, reg);
                gen_helper_set_dr(cpu_env, s->tmp2_i32, s->T0);
                gen_jmp_im(s, s->pc - s->cs_base);
                gen_eob(s);
            } else {
                gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_READ_DR0 + reg);
                tcg_gen_movi_i32(s->tmp2_i32, reg);
                gen_helper_get_dr(s->T0, cpu_env, s->tmp2_i32);
                gen_op_mov_reg_v(s, ot, rm, s->T0);
            }
        }
        break;
    case 0x106: /* clts */
        if (s->cpl != 0) {
            gen_exception(s, EXCP0D_GPF);
        } else {
            gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_WRITE_CR0);
            gen_helper_clts(cpu_env);
            /* abort block because static cpu state changed */
            gen_jmp_im(s, s->pc - s->cs_base);
            gen_eob(s);
        }
        break;
    /* MMX/3DNow!/SSE/SSE2/SSE3/SSSE3/SSE4 support */
    case 0x1c3: /* MOVNTI reg, mem */
        if (!(s->cpuid_features & CPUID_SSE2))
            goto illegal_op;
        ot = mo_64_32(s->dflag);
        modrm = x86_ldub_code(env, s);
        mod = (modrm >> 6) & 3;
        if (mod == 3)
            goto illegal_op;
        reg = ((modrm >> 3) & 7) | REX_R(s);
        /* generate a generic store */
        gen_ldst_modrm(env, s, modrm, ot, reg, 1);
        break;
    case 0x1ae:
        modrm = x86_ldub_code(env, s);
        switch (modrm) {
        CASE_MODRM_MEM_OP(0): /* fxsave */
            if (!(s->cpuid_features & CPUID_FXSR)
                || (s->prefix & PREFIX_LOCK)) {
                goto illegal_op;
            }
            if ((s->flags & HF_EM_MASK) || (s->flags & HF_TS_MASK)) {
                gen_exception(s, EXCP07_PREX);
                break;
            }
            gen_lea_modrm(env, s, modrm);
            gen_helper_fxsave(cpu_env, s->A0);
            break;

        CASE_MODRM_MEM_OP(1): /* fxrstor */
            if (!(s->cpuid_features & CPUID_FXSR)
                || (s->prefix & PREFIX_LOCK)) {
                goto illegal_op;
            }
            if ((s->flags & HF_EM_MASK) || (s->flags & HF_TS_MASK)) {
                gen_exception(s, EXCP07_PREX);
                break;
            }
            gen_lea_modrm(env, s, modrm);
            gen_helper_fxrstor(cpu_env, s->A0);
            break;

        CASE_MODRM_MEM_OP(2): /* ldmxcsr */
            if ((s->flags & HF_EM_MASK) || !(s->flags & HF_OSFXSR_MASK)) {
                goto illegal_op;
            }
            if (s->flags & HF_TS_MASK) {
                gen_exception(s, EXCP07_PREX);
                break;
            }
            gen_lea_modrm(env, s, modrm);
            tcg_gen_qemu_ld_i32(s->tmp2_i32, s->A0, s->mem_index, MO_LEUL);
            gen_helper_ldmxcsr(cpu_env, s->tmp2_i32);
            break;

        CASE_MODRM_MEM_OP(3): /* stmxcsr */
            if ((s->flags & HF_EM_MASK) || !(s->flags & HF_OSFXSR_MASK)) {
                goto illegal_op;
            }
            if (s->flags & HF_TS_MASK) {
                gen_exception(s, EXCP07_PREX);
                break;
            }
            gen_lea_modrm(env, s, modrm);
            tcg_gen_ld32u_tl(s->T0, cpu_env, offsetof(CPUX86State, mxcsr));
            gen_op_st_v(s, MO_32, s->T0, s->A0);
            break;

        CASE_MODRM_MEM_OP(4): /* xsave */
            if ((s->cpuid_ext_features & CPUID_EXT_XSAVE) == 0
                || (s->prefix & (PREFIX_LOCK | PREFIX_DATA
                                 | PREFIX_REPZ | PREFIX_REPNZ))) {
                goto illegal_op;
            }
            gen_lea_modrm(env, s, modrm);
            tcg_gen_concat_tl_i64(s->tmp1_i64, cpu_regs[R_EAX],
                                  cpu_regs[R_EDX]);
            gen_helper_xsave(cpu_env, s->A0, s->tmp1_i64);
            break;

        CASE_MODRM_MEM_OP(5): /* xrstor */
            if ((s->cpuid_ext_features & CPUID_EXT_XSAVE) == 0
                || (s->prefix & (PREFIX_LOCK | PREFIX_DATA
                                 | PREFIX_REPZ | PREFIX_REPNZ))) {
                goto illegal_op;
            }
            gen_lea_modrm(env, s, modrm);
            tcg_gen_concat_tl_i64(s->tmp1_i64, cpu_regs[R_EAX],
                                  cpu_regs[R_EDX]);
            gen_helper_xrstor(cpu_env, s->A0, s->tmp1_i64);
            /* XRSTOR is how MPX is enabled, which changes how
               we translate.  Thus we need to end the TB.  */
            gen_update_cc_op(s);
            gen_jmp_im(s, s->pc - s->cs_base);
            gen_eob(s);
            break;

        CASE_MODRM_MEM_OP(6): /* xsaveopt / clwb */
            if (s->prefix & PREFIX_LOCK) {
                goto illegal_op;
            }
            if (s->prefix & PREFIX_DATA) {
                /* clwb */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_CLWB)) {
                    goto illegal_op;
                }
                gen_nop_modrm(env, s, modrm);
            } else {
                /* xsaveopt */
                if ((s->cpuid_ext_features & CPUID_EXT_XSAVE) == 0
                    || (s->cpuid_xsave_features & CPUID_XSAVE_XSAVEOPT) == 0
                    || (s->prefix & (PREFIX_REPZ | PREFIX_REPNZ))) {
                    goto illegal_op;
                }
                gen_lea_modrm(env, s, modrm);
                tcg_gen_concat_tl_i64(s->tmp1_i64, cpu_regs[R_EAX],
                                      cpu_regs[R_EDX]);
                gen_helper_xsaveopt(cpu_env, s->A0, s->tmp1_i64);
            }
            break;

        CASE_MODRM_MEM_OP(7): /* clflush / clflushopt */
            if (s->prefix & PREFIX_LOCK) {
                goto illegal_op;
            }
            if (s->prefix & PREFIX_DATA) {
                /* clflushopt */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_CLFLUSHOPT)) {
                    goto illegal_op;
                }
            } else {
                /* clflush */
                if ((s->prefix & (PREFIX_REPZ | PREFIX_REPNZ))
                    || !(s->cpuid_features & CPUID_CLFLUSH)) {
                    goto illegal_op;
                }
            }
            gen_nop_modrm(env, s, modrm);
            break;

        case 0xc0 ... 0xc7: /* rdfsbase (f3 0f ae /0) */
        case 0xc8 ... 0xcf: /* rdgsbase (f3 0f ae /1) */
        case 0xd0 ... 0xd7: /* wrfsbase (f3 0f ae /2) */
        case 0xd8 ... 0xdf: /* wrgsbase (f3 0f ae /3) */
            if (CODE64(s)
                && (s->prefix & PREFIX_REPZ)
                && !(s->prefix & PREFIX_LOCK)
                && (s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_FSGSBASE)) {
                TCGv base, treg, src, dst;

                /* Preserve hflags bits by testing CR4 at runtime.  */
                tcg_gen_movi_i32(s->tmp2_i32, CR4_FSGSBASE_MASK);
                gen_helper_cr4_testbit(cpu_env, s->tmp2_i32);

                base = cpu_seg_base[modrm & 8 ? R_GS : R_FS];
                treg = cpu_regs[(modrm & 7) | REX_B(s)];

                if (modrm & 0x10) {
                    /* wr*base */
                    dst = base, src = treg;
                } else {
                    /* rd*base */
                    dst = treg, src = base;
                }

                if (s->dflag == MO_32) {
                    tcg_gen_ext32u_tl(dst, src);
                } else {
                    tcg_gen_mov_tl(dst, src);
                }
                break;
            }
            goto unknown_op;

        case 0xf8: /* sfence / pcommit */
            if (s->prefix & PREFIX_DATA) {
                /* pcommit */
                if (!(s->cpuid_7_0_ebx_features & CPUID_7_0_EBX_PCOMMIT)
                    || (s->prefix & PREFIX_LOCK)) {
                    goto illegal_op;
                }
                break;
            }
            /* fallthru */
        case 0xf9 ... 0xff: /* sfence */
            if (!(s->cpuid_features & CPUID_SSE)
                || (s->prefix & PREFIX_LOCK)) {
                goto illegal_op;
            }
            tcg_gen_mb(TCG_MO_ST_ST | TCG_BAR_SC);
            break;
        case 0xe8 ... 0xef: /* lfence */
            if (!(s->cpuid_features & CPUID_SSE)
                || (s->prefix & PREFIX_LOCK)) {
                goto illegal_op;
            }
            tcg_gen_mb(TCG_MO_LD_LD | TCG_BAR_SC);
            break;
        case 0xf0 ... 0xf7: /* mfence */
            if (!(s->cpuid_features & CPUID_SSE2)
                || (s->prefix & PREFIX_LOCK)) {
                goto illegal_op;
            }
            tcg_gen_mb(TCG_MO_ALL | TCG_BAR_SC);
            break;

        default:
            goto unknown_op;
        }
        break;

    case 0x10d: /* 3DNow! prefetch(w) */
        modrm = x86_ldub_code(env, s);
        mod = (modrm >> 6) & 3;
        if (mod == 3)
            goto illegal_op;
        gen_nop_modrm(env, s, modrm);
        break;
    case 0x1aa: /* rsm */
        gen_svm_check_intercept(s, s->pc_start, SVM_EXIT_RSM);
        if (!(s->flags & HF_SMM_MASK))
            goto illegal_op;
        gen_update_cc_op(s);
        gen_jmp_im(s, s->pc - s->cs_base);
        gen_helper_rsm(cpu_env);
        gen_eob(s);
        break;
    case 0x1b8: /* SSE4.2 popcnt */
        if ((s->prefix & (PREFIX_REPZ | PREFIX_LOCK | PREFIX_REPNZ)) !=
            PREFIX_REPZ)
            goto illegal_op;
        if (!(s->cpuid_ext_features & CPUID_EXT_POPCNT))
            goto illegal_op;

        modrm = x86_ldub_code(env, s);
        reg = ((modrm >> 3) & 7) | REX_R(s);

        if (s->prefix & PREFIX_DATA) {
            ot = MO_16;
        } else {
            ot = mo_64_32(s->dflag);
        }

        gen_ldst_modrm(env, s, modrm, ot, OR_TMP0, 0);
        gen_extu(ot, s->T0);
        tcg_gen_mov_tl(cpu_cc_src, s->T0);
        tcg_gen_ctpop_tl(s->T0, s->T0);
        gen_op_mov_reg_v(s, ot, reg, s->T0);

        set_cc_op(s, CC_OP_POPCNT);
        break;
    case 0x10e ... 0x10f:
        /* 3DNow! instructions, ignore prefixes */
        s->prefix &= ~(PREFIX_REPZ | PREFIX_REPNZ | PREFIX_DATA);
        /* fall through */
    case 0x110 ... 0x117:
    case 0x128 ... 0x12f:
    case 0x138 ... 0x13a:
    case 0x150 ... 0x179:
    case 0x17c ... 0x17f:
    case 0x1c2:
    case 0x1c4 ... 0x1c6:
    case 0x1d0 ... 0x1fe:
        gen_sse_ng(env, s, b);
        break;
    default:
        goto unknown_op;
    }
    return s->pc;
 illegal_op:
    gen_illegal_opcode(s);
    return s->pc;
 unknown_op:
    gen_unknown_opcode(env, s);
    return s->pc;
}

void tcg_x86_init(void)
{
    static const char reg_names[CPU_NB_REGS][4] = {
#ifdef TARGET_X86_64
        [R_EAX] = "rax",
        [R_EBX] = "rbx",
        [R_ECX] = "rcx",
        [R_EDX] = "rdx",
        [R_ESI] = "rsi",
        [R_EDI] = "rdi",
        [R_EBP] = "rbp",
        [R_ESP] = "rsp",
        [8]  = "r8",
        [9]  = "r9",
        [10] = "r10",
        [11] = "r11",
        [12] = "r12",
        [13] = "r13",
        [14] = "r14",
        [15] = "r15",
#else
        [R_EAX] = "eax",
        [R_EBX] = "ebx",
        [R_ECX] = "ecx",
        [R_EDX] = "edx",
        [R_ESI] = "esi",
        [R_EDI] = "edi",
        [R_EBP] = "ebp",
        [R_ESP] = "esp",
#endif
    };
    static const char seg_base_names[6][8] = {
        [R_CS] = "cs_base",
        [R_DS] = "ds_base",
        [R_ES] = "es_base",
        [R_FS] = "fs_base",
        [R_GS] = "gs_base",
        [R_SS] = "ss_base",
    };
    static const char bnd_regl_names[4][8] = {
        "bnd0_lb", "bnd1_lb", "bnd2_lb", "bnd3_lb"
    };
    static const char bnd_regu_names[4][8] = {
        "bnd0_ub", "bnd1_ub", "bnd2_ub", "bnd3_ub"
    };
    int i;

    cpu_cc_op = tcg_global_mem_new_i32(cpu_env,
                                       offsetof(CPUX86State, cc_op), "cc_op");
    cpu_cc_dst = tcg_global_mem_new(cpu_env, offsetof(CPUX86State, cc_dst),
                                    "cc_dst");
    cpu_cc_src = tcg_global_mem_new(cpu_env, offsetof(CPUX86State, cc_src),
                                    "cc_src");
    cpu_cc_src2 = tcg_global_mem_new(cpu_env, offsetof(CPUX86State, cc_src2),
                                     "cc_src2");

    for (i = 0; i < CPU_NB_REGS; ++i) {
        cpu_regs[i] = tcg_global_mem_new(cpu_env,
                                         offsetof(CPUX86State, regs[i]),
                                         reg_names[i]);
    }

    for (i = 0; i < 6; ++i) {
        cpu_seg_base[i]
            = tcg_global_mem_new(cpu_env,
                                 offsetof(CPUX86State, segs[i].base),
                                 seg_base_names[i]);
    }

    for (i = 0; i < 4; ++i) {
        cpu_bndl[i]
            = tcg_global_mem_new_i64(cpu_env,
                                     offsetof(CPUX86State, bnd_regs[i].lb),
                                     bnd_regl_names[i]);
        cpu_bndu[i]
            = tcg_global_mem_new_i64(cpu_env,
                                     offsetof(CPUX86State, bnd_regs[i].ub),
                                     bnd_regu_names[i]);
    }
}

static void i386_tr_init_disas_context(DisasContextBase *dcbase, CPUState *cpu)
{
    DisasContext *dc = container_of(dcbase, DisasContext, base);
    CPUX86State *env = cpu->env_ptr;
    uint32_t flags = dc->base.tb->flags;
    target_ulong cs_base = dc->base.tb->cs_base;

    dc->pe = (flags >> HF_PE_SHIFT) & 1;
    dc->code32 = (flags >> HF_CS32_SHIFT) & 1;
    dc->ss32 = (flags >> HF_SS32_SHIFT) & 1;
    dc->addseg = (flags >> HF_ADDSEG_SHIFT) & 1;
    dc->f_st = 0;
    dc->vm86 = (flags >> VM_SHIFT) & 1;
    dc->cpl = (flags >> HF_CPL_SHIFT) & 3;
    dc->iopl = (flags >> IOPL_SHIFT) & 3;
    dc->tf = (flags >> TF_SHIFT) & 1;
    dc->cc_op = CC_OP_DYNAMIC;
    dc->cc_op_dirty = false;
    dc->cs_base = cs_base;
    dc->popl_esp_hack = 0;
    /* select memory access functions */
    dc->mem_index = 0;
#ifdef CONFIG_SOFTMMU
    dc->mem_index = cpu_mmu_index(env, false);
#endif
    dc->cpuid_features = env->features[FEAT_1_EDX];
    dc->cpuid_ext_features = env->features[FEAT_1_ECX];
    dc->cpuid_ext2_features = env->features[FEAT_8000_0001_EDX];
    dc->cpuid_ext3_features = env->features[FEAT_8000_0001_ECX];
    dc->cpuid_7_0_ebx_features = env->features[FEAT_7_0_EBX];
    dc->cpuid_xsave_features = env->features[FEAT_XSAVE];
#ifdef TARGET_X86_64
    dc->lma = (flags >> HF_LMA_SHIFT) & 1;
    dc->code64 = (flags >> HF_CS64_SHIFT) & 1;
#endif
    dc->flags = flags;
    dc->jmp_opt = !(dc->tf || dc->base.singlestep_enabled ||
                    (flags & HF_INHIBIT_IRQ_MASK));
    /* Do not optimize repz jumps at all in icount mode, because
       rep movsS instructions are execured with different paths
       in !repz_opt and repz_opt modes. The first one was used
       always except single step mode. And this setting
       disables jumps optimization and control paths become
       equivalent in run and single step modes.
       Now there will be no jump optimization for repz in
       record/replay modes and there will always be an
       additional step for ecx=0 when icount is enabled.
     */
    dc->repz_opt = !dc->jmp_opt && !(tb_cflags(dc->base.tb) & CF_USE_ICOUNT);
#if 0
    /* check addseg logic */
    if (!dc->addseg && (dc->vm86 || !dc->pe || !dc->code32))
        printf("ERROR addseg\n");
#endif

    dc->T0 = tcg_temp_new();
    dc->T1 = tcg_temp_new();
    dc->A0 = tcg_temp_new();

    dc->tmp0 = tcg_temp_new();
    dc->tmp1_i64 = tcg_temp_new_i64();
    dc->tmp2_i32 = tcg_temp_new_i32();
    dc->tmp3_i32 = tcg_temp_new_i32();
    dc->tmp4 = tcg_temp_new();
    dc->ptr0 = tcg_temp_new_ptr();
    dc->ptr1 = tcg_temp_new_ptr();
    dc->cc_srcT = tcg_temp_local_new();
}

static void i386_tr_tb_start(DisasContextBase *db, CPUState *cpu)
{
}

static void i386_tr_insn_start(DisasContextBase *dcbase, CPUState *cpu)
{
    DisasContext *dc = container_of(dcbase, DisasContext, base);

    tcg_gen_insn_start(dc->base.pc_next, dc->cc_op);
}

static bool i386_tr_breakpoint_check(DisasContextBase *dcbase, CPUState *cpu,
                                     const CPUBreakpoint *bp)
{
    DisasContext *dc = container_of(dcbase, DisasContext, base);
    /* If RF is set, suppress an internally generated breakpoint.  */
    int flags = dc->base.tb->flags & HF_RF_MASK ? BP_GDB : BP_ANY;
    if (bp->flags & flags) {
        gen_debug(dc, dc->base.pc_next - dc->cs_base);
        dc->base.is_jmp = DISAS_NORETURN;
        /* The address covered by the breakpoint must be included in
           [tb->pc, tb->pc + tb->size) in order to for it to be
           properly cleared -- thus we increment the PC here so that
           the generic logic setting tb->size later does the right thing.  */
        dc->base.pc_next += 1;
        return true;
    } else {
        return false;
    }
}

static void i386_tr_translate_insn(DisasContextBase *dcbase, CPUState *cpu)
{
    DisasContext *dc = container_of(dcbase, DisasContext, base);
    target_ulong pc_next = disas_insn(dc, cpu);

    if (dc->tf || (dc->base.tb->flags & HF_INHIBIT_IRQ_MASK)) {
        /* if single step mode, we generate only one instruction and
           generate an exception */
        /* if irq were inhibited with HF_INHIBIT_IRQ_MASK, we clear
           the flag and abort the translation to give the irqs a
           chance to happen */
        dc->base.is_jmp = DISAS_TOO_MANY;
    } else if ((tb_cflags(dc->base.tb) & CF_USE_ICOUNT)
               && ((pc_next & TARGET_PAGE_MASK)
                   != ((pc_next + TARGET_MAX_INSN_SIZE - 1)
                       & TARGET_PAGE_MASK)
                   || (pc_next & ~TARGET_PAGE_MASK) == 0)) {
        /* Do not cross the boundary of the pages in icount mode,
           it can cause an exception. Do it only when boundary is
           crossed by the first instruction in the block.
           If current instruction already crossed the bound - it's ok,
           because an exception hasn't stopped this code.
         */
        dc->base.is_jmp = DISAS_TOO_MANY;
    } else if ((pc_next - dc->base.pc_first) >= (TARGET_PAGE_SIZE - 32)) {
        dc->base.is_jmp = DISAS_TOO_MANY;
    }

    dc->base.pc_next = pc_next;
}

static void i386_tr_tb_stop(DisasContextBase *dcbase, CPUState *cpu)
{
    DisasContext *dc = container_of(dcbase, DisasContext, base);

    if (dc->base.is_jmp == DISAS_TOO_MANY) {
        gen_jmp_im(dc, dc->base.pc_next - dc->cs_base);
        gen_eob(dc);
    }
}

static void i386_tr_disas_log(const DisasContextBase *dcbase,
                              CPUState *cpu)
{
    DisasContext *dc = container_of(dcbase, DisasContext, base);

    qemu_log("IN: %s\n", lookup_symbol(dc->base.pc_first));
    log_target_disas(cpu, dc->base.pc_first, dc->base.tb->size);
}

static const TranslatorOps i386_tr_ops = {
    .init_disas_context = i386_tr_init_disas_context,
    .tb_start           = i386_tr_tb_start,
    .insn_start         = i386_tr_insn_start,
    .breakpoint_check   = i386_tr_breakpoint_check,
    .translate_insn     = i386_tr_translate_insn,
    .tb_stop            = i386_tr_tb_stop,
    .disas_log          = i386_tr_disas_log,
};

/* generate intermediate code for basic block 'tb'.  */
void gen_intermediate_code(CPUState *cpu, TranslationBlock *tb, int max_insns)
{
    DisasContext dc;

    translator_loop(&i386_tr_ops, &dc.base, cpu, tb, max_insns);
}

void restore_state_to_opc(CPUX86State *env, TranslationBlock *tb,
                          target_ulong *data)
{
    int cc_op = data[1];
    env->eip = data[0] - tb->cs_base;
    if (cc_op != CC_OP_DYNAMIC) {
        env->cc_op = cc_op;
    }
}
