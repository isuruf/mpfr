/* mpfr_sub1sp -- internal function to perform a "real" subtraction
   All the op must have the same precision

Copyright 2003-2016 Free Software Foundation, Inc.
Contributed by the AriC and Caramba projects, INRIA.

This file is part of the GNU MPFR Library.

The GNU MPFR Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at your
option) any later version.

The GNU MPFR Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details.

You should have received a copy of the GNU Lesser General Public License
along with the GNU MPFR Library; see the file COPYING.LESSER.  If not, see
http://www.gnu.org/licenses/ or write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA. */

#define MPFR_NEED_LONGLONG_H
#include "mpfr-impl.h"

/* Check if we have to check the result of mpfr_sub1sp with mpfr_sub1 */
#if MPFR_WANT_ASSERT >= 2

int mpfr_sub1sp_ref (mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sub1sp (mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c, mpfr_rnd_t rnd_mode)
{
  mpfr_t tmpa, tmpb, tmpc;
  mpfr_flags_t old_flags, flags, flags2;
  int inexb, inexc, inexact, inexact2;

  old_flags = __gmpfr_flags;

  mpfr_init2 (tmpa, MPFR_PREC (a));
  mpfr_init2 (tmpb, MPFR_PREC (b));
  mpfr_init2 (tmpc, MPFR_PREC (c));

  inexb = mpfr_set (tmpb, b, MPFR_RNDN);
  MPFR_ASSERTN (inexb == 0);

  inexc = mpfr_set (tmpc, c, MPFR_RNDN);
  MPFR_ASSERTN (inexc == 0);

  MPFR_ASSERTN (__gmpfr_flags == old_flags);

  inexact2 = mpfr_sub1 (tmpa, tmpb, tmpc, rnd_mode);
  flags2 = __gmpfr_flags;

  __gmpfr_flags = old_flags;
  inexact = mpfr_sub1sp_ref (a, b, c, rnd_mode);
  flags = __gmpfr_flags;

  if (! mpfr_equal_p (tmpa, a) || inexact != inexact2 || flags != flags2)
    {
      fprintf (stderr, "sub1 & sub1sp return different values for %s\n"
               "Prec_a = %lu, Prec_b = %lu, Prec_c = %lu\nB = ",
               mpfr_print_rnd_mode (rnd_mode),
               (unsigned long) MPFR_PREC (a),
               (unsigned long) MPFR_PREC (b),
               (unsigned long) MPFR_PREC (c));
      mpfr_fprint_binary (stderr, tmpb);
      fprintf (stderr, "\nC = ");
      mpfr_fprint_binary (stderr, tmpc);
      fprintf (stderr, "\nsub1  : ");
      mpfr_fprint_binary (stderr, tmpa);
      fprintf (stderr, "\nsub1sp: ");
      mpfr_fprint_binary (stderr, a);
      fprintf (stderr, "\nInexact sp = %d | Inexact = %d\n"
               "Flags sp = %u | Flags = %u\n",
               inexact, inexact2, flags, flags2);
      MPFR_ASSERTN (0);
    }
  mpfr_clears (tmpa, tmpb, tmpc, (mpfr_ptr) 0);
  return inexact;
}
# define mpfr_sub1sp mpfr_sub1sp_ref
#endif  /* MPFR_WANT_ASSERT >= 2 */

/* Debugging support */
#ifdef DEBUG
# undef DEBUG
# define DEBUG(x) (x)
#else
# define DEBUG(x) /**/
#endif

/* Rounding Sub */

/*
   compute sgn(b)*(|b| - |c|) if |b|>|c| else -sgn(b)*(|c| -|b|)
   Returns 0 iff result is exact,
   a negative value when the result is less than the exact value,
   a positive value otherwise.
*/

/* A0...Ap-1
 *          Cp Cp+1 ....
 *             <- C'p+1 ->
 * Cp = -1 if calculated from c mantissa
 * Cp = 0  if 0 from a or c
 * Cp = 1  if calculated from a.
 * C'p+1 = First bit not null or 0 if there isn't one
 *
 * Can't have Cp=-1 and C'p+1=1*/

/* RND = MPFR_RNDZ:
 *  + if Cp=0 and C'p+1=0,1,  Truncate.
 *  + if Cp=0 and C'p+1=-1,   SubOneUlp
 *  + if Cp=-1,               SubOneUlp
 *  + if Cp=1,                AddOneUlp
 * RND = MPFR_RNDA (Away)
 *  + if Cp=0 and C'p+1=0,-1, Truncate
 *  + if Cp=0 and C'p+1=1,    AddOneUlp
 *  + if Cp=1,                AddOneUlp
 *  + if Cp=-1,               Truncate
 * RND = MPFR_RNDN
 *  + if Cp=0,                Truncate
 *  + if Cp=1 and C'p+1=1,    AddOneUlp
 *  + if Cp=1 and C'p+1=-1,   Truncate
 *  + if Cp=1 and C'p+1=0,    Truncate if Ap-1=0, AddOneUlp else
 *  + if Cp=-1 and C'p+1=-1,  SubOneUlp
 *  + if Cp=-1 and C'p+1=0,   Truncate if Ap-1=0, SubOneUlp else
 *
 * If AddOneUlp:
 *   If carry, then it is 11111111111 + 1 = 10000000000000
 *      ap[n-1]=MPFR_HIGHT_BIT
 * If SubOneUlp:
 *   If we lose one bit, it is 1000000000 - 1 = 0111111111111
 *      Then shift, and put as last bit x which is calculated
 *              according Cp, Cp-1 and rnd_mode.
 * If Truncate,
 *    If it is a power of 2,
 *       we may have to suboneulp in some special cases.
 *
 * To simplify, we don't use Cp = 1.
 *
 */

/* special code for p < GMP_NUMB_BITS */
static int
mpfr_sub1sp1 (mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c, mpfr_rnd_t rnd_mode,
              mpfr_prec_t p)
{
  mpfr_exp_t bx = MPFR_GET_EXP (b);
  mpfr_exp_t cx = MPFR_GET_EXP (c);
  mp_limb_t *ap = MPFR_MANT(a);
  mp_limb_t *bp = MPFR_MANT(b);
  mp_limb_t *cp = MPFR_MANT(c);
  mpfr_prec_t cnt, INITIALIZED(sh);
  mp_limb_t rb; /* round bit */
  mp_limb_t sb; /* sticky bit */
  mp_limb_t mask;
  mpfr_uexp_t d;

  MPFR_ASSERTD(p < GMP_NUMB_BITS);

  if (bx == cx)
    {
      mp_limb_t b0 = bp[0]; /* save bp[0] if a = b */

      ap[0] = b0 - cp[0];
      if (ap[0] == 0) /* result is zero */
        {
          if (rnd_mode == MPFR_RNDD)
            MPFR_SET_NEG(a);
          else
            MPFR_SET_POS(a);
          MPFR_SET_ZERO(a);
          return 0; /* same as MPFR_RET(0) but faster */
        }
      else if (ap[0] > b0) /* borrow: |c| > |b| */
        {
          MPFR_SET_OPPOSITE_SIGN (a, b);
          ap[0] = -ap[0];
        }
      else /* bp[0] > cp[0] */
        MPFR_SET_SAME_SIGN (a, b);

      /* now ap[0] != 0 */
      MPFR_ASSERTD(ap[0] != 0);
      count_leading_zeros (cnt, ap[0]);
      ap[0] <<= cnt;
      bx -= cnt;
      rb = sb = 0;
      /* Note: sh is not initialized, but will not be used in this case. */
    }
  else if (bx > cx)
    {
      MPFR_SET_SAME_SIGN (a, b);
    BGreater1:
      d = (mpfr_uexp_t) bx - cx;
      sh = GMP_NUMB_BITS - p;
      mask = MPFR_LIMB_MASK(sh);
      if (d < GMP_NUMB_BITS)
        {
          sb = cp[0] << (GMP_NUMB_BITS - d); /* neglected part of c */
          ap[0] = bp[0] - (cp[0] >> d);
          if (sb)
            {
              ap[0] --;
              /* ap[0] cannot become zero here since:
                 a) if d >= 2, then ap[0] >= 2^w - (2^(w-1)-1) with
                    w = GMP_NUMB_BITS, thus ap[0] - 1 >= 2^(w-1),
                 b) if d = 1, then since p < GMP_NUMB_BITS we have sb=0.
              */
              MPFR_ASSERTD(ap[0] > 0);
              sb = -sb;
            }
          count_leading_zeros (cnt, ap[0]);
          if (cnt)
            ap[0] = (ap[0] << cnt) | (sb >> (GMP_NUMB_BITS - cnt));
          sb <<= cnt;
          bx -= cnt;
          /* sh > 0 since p < GMP_NUMB_BITS */
          MPFR_ASSERTD(sh > 0);
          rb = ap[0] & (MPFR_LIMB_ONE << (sh - 1));
          sb |= (ap[0] & mask) ^ rb;
          ap[0] = ap[0] & ~mask;
        }
      else /* d >= GMP_NUMB_BITS */
        {
          /* We compute b - ulp(b), and the remainder ulp(b) - c satisfies:
             1/2 ulp(b) < ulp(b) - c < ulp(b), thus rb = sb = 1. */
          if (bp[0] > MPFR_LIMB_HIGHBIT)
            ap[0] = bp[0] - (MPFR_LIMB_ONE << sh);
          else
            {
              ap[0] = ~mask;
              bx --;
            }
          rb = sb = 1;
        }
    }
  else /* cx > bx */
    {
      mpfr_exp_t tx;
      mp_limb_t *tp;
      tx = bx; bx = cx; cx = tx;
      tp = bp; bp = cp; cp = tp;
      MPFR_SET_OPPOSITE_SIGN (a, b);
      goto BGreater1;
    }

  /* now perform rounding */

  /* Warning: MPFR considers underflow *after* rounding with an unbounded
     exponent range. However since b and c have same precision p, they are
     multiples of 2^(emin-p), likewise for b-c. Thus if bx < emin, the
     subtraction (with an unbounded exponent range) is exact, so that bx is
     also the exponent after rounding with an unbounded exponent range. */
  if (MPFR_UNLIKELY(bx < __gmpfr_emin))
    {
      /* for RNDN, mpfr_underflow always rounds away, thus for |a| <= 2^(emin-2)
         we have to change to RNDZ */
      if (rnd_mode == MPFR_RNDN &&
          (bx < __gmpfr_emin - 1 || ap[0] == MPFR_LIMB_HIGHBIT))
        rnd_mode = MPFR_RNDZ;
      return mpfr_underflow (a, rnd_mode, MPFR_SIGN(a));
    }

  MPFR_SET_EXP (a, bx);
  if (rb == 0 && sb == 0)
    return 0; /* idem than MPFR_RET(0) but faster */
  else if (rnd_mode == MPFR_RNDN)
    {
      if (rb == 0 || (rb && sb == 0 &&
                      (ap[0] & (MPFR_LIMB_ONE << sh)) == 0))
        goto truncate;
      else
        goto add_one_ulp;
    }
  else if (MPFR_IS_LIKE_RNDZ(rnd_mode, MPFR_IS_NEG(a)))
    {
    truncate:
      MPFR_RET(-MPFR_SIGN(a));
    }
  else /* round away from zero */
    {
    add_one_ulp:
      ap[0] += MPFR_LIMB_ONE << sh;
      if (ap[0] == 0)
        {
          ap[0] = MPFR_LIMB_HIGHBIT;
          /* Note: bx+1 cannot exceed __gmpfr_emax, since |a| <= |b|, thus
             bx+1 is at most equal to the original exponent of b. */
          MPFR_ASSERTD(bx + 1 <= __gmpfr_emax);
          MPFR_SET_EXP (a, bx + 1);
        }
      MPFR_RET(MPFR_SIGN(a));
    }
}

/* special code for GMP_NUMB_BITS < p < 2*GMP_NUMB_BITS */
static int
mpfr_sub1sp2 (mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c, mpfr_rnd_t rnd_mode,
              mpfr_prec_t p)
{
  mpfr_exp_t bx = MPFR_GET_EXP (b);
  mpfr_exp_t cx = MPFR_GET_EXP (c);
  mp_limb_t *ap = MPFR_MANT(a);
  mp_limb_t *bp = MPFR_MANT(b);
  mp_limb_t *cp = MPFR_MANT(c);
  mpfr_prec_t cnt, INITIALIZED(sh);
  mp_limb_t rb; /* round bit */
  mp_limb_t sb; /* sticky bit */
  mp_limb_t mask, b0, b1;
  mpfr_uexp_t d;

  MPFR_ASSERTD(GMP_NUMB_BITS < p && p < 2 * GMP_NUMB_BITS);

  /* save b0, b1 in case a and b are the same variable */
  if (bx == cx) /* subtraction is exact in this case */
    {
      b0 = bp[0];
      b1 = bp[1];
      ap[0] = b0 - cp[0];
      ap[1] = b1 - cp[1] - (ap[0] > b0);
      if (ap[1] == 0 && ap[0] == 0) /* result is zero */
        {
          if (rnd_mode == MPFR_RNDD)
            MPFR_SET_NEG(a);
          else
            MPFR_SET_POS(a);
          MPFR_SET_ZERO(a);
          return 0; /* same as MPFR_RET(0) but faster */
        }
      else if (ap[1] >= b1) /* borrow: |c| > |b| */
        {
          MPFR_SET_OPPOSITE_SIGN (a, b);
          /* a = b-c mod 2^(2*GMP_NUMB_BITS) */
          ap[1] = -ap[1] - (ap[0] != 0);
          ap[0] = -ap[0];
        }
      else /* bp[0] > cp[0] */
        MPFR_SET_SAME_SIGN (a, b);

      if (ap[1] == 0)
        {
          ap[1] = ap[0];
          ap[0] = 0;
          bx -= GMP_NUMB_BITS;
        }

      /* now ap[1] != 0 */
      MPFR_ASSERTD(ap[1] != 0);
      count_leading_zeros (cnt, ap[1]);
      if (cnt)
        {
          ap[1] = (ap[1] << cnt) | (ap[0] >> (GMP_NUMB_BITS - cnt));
          ap[0] = ap[0] << cnt;
          bx -= cnt;
        }
      rb = sb = 0;
      /* Note: sh is not initialized, but will not be used in this case. */
    }
  else if (bx > cx)
    {
      MPFR_SET_SAME_SIGN (a, b);
    BGreater2:
      b0 = bp[0];
      b1 = bp[1];
      d = (mpfr_uexp_t) bx - cx;
      sh =  2 * GMP_NUMB_BITS - p;
      mask = MPFR_LIMB_MASK(sh);
      if (d < GMP_NUMB_BITS)
        {
          sb = cp[0] << (GMP_NUMB_BITS - d); /* neglected part of c */
          ap[0] = b0 - ((cp[0] >> d) | (cp[1] << (GMP_NUMB_BITS - d)));
          ap[1] = b1 - (cp[1] >> d) - (ap[0] > b0);
          if (sb)
            {
              ap[1] -= (ap[0] == 0);
              ap[0] --;
              /* a = ap[1],ap[0] cannot become zero here */
              MPFR_ASSERTD(ap[1] > 0 || ap[0] > 0);
              sb = -sb;
            }
          if (ap[1] == 0)
            {
              ap[1] = ap[0];
              ap[0] = sb;
              sb = 0;
              bx -= GMP_NUMB_BITS;
            }
          /* now ap[1] != 0 */
          MPFR_ASSERTD(ap[1] != 0);
          count_leading_zeros (cnt, ap[1]);
          if (cnt)
            {
              ap[1] = (ap[1] << cnt) | (ap[0] >> (GMP_NUMB_BITS - cnt));
              ap[0] = (ap[0] << cnt) | (sb >> (GMP_NUMB_BITS - cnt));
              sb <<= cnt;
              bx -= cnt;
            }
          /* sh > 0 since p < 2*GMP_NUMB_BITS */
          MPFR_ASSERTD(sh > 0);
          rb = ap[0] & (MPFR_LIMB_ONE << (sh - 1));
          sb |= (ap[0] & mask) ^ rb;
          ap[0] = ap[0] & ~mask;
        }
      else if (d < 2 * GMP_NUMB_BITS)
        {  /* GMP_NUMB_BITS <= d < 2*GMP_NUMB_BITS */
          /* warning: the most significant bit of sb might become the least
             significant bit of ap[0] below */
          sb = (d == GMP_NUMB_BITS) ? cp[0]
            : (cp[1] << (2*GMP_NUMB_BITS - d)) | (cp[0] != 0);
          ap[0] = b0 - (cp[1] >> (d - GMP_NUMB_BITS)) - (sb != 0);
          ap[1] = b1 - (ap[0] >= b0); /* since cp[1] has its most significant bit
                                         set, and d-GMP_NUMB_BITS < GMP_NUMB_BITS,
                                         cp[1] >> (d - GMP_NUMB_BITS) is always
                                         non-zero, thus a borrow occurs iff
                                         ap[0] >= b0 */
          sb = -sb;
          /* since has its most significant bit set, we can have an exponent
             decrease of at most one */
          if (ap[1] < MPFR_LIMB_HIGHBIT)
            {
              ap[1] = (ap[1] << 1) | (ap[0] >> (GMP_NUMB_BITS - 1));
              ap[0] = (ap[0] << 1) | (sb >> (GMP_NUMB_BITS - 1));
              bx --;
            }
          rb = ap[0] & (MPFR_LIMB_ONE << (sh - 1));
          sb |= (ap[0] & mask) ^ rb;
          ap[0] = ap[0] & ~mask;
        }
      else /* d >= 2*GMP_NUMB_BITS */
        {
          /* We compute b - ulp(b), and the remainder ulp(b) - c satisfies:
             1/2 ulp(b) < ulp(b) - c < ulp(b), thus rb = sb = 1. */
          ap[0] = b0 - (MPFR_LIMB_ONE << sh);
          ap[1] = b1 - (ap[0] > b0);
          if (ap[1] < MPFR_LIMB_HIGHBIT)
            {
              /* necessarily we had b = 1000...000 */
              ap[0] = ~mask;
              ap[1] = ~0;
              bx --;
            }
          rb = sb = 1;
        }
    }
  else /* cx > bx */
    {
      mpfr_exp_t tx;
      mp_limb_t *tp;
      tx = bx; bx = cx; cx = tx;
      tp = bp; bp = cp; cp = tp;
      MPFR_SET_OPPOSITE_SIGN (a, b);
      goto BGreater2;
    }

  /* now perform rounding */

  /* Warning: MPFR considers underflow *after* rounding with an unbounded
     exponent range. However since b and c have same precision p, they are
     multiples of 2^(emin-p), likewise for b-c. Thus if bx < emin, the
     subtraction (with an unbounded exponent range) is exact, so that bx is
     also the exponent after rounding with an unbounded exponent range. */
  if (MPFR_UNLIKELY(bx < __gmpfr_emin))
    {
      /* for RNDN, mpfr_underflow always rounds away, thus for |a|<=2^(emin-2)
         we have to change to RNDZ */
      if (rnd_mode == MPFR_RNDN &&
          (bx < __gmpfr_emin - 1 ||
           (ap[1] == MPFR_LIMB_HIGHBIT && ap[0] == 0)))
        rnd_mode = MPFR_RNDZ;
      return mpfr_underflow (a, rnd_mode, MPFR_SIGN(a));
    }

  MPFR_SET_EXP (a, bx);
  if (rb == 0 && sb == 0)
    return 0; /* idem than MPFR_RET(0) but faster */
  else if (rnd_mode == MPFR_RNDN)
    {
      if (rb == 0 || (rb && sb == 0 &&
                      (ap[0] & (MPFR_LIMB_ONE << sh)) == 0))
        goto truncate;
      else
        goto add_one_ulp;
    }
  else if (MPFR_IS_LIKE_RNDZ(rnd_mode, MPFR_IS_NEG(a)))
    {
    truncate:
      MPFR_RET(-MPFR_SIGN(a));
    }
  else /* round away from zero */
    {
    add_one_ulp:
      ap[0] += MPFR_LIMB_ONE << sh;
      ap[1] += (ap[0] == 0);
      if (ap[1] == 0)
        {
          ap[1] = MPFR_LIMB_HIGHBIT;
          /* Note: bx+1 cannot exceed __gmpfr_emax, since |a| <= |b|, thus
             bx+1 is at most equal to the original exponent of b. */
          MPFR_ASSERTD(bx + 1 <= __gmpfr_emax);
          MPFR_SET_EXP (a, bx + 1);
        }
      MPFR_RET(MPFR_SIGN(a));
    }
}

MPFR_HOT_FUNCTION_ATTR int
mpfr_sub1sp (mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c, mpfr_rnd_t rnd_mode)
{
  mpfr_exp_t bx, cx;
  mpfr_uexp_t d;
  mpfr_prec_t p, sh, cnt;
  mp_size_t n;
  mp_limb_t *ap, *bp, *cp;
  mp_limb_t limb;
  int inexact;
  mp_limb_t bcp,bcp1; /* Cp and C'p+1 */
  mp_limb_t bbcp = MPFR_LIMB_MAX, bbcp1 = MPFR_LIMB_MAX; /* Cp+1 and C'p+2,
    gcc claims that they might be used uninitialized. We fill them with invalid
    values, which should produce a failure if so. See README.dev file. */

  MPFR_TMP_DECL(marker);

  MPFR_ASSERTD(MPFR_PREC(a) == MPFR_PREC(b) && MPFR_PREC(b) == MPFR_PREC(c));
  MPFR_ASSERTD(MPFR_IS_PURE_FP(b));
  MPFR_ASSERTD(MPFR_IS_PURE_FP(c));

  /* Read prec and num of limbs */
  p = MPFR_GET_PREC (b);

  /* special case for p < GMP_NUMB_BITS */
  if (p < GMP_NUMB_BITS)
    return mpfr_sub1sp1 (a, b, c, rnd_mode, p);

  /* special case for GMP_NUMB_BITS < p < 2*GMP_NUMB_BITS */
  if (GMP_NUMB_BITS < p && p < 2 * GMP_NUMB_BITS)
    return mpfr_sub1sp2 (a, b, c, rnd_mode, p);

  n = MPFR_PREC2LIMBS (p);
  /* Fast cmp of |b| and |c| */
  bx = MPFR_GET_EXP (b);
  cx = MPFR_GET_EXP (c);

  MPFR_TMP_MARK(marker);

  if (bx == cx)
    {
      mp_size_t k = n - 1;
      /* Check mantissa since exponents are equal */
      bp = MPFR_MANT(b);
      cp = MPFR_MANT(c);
      while (k >= 0 && MPFR_UNLIKELY(bp[k] == cp[k]))
        k--;
      if (k < 0)
        /* b == c ! */
        {
          /* Return exact number 0 */
          if (rnd_mode == MPFR_RNDD)
            MPFR_SET_NEG(a);
          else
            MPFR_SET_POS(a);
          MPFR_SET_ZERO(a);
          MPFR_RET(0);
        }
      else if (bp[k] > cp[k])
        goto BGreater;
      else
        {
          MPFR_ASSERTD(bp[k] < cp[k]);
          goto CGreater;
        }
    }
  else if (bx < cx)
    {
      /* Swap b and c and set sign */
      mpfr_srcptr t;
      mpfr_exp_t tx;
    CGreater:
      MPFR_SET_OPPOSITE_SIGN(a,b);
      t  = b;  b  = c;  c  = t;
      tx = bx; bx = cx; cx = tx;
    }
  else
    {
      /* |b| > |c| */
    BGreater:
      MPFR_SET_SAME_SIGN(a,b);
    }

  /* Now |b| > |c| */
  MPFR_ASSERTD(bx >= cx);
  d = (mpfr_uexp_t) bx - cx;
  DEBUG (printf ("New with diff=%lu\n", (unsigned long) d));

  if (d <= 1)
    {
      if (d == 0)
        {
          /* <-- b -->
             <-- c --> : exact sub */
          ap = MPFR_MANT(a);
          mpn_sub_n (ap, MPFR_MANT(b), MPFR_MANT(c), n);
          /* Normalize */
        ExactNormalize:
          limb = ap[n-1];
          if (MPFR_LIKELY (limb != 0))
            {
              /* First limb is not zero. */
              count_leading_zeros(cnt, limb);
              /* cnt could be == 0 <= SubD1Lose */
              if (MPFR_LIKELY(cnt))
                {
                  mpn_lshift(ap, ap, n, cnt); /* Normalize number */
                  bx -= cnt; /* Update final expo */
                }
              /* Last limb should be OK */
              MPFR_ASSERTD(!(ap[0] & MPFR_LIMB_MASK((unsigned int) (-p)
                                                    % GMP_NUMB_BITS)));
            }
          else
            {
              /* First limb is zero */
              mp_size_t k = n-1, len;
              /* Find the first limb not equal to zero. It necessarily exists
                 since |b| > |c|. */
              do
                {
                  MPFR_ASSERTD( k > 0 );
                  limb = ap[--k];
                }
              while (limb == 0);
              MPFR_ASSERTD(limb != 0);
              count_leading_zeros(cnt, limb);
              k++;
              len = n - k; /* Number of last limb */
              MPFR_ASSERTD(k >= 0);
              if (cnt)
                mpn_lshift (ap + len, ap, k, cnt); /* Normalize the High Limb*/
              else
                {
                  /* Must use DECR since src and dest may overlap & dest>=src*/
                  MPN_COPY_DECR(ap+len, ap, k);
                }
              MPN_ZERO(ap, len); /* Zeroing the last limbs */
              bx -= cnt + len*GMP_NUMB_BITS; /* Update Expo */
              /* Last limb should be OK */
              MPFR_ASSERTD(!(ap[len] & MPFR_LIMB_MASK((unsigned int) (-p)
                                                      % GMP_NUMB_BITS)));
            }
          /* Check expo underflow */
          if (MPFR_UNLIKELY(bx < __gmpfr_emin))
            {
              MPFR_TMP_FREE(marker);
              /* since b and c have same sign, exponent and precision, the
                 subtraction is exact */
              DEBUG( printf("(D==0 Underflow)\n") );
              /* for MPFR_RNDN, mpfr_underflow always rounds away from zero,
                 thus for |a| <= 2^(emin-2) we change to RNDZ. */
              if (rnd_mode == MPFR_RNDN &&
                  (bx < __gmpfr_emin - 1 || mpfr_powerof2_raw (a)))
                rnd_mode = MPFR_RNDZ;
              return mpfr_underflow (a, rnd_mode, MPFR_SIGN(a));
            }
          MPFR_SET_EXP (a, bx);
          /* No rounding is necessary since the result is exact */
          MPFR_ASSERTD(ap[n-1] > ~ap[n-1]);
          MPFR_TMP_FREE(marker);
          return 0;
        }
      else /* if (d == 1) */
        {
          /* | <-- b -->
             |  <-- c --> */
          mp_limb_t c0, mask;
          mp_size_t k;
          MPFR_UNSIGNED_MINUS_MODULO(sh, p);
          /* If we lose at least one bit, compute 2*b-c (Exact)
           * else compute b-c/2 */
          bp = MPFR_MANT(b);
          cp = MPFR_MANT(c);
          k = n-1;
          limb = bp[k] - cp[k]/2;
          /* we have |b|-|c| >= limb*W^k - (2*W^k-1)/2 >= limb*W^k - W^k + 1/2
             thus if limb > W^k/2, |b|-|c| >= 1/2*W^n.
             Moreover if trunc(|c|) represents the first p-1 bits of |c|,
             minus the last significant bit called c0 below, then we have
             |b|-trunc(|c|) >= 1/2*W^n+1, thus the two mpn_sub_n calls
             below necessarily yield a > 1/2*W^n. */
          if (limb > MPFR_LIMB_HIGHBIT)
            {
              /* The exponent cannot decrease: compute b-c/2 */
              /* Shift c in the allocated temporary block */
            SubD1NoLose:
              c0 = cp[0] & (MPFR_LIMB_ONE << sh);
              mask = ~MPFR_LIMB_MASK(sh);
              ap = MPFR_MANT(a);
              cp = MPFR_TMP_LIMBS_ALLOC (n);
              /* FIXME: it might be faster to have one function shifting c by 1
                 to the right and adding with b to a, which would read c once
                 only, and avoid a temporary allocation. */
              mpn_rshift (cp, MPFR_MANT(c), n, 1);
              cp[0] &= mask; /* Zero last bit of c if set */
              mpn_sub_n (ap, bp, cp, n);
              MPFR_SET_EXP(a, bx); /* No expo overflow! */
              MPFR_ASSERTD(ap[n-1] > ~ap[n-1]);
              if (MPFR_LIKELY(c0 == 0))
                {
                  /* Result is exact: no need of rounding! */
                  MPFR_TMP_FREE(marker);
                  return 0;
                }
              MPFR_ASSERTD( !(ap[0] & ~mask) );    /* Check last bits */
              /* No normalize is needed */
              /* Rounding is necessary since c0 = 1 */
              /* Cp =-1 and C'p+1=0 */
              bcp = 1; bcp1 = 0;
              if (MPFR_LIKELY(rnd_mode == MPFR_RNDN))
                {
                  /* Even Rule apply: Check last bit of a. */
                  if (MPFR_LIKELY( (ap[0] & (MPFR_LIMB_ONE << sh)) == 0) )
                    goto truncate;
                  else
                    goto sub_one_ulp;
                }
              MPFR_UPDATE_RND_MODE(rnd_mode, MPFR_IS_NEG(a));
              if (rnd_mode == MPFR_RNDZ)
                goto sub_one_ulp;
              else
                goto truncate;
            }
          else if (MPFR_LIKELY(limb < MPFR_LIMB_HIGHBIT))
            {
              /* |b| - |c| <= (W/2-1)*W^k + W^k-1 = 1/2*W^n - 1 */
              /* The exponent decreases by one. */
            SubD1Lose:
              ap = MPFR_MANT(a);
              /* Compute 2*b-c (Exact) */
              /* Experiments with __gmpn_rsblsh_n show that it is not always
                 faster than mpn_lshift + mpn_sub_n, thus we don't enable it
                 for now (HAVE___GMPN_RSBLSH_N -> HAVE___GMPN_RSBLSH_Nxxx). */
#if defined(WANT_GMP_INTERNALS) && defined(HAVE___GMPN_RSBLSH_Nxxx)
              /* {ap, n} = 2*{bp, n} - {cp, n} */
              __gmpn_rsblsh_n (ap, MPFR_MANT(c), MPFR_MANT(b), n, 1);
#else
              bp = MPFR_TMP_LIMBS_ALLOC (n);
              /* Shift b in the allocated temporary block */
              mpn_lshift (bp, MPFR_MANT(b), n, 1);
              mpn_sub_n (ap, bp, cp, n);
#endif
              bx--;
              goto ExactNormalize;
            }
          else
            {
              /* Case: limb = 100000000000 */
              /* Check while b[k] == c'[k] (C' is C shifted by 1) */
              /* If b[k]<c'[k] => We lose at least one bit*/
              /* If b[k]>c'[k] => We don't lose any bit */
              /* If k==-1 => We don't lose any bit
                 AND the result is 100000000000 0000000000 00000000000 */
              mp_limb_t carry;
              do
                {
                  carry = cp[k] << (GMP_NUMB_BITS - 1);
                  if (--k < 0)
                    break;
                  carry += cp[k] >> 1;
                }
              while (bp[k] == carry);
              if (MPFR_UNLIKELY(k < 0))
                {
                  ap = MPFR_MANT (a);
                  if (MPFR_UNLIKELY(carry))
                    {
                      /* If carry then necessarily the precision is an exact
                         multiple of GMP_NUMB_BITS, and we lose one bit,
                         thus the (exact) result is a power of 2 minus 1. */
                      memset (ap, -1, n * MPFR_BYTES_PER_MP_LIMB);
                      MPFR_SET_EXP (a, bx - 1);
                      /* No underflow is possible since cx = bx-1 is a valid
                         exponent. */
                    }
                  else
                    {
                      /* No carry: result is a power of 2. */
                      MPN_ZERO (ap, n - 1);
                      ap[n-1] = MPFR_LIMB_HIGHBIT;
                      MPFR_SET_EXP (a, bx); /* No expo overflow! */
                    }
                  /* No Normalize is needed */
                  /* No Rounding is needed */
                  MPFR_TMP_FREE (marker);
                  return 0;
                }
              /* carry = cp[k]/2+(cp[k-1]&1)<<(GMP_NUMB_BITS-1) = c'[k]*/
              else if (bp[k] > carry)
                goto SubD1NoLose; /* |b|-|c| >= 1/2*W^n */
              else
                {
                  MPFR_ASSERTD(bp[k] < carry);
                  goto SubD1Lose; /* |b|-|c| <= 1/2*W^n-1 and is exact */
                }
            }
        }
    }
  else if (MPFR_UNLIKELY(d >= p))
    {
      ap = MPFR_MANT(a);
      MPFR_UNSIGNED_MINUS_MODULO(sh, p);
      /* We can't set A before since we use cp for rounding... */
      /* Perform rounding: check if a=b or a=b-ulp(b) */
      if (MPFR_UNLIKELY(d == p))
        {
          /* cp == -1 and c'p+1 = ? */
          bcp  = 1;
          /* We need Cp+1 later for a very improbable case. */
          bbcp = (MPFR_MANT(c)[n-1] & (MPFR_LIMB_ONE<<(GMP_NUMB_BITS-2)));
          /* We need also C'p+1 for an even more unprobable case... */
          if (MPFR_LIKELY( bbcp ))
            bcp1 = 1;
          else
            {
              cp = MPFR_MANT(c);
              if (MPFR_UNLIKELY(cp[n-1] == MPFR_LIMB_HIGHBIT))
                {
                  mp_size_t k = n-1;
                  do
                    k--;
                  while (k >= 0 && cp[k] == 0);
                  bcp1 = (k >= 0);
                }
              else
                bcp1 = 1;
            }
          DEBUG( printf("(D=P) Cp=-1 Cp+1=%d C'p+1=%d \n", bbcp!=0, bcp1!=0) );
          bp = MPFR_MANT (b);

          /* Even if src and dest overlap, it is OK using MPN_COPY */
          if (MPFR_LIKELY(rnd_mode == MPFR_RNDN))
            {
              if (MPFR_UNLIKELY (bcp != 0 && bcp1 == 0))
                /* Cp=-1 and C'p+1=0: Even rule Apply! */
                /* Check Ap-1 = Bp-1 */
                if ((bp[0] & (MPFR_LIMB_ONE << sh)) == 0)
                  {
                    MPN_COPY(ap, bp, n);
                    goto truncate;
                  }
              MPN_COPY(ap, bp, n);
              goto sub_one_ulp;
            }
          MPFR_UPDATE_RND_MODE(rnd_mode, MPFR_IS_NEG(a));
          if (rnd_mode == MPFR_RNDZ)
            {
              MPN_COPY(ap, bp, n);
              goto sub_one_ulp;
            }
          else
            {
              MPN_COPY(ap, bp, n);
              goto truncate;
            }
        }
      else
        {
          /* Cp=0, Cp+1=-1 if d==p+1, C'p+1=-1 */
          bcp = 0; bbcp = (d==p+1); bcp1 = 1;
          DEBUG( printf("(D>P) Cp=%d Cp+1=%d C'p+1=%d\n", bcp!=0,bbcp!=0,bcp1!=0) );
          /* Need to compute C'p+2 if d==p+1 and if rnd_mode=NEAREST
             (Because of a very improbable case) */
          if (MPFR_UNLIKELY(d==p+1 && rnd_mode==MPFR_RNDN))
            {
              cp = MPFR_MANT(c);
              if (MPFR_UNLIKELY(cp[n-1] == MPFR_LIMB_HIGHBIT))
                {
                  mp_size_t k = n-1;
                  do
                    k--;
                  while (k >= 0 && cp[k] == 0);
                  bbcp1 = (k >= 0);
                }
              else
                bbcp1 = 1;
              DEBUG( printf("(D>P) C'p+2=%d\n", bbcp1!=0) );
            }
          /* Copy mantissa B in A */
          MPN_COPY(ap, MPFR_MANT(b), n);
          /* Round */
          if (MPFR_LIKELY(rnd_mode == MPFR_RNDN))
            goto truncate;
          MPFR_UPDATE_RND_MODE(rnd_mode, MPFR_IS_NEG(a));
          if (rnd_mode == MPFR_RNDZ)
            goto sub_one_ulp;
          else /* rnd_mode = AWAY */
            goto truncate;
        }
    }
  else /* case 2 <= d < p */
    {
      mpfr_uexp_t dm;
      mp_size_t m;
      mp_limb_t mask;

      MPFR_UNSIGNED_MINUS_MODULO(sh, p);
      cp = MPFR_TMP_LIMBS_ALLOC (n);

      /* Shift c in temporary allocated place */
      dm = d % GMP_NUMB_BITS;
      m = d / GMP_NUMB_BITS;
      if (MPFR_UNLIKELY(dm == 0))
        {
          /* dm = 0 and m > 0: Just copy */
          MPFR_ASSERTD(m != 0);
          MPN_COPY(cp, MPFR_MANT(c)+m, n-m);
          MPN_ZERO(cp+n-m, m);
        }
      else if (MPFR_LIKELY(m == 0))
        {
          /* dm >=2 and m == 0: just shift */
          MPFR_ASSERTD(dm >= 2);
          mpn_rshift(cp, MPFR_MANT(c), n, dm);
        }
      else
        {
          /* dm > 0 and m > 0: shift and zero  */
          mpn_rshift(cp, MPFR_MANT(c)+m, n-m, dm);
          MPN_ZERO(cp+n-m, m);
        }

      DEBUG( mpfr_print_mant_binary("Before", MPFR_MANT(c), p) );
      DEBUG( mpfr_print_mant_binary("B=    ", MPFR_MANT(b), p) );
      DEBUG( mpfr_print_mant_binary("After ", cp, p) );

      /* Compute bcp=Cp and bcp1=C'p+1 */
      if (MPFR_LIKELY(sh))
        {
          /* Try to compute them from C' rather than C (FIXME: Faster?) */
          bcp = (cp[0] & (MPFR_LIMB_ONE<<(sh-1))) ;
          if (cp[0] & MPFR_LIMB_MASK(sh-1))
            bcp1 = 1;
          else
            {
              /* We can't compute C'p+1 from C'. Compute it from C */
              /* Start from bit x=p-d+sh in mantissa C
                 (+sh since we have already looked sh bits in C'!) */
              mpfr_prec_t x = p-d+sh-1;
              if (x > p)
                /* We are already looked at all the bits of c, so C'p+1 = 0*/
                bcp1 = 0;
              else
                {
                  mp_limb_t *tp = MPFR_MANT(c);
                  mp_size_t kx = n-1 - (x / GMP_NUMB_BITS);
                  mpfr_prec_t sx = GMP_NUMB_BITS-1-(x%GMP_NUMB_BITS);
                  DEBUG (printf ("(First) x=%lu Kx=%ld Sx=%lu\n",
                                 (unsigned long) x, (long) kx,
                                 (unsigned long) sx));
                  /* Looks at the last bits of limb kx (if sx=0 does nothing)*/
                  if (tp[kx] & MPFR_LIMB_MASK(sx))
                    bcp1 = 1;
                  else
                    {
                      /*kx += (sx==0);*/
                      /*If sx==0, tp[kx] hasn't been checked*/
                      do
                        kx--;
                      while (kx >= 0 && tp[kx] == 0);
                      bcp1 = (kx >= 0);
                    }
                }
            }
        }
      else
        {
          /* Compute Cp and C'p+1 from C with sh=0 */
          mp_limb_t *tp = MPFR_MANT(c);
          /* Start from bit x=p-d in mantissa C */
          mpfr_prec_t  x = p-d;
          mp_size_t   kx = n-1 - (x / GMP_NUMB_BITS);
          mpfr_prec_t sx = GMP_NUMB_BITS-1-(x%GMP_NUMB_BITS);
          MPFR_ASSERTD(p >= d);
          bcp = (tp[kx] & (MPFR_LIMB_ONE<<sx));
          /* Looks at the last bits of limb kx (If sx=0, does nothing)*/
          if (tp[kx] & MPFR_LIMB_MASK(sx))
            bcp1 = 1;
          else
            {
              /*kx += (sx==0);*/ /*If sx==0, tp[kx] hasn't been checked*/
              do
                kx--;
              while (kx >= 0 && tp[kx] == 0);
              bcp1 = (kx >= 0);
            }
        }
      DEBUG( printf("sh=%lu Cp=%d C'p+1=%d\n", sh, bcp!=0, bcp1!=0) );

      /* Check if we can lose a bit, and if so compute Cp+1 and C'p+2 */
      bp = MPFR_MANT(b);
      if (MPFR_UNLIKELY (bp[n-1] - cp[n-1] <= MPFR_LIMB_HIGHBIT))
        {
          /* We can lose a bit so we precompute Cp+1 and C'p+2 */
          /* Test for trivial case: since C'p+1=0, Cp+1=0 and C'p+2 =0 */
          if (MPFR_LIKELY(bcp1 == 0))
            {
              bbcp = 0;
              bbcp1 = 0;
            }
          else /* bcp1 != 0 */
            {
              /* We can lose a bit:
                 compute Cp+1 and C'p+2 from mantissa C */
              mp_limb_t *tp = MPFR_MANT(c);
              /* Start from bit x=(p+1)-d in mantissa C */
              mpfr_prec_t x  = p+1-d;
              mp_size_t kx = n-1 - (x / GMP_NUMB_BITS);
              mpfr_prec_t sx = GMP_NUMB_BITS-1 - (x % GMP_NUMB_BITS);

              MPFR_ASSERTD(p > d);
              DEBUG (printf ("(pre) x=%lu Kx=%ld Sx=%lu\n",
                             (unsigned long) x, (long) kx,
                             (unsigned long) sx));
              bbcp = (tp[kx] & (MPFR_LIMB_ONE<<sx)) ;
              /* Looks at the last bits of limb kx (If sx=0, does nothing)*/
              /* If Cp+1=0, since C'p+1!=0, C'p+2=1 ! */
              if (MPFR_LIKELY (bbcp == 0 || (tp[kx] & MPFR_LIMB_MASK(sx))))
                bbcp1 = 1;
              else
                {
                  /*kx += (sx==0);*/ /*If sx==0, tp[kx] hasn't been checked*/
                  do
                    kx--;
                  while (kx >= 0 && tp[kx] == 0);
                  bbcp1 = (kx >= 0);
                  DEBUG (printf ("(Pre) Scan done for %ld\n", (long) kx));
                }
            } /*End of Bcp1 != 0*/
          DEBUG( printf("(Pre) Cp+1=%d C'p+2=%d\n", bbcp!=0, bbcp1!=0) );
        } /* End of "can lose a bit" */

      /* Clean shifted C' */
      mask = ~MPFR_LIMB_MASK (sh);
      cp[0] &= mask;

      /* Subtract the mantissa c from b in a */
      ap = MPFR_MANT(a);
      mpn_sub_n (ap, bp, cp, n);
      DEBUG( mpfr_print_mant_binary("Sub=  ", ap, p) );

     /* Normalize: we lose at max one bit*/
      if (MPFR_UNLIKELY(MPFR_LIMB_MSB(ap[n-1]) == 0))
        {
          /* High bit is not set and we have to fix it! */
          /* Ap >= 010000xxx001 */
          mpn_lshift(ap, ap, n, 1);
          /* Ap >= 100000xxx010 */
          if (MPFR_UNLIKELY(bcp != 0)) /* Check if Cp = -1 */
            /* Since Cp == -1, we have to subtract one more */
            {
              mpn_sub_1(ap, ap, n, MPFR_LIMB_ONE<<sh);
              MPFR_ASSERTD(MPFR_LIMB_MSB(ap[n-1]) != 0);
            }
          /* Ap >= 10000xxx001 */
          /* Final exponent -1 since we have shifted the mantissa */
          bx--;
          /* Update bcp and bcp1 */
          MPFR_ASSERTD(bbcp != MPFR_LIMB_MAX);
          MPFR_ASSERTD(bbcp1 != MPFR_LIMB_MAX);
          bcp  = bbcp;
          bcp1 = bbcp1;
          /* We don't have anymore a valid Cp+1!
             But since Ap >= 100000xxx001, the final sub can't unnormalize!*/
        }
      MPFR_ASSERTD( !(ap[0] & ~mask) );

      /* Rounding */
      if (MPFR_LIKELY(rnd_mode == MPFR_RNDN))
        {
          if (MPFR_LIKELY(bcp == 0))
            goto truncate;
          else if (bcp1 != 0 || (ap[0] & (MPFR_LIMB_ONE << sh)) != 0)
            goto sub_one_ulp;
          else
            goto truncate;
        }

      /* Update rounding mode */
      MPFR_UPDATE_RND_MODE(rnd_mode, MPFR_IS_NEG(a));
      if (rnd_mode == MPFR_RNDZ && MPFR_LIKELY (bcp != 0 || bcp1 != 0))
        goto sub_one_ulp;
      goto truncate;
    }
  MPFR_RET_NEVER_GO_HERE ();

  /* Sub one ulp to the result */
 sub_one_ulp:
  mpn_sub_1 (ap, ap, n, MPFR_LIMB_ONE << sh);
  /* Result should be smaller than exact value: inexact=-1 */
  inexact = -1;
  /* Check normalization */
  if (MPFR_UNLIKELY(ap[n-1] < MPFR_LIMB_HIGHBIT))
    {
      /* ap was a power of 2, and we lose a bit */
      /* Now it is 0111111111111111111[00000 */
      /* The following 2 lines are equivalent to: mpn_lshift(ap, ap, n, 1); */
      ap[0] <<= 1;
      ap[n-1] |= MPFR_LIMB_HIGHBIT;
      bx--;
      /* And the lost bit x depends on Cp+1, and Cp */
      /* Compute Cp+1 if it isn't already computed (ie d==1) */
      /* Note: we can't have d = 1 here, since the only "goto sub_one_ulp"
         for d = 1 are in the "SubD1NoLose" case, and in that case
         |b|-|c| >= 1/2*W^n, thus round(|b|-|c|) >= 1/2*W^n, and ap[n-1]
         cannot go below MPFR_LIMB_HIGHBIT. */
      DEBUG( printf("(SubOneUlp)Cp=%d, Cp+1=%d C'p+1=%d\n", bcp!=0,bbcp!=0,bcp1!=0));
      /* Compute the last bit (Since we have shifted the mantissa)
         we need one more bit! */
      MPFR_ASSERTD(bbcp != MPFR_LIMB_MAX);
      if ((rnd_mode == MPFR_RNDZ && bcp == 0) ||
          (rnd_mode == MPFR_RNDN && bbcp == 0) ||
          (bcp != 0 && bcp1 == 0)) /* Exact result */
        {
          ap[0] |= MPFR_LIMB_ONE << sh;
          if (rnd_mode == MPFR_RNDN)
            inexact = 1;
          DEBUG( printf("(SubOneUlp) Last bit set\n") );
        }
      /* Result could be exact if C'p+1 = 0 and rnd == Zero
         since we have had one more bit to the result */
      /* Fixme: rnd_mode == MPFR_RNDZ needed ? */
      if (rnd_mode == MPFR_RNDZ && bcp1 == 0)
        {
          DEBUG( printf("(SubOneUlp) Exact result\n") );
          inexact = 0;
        }
    }

  goto end_of_sub;

 truncate:
  /* Check if the result is an exact power of 2: 100000000000
     in which cases, we could have to do sub_one_ulp due to some nasty reasons:
     If Result is a Power of 2:
      + If rnd = AWAY,
      |  If Cp=-1 and C'p+1 = 0, SubOneUlp and the result is EXACT.
         If Cp=-1 and C'p+1 =-1, SubOneUlp and the result is above.
         Otherwise truncate
      + If rnd = NEAREST,
         If Cp= 0 and Cp+1  =-1 and C'p+2=-1, SubOneUlp and the result is above
         If cp=-1 and C'p+1 = 0, SubOneUlp and the result is exact.
         Otherwise truncate.
      X bit should always be set if SubOneUlp*/
  if (MPFR_UNLIKELY(ap[n-1] == MPFR_LIMB_HIGHBIT))
    {
      mp_size_t k = n-1;
      do
        k--;
      while (k >= 0 && ap[k] == 0);
      if (MPFR_UNLIKELY (k < 0))
        {
          /* It is a power of 2! */
          /* Compute Cp+1 if it isn't already compute (ie d==1) */
          /* Note: if d=1, we have {a, n} > 1/2*W^n, thus we cannot have k < 0. */
          DEBUG( printf("(Truncate) Cp=%d, Cp+1=%d C'p+1=%d C'p+2=%d\n", \
                 bcp!=0, bbcp!=0, bcp1!=0, bbcp1!=0) );
          MPFR_ASSERTD(bbcp != MPFR_LIMB_MAX);
          MPFR_ASSERTD(rnd_mode != MPFR_RNDN || bcp != 0 ||
                       bbcp == 0 || bbcp1 != MPFR_LIMB_MAX);
          if ((rnd_mode != MPFR_RNDZ && bcp != 0) ||
              (rnd_mode == MPFR_RNDN && bcp == 0 && bbcp != 0 && bbcp1 != 0))
            {
              DEBUG( printf("(Truncate) Do sub\n") );
              mpn_sub_1 (ap, ap, n, MPFR_LIMB_ONE << sh);
              ap[n-1] |= MPFR_LIMB_HIGHBIT;
              bx--;
              /* FIXME: Explain why it works (or why not)... */
              inexact = (bcp1 == 0) ? 0 : (rnd_mode == MPFR_RNDN) ? -1 : 1;
              goto end_of_sub;
            }
        }
    }

  /* Calcul of Inexact flag.*/
  inexact = (bcp != 0 || bcp1 != 0);

 end_of_sub:
  /* Update Exponent */
  /* bx >= emin. Proof:
      If d==0      : Exact case. This is never called.
      if 1 < d < p : bx=MPFR_EXP(b) or MPFR_EXP(b)-1 > MPFR_EXP(c) > emin
      if d == 1    : bx=MPFR_EXP(b). If we could lose any bits, the exact
                     normalization is called.
      if d >=  p   : bx=MPFR_EXP(b) >= MPFR_EXP(c) + p > emin
     After SubOneUlp, we could have one bit less.
      if 1 < d < p : bx >= MPFR_EXP(b)-2 >= MPFR_EXP(c) > emin
      if d == 1    : bx >= MPFR_EXP(b)-1 = MPFR_EXP(c) > emin.
      if d >=  p   : bx >= MPFR_EXP(b)-1 > emin since p>=2.
  */
  MPFR_ASSERTD( bx >= __gmpfr_emin);
  MPFR_SET_EXP (a, bx);

  MPFR_TMP_FREE(marker);
  MPFR_RET (inexact * MPFR_INT_SIGN (a));
}
