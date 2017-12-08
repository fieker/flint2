/*
    Copyright (C) 2017 Daniel Schultz

    This file is part of FLINT.

    FLINT is free software: you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License (LGPL) as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.  See <http://www.gnu.org/licenses/>.
*/

#include "nmod_mpoly.h"

slong _nmod_mpoly_divrem_monagan_pearce1(slong * lenr,
        mp_limb_t ** polyq, ulong ** expq, slong * allocq,
        mp_limb_t ** polyr, ulong ** expr, slong * allocr,
        const mp_limb_t * coeff2, const ulong * exp2, slong len2,
        const mp_limb_t * coeff3, const ulong * exp3, slong len3,
                              slong bits, ulong maskhi, const nmodf_ctx_t fctx)
{
    slong i, j, k, l, s;
    slong next_loc, heap_len = 2;
    mpoly_heap1_s * heap;
    mpoly_heap_t * chain;
    slong * store, * store_base;
    mpoly_heap_t * x;
    mp_limb_t * p1 = *polyq;
    mp_limb_t * p2 = *polyr;
    ulong * e1 = *expq;
    ulong * e2 = *expr;
    slong * hind;
    ulong mask, exp;
    int lt_divides;
    mp_limb_t lc_minus_inv, acc0, acc1, acc2, pp1, pp0;
    TMP_INIT;

    TMP_START;

    /* alloc array of heap nodes which can be chained together */
    next_loc = len3 + 4;   /* something bigger than heap can ever be */
    heap = (mpoly_heap1_s *) TMP_ALLOC((len3 + 1)*sizeof(mpoly_heap1_s));
    chain = (mpoly_heap_t *) TMP_ALLOC(len3*sizeof(mpoly_heap_t));
    store = store_base = (slong *) TMP_ALLOC(2*len3*sizeof(mpoly_heap_t *));

    /* space for flagged heap indicies */
    hind = (slong *) TMP_ALLOC(len3*sizeof(slong));
    for (i = 0; i < len3; i++)
        hind[i] = 1;

    /* mask with high bit set in each field of exponent vector */
    mask = 0;
    for (i = 0; i < FLINT_BITS/bits; i++)
        mask = (mask << bits) + (UWORD(1) << (bits - 1));

    /* quotient and remainder poly indices start at -1 */
    k = -WORD(1);
    l = -WORD(1);

    /* s is the number of terms * (latest quotient) we should put into heap */
    s = len3;

    /* insert (-1, 0, exp2[0]) into heap */
    x = chain + 0;
    x->i = -WORD(1);
    x->j = 0;
    x->next = NULL;
    HEAP_ASSIGN(heap[1], exp2[0], x);

    /* precompute leading cofficient info */
    lc_minus_inv = fctx->mod.n - nmod_inv(coeff3[0], fctx->mod);

    while (heap_len > 1)
    {
        exp = heap[1].exp;

        if (mpoly_monomial_overflows1(exp, mask))
            goto exp_overflow;

        k++;
        _nmod_mpoly_fit_length(&p1, &e1, allocq, k + 1, 1);

        lt_divides = mpoly_monomial_divides1(e1 + k, exp, exp3[0], mask);

        acc0 = acc1 = acc2 = 0;
        do
        {
            x = _mpoly_heap_pop1(heap, &heap_len, maskhi);
            do
            {
                *store++ = x->i;
                *store++ = x->j;
                if (x->i != -WORD(1))
                    hind[x->i] |= WORD(1);

                if (x->i == -WORD(1))
                {
                    add_sssaaaaaa(acc2, acc1, acc0, acc2, acc1, acc0, WORD(0), WORD(0), fctx->mod.n - coeff2[x->j]);
                } else
                {
                    umul_ppmm(pp1, pp0, coeff3[x->i], p1[x->j]);
                    add_sssaaaaaa(acc2, acc1, acc0, acc2, acc1, acc0, WORD(0), pp1, pp0);
                }

            } while ((x = x->next) != NULL);
        } while (heap_len > 1 && heap[1].exp == exp);

        NMOD_RED3(acc0, acc2, acc1, acc0, fctx->mod);

        /* process nodes taken from the heap */
        while (store > store_base)
        {
            j = *--store;
            i = *--store;

            if (i == -WORD(1))
            {
                /* take next dividend term */
                if (j + 1 < len2)
                {
                    x = chain + 0;
                    x->i = i;
                    x->j = j + 1;
                    x->next = NULL;
                    _mpoly_heap_insert1(heap, exp2[x->j], x,
                                                 &next_loc, &heap_len, maskhi);
                }
            } else
            {
                /* should we go right? */
                if (  (i + 1 < len3)
                   && (hind[i + 1] == 2*j + 1)
                   )
                {
                    x = chain + i + 1;
                    x->i = i + 1;
                    x->j = j;
                    x->next = NULL;
                    hind[x->i] = 2*(x->j + 1) + 0;
                    _mpoly_heap_insert1(heap, exp3[x->i] + e1[x->j], x,
                                                 &next_loc, &heap_len, maskhi);
                }
                /* should we go up? */
                if (j + 1 == k)
                {
                    s++;
                } else if (  ((hind[i] & 1) == 1)
                          && ((i == 1) || (hind[i - 1] >= 2*(j + 2) + 1))
                          )
                {
                    x = chain + i;
                    x->i = i;
                    x->j = j + 1;
                    x->next = NULL;
                    hind[x->i] = 2*(x->j + 1) + 0;
                    _mpoly_heap_insert1(heap, exp3[x->i] + e1[x->j], x,
                                                 &next_loc, &heap_len, maskhi);
                }
            }
        }

        /* try to divide accumulated term by leading term */
        if (acc0 == 0)
        {
            k--;
            continue;
        }
        if (!lt_divides)
        {
            l++;
            _nmod_mpoly_fit_length(&p2, &e2, allocr, l + 1, 1);
            p2[l] = fctx->mod.n - acc0;
            e2[l] = exp;
            k--;
            continue;
        }

        p1[k] = nmod_mul(acc0, lc_minus_inv, fctx->mod);

        /* put newly generated quotient term back into the heap if neccesary */
        if (s > 1)
        {
            i = 1;
            x = chain + i;
            x->i = i;
            x->j = k;
            x->next = NULL;
            hind[x->i] = 2*(x->j + 1) + 0;
            _mpoly_heap_insert1(heap, exp3[x->i] + e1[x->j], x,
                                                 &next_loc, &heap_len, maskhi);
        }
        s = 1;
    }

    k++;
    l++;

cleanup:

   (*polyq) = p1;
   (*expq) = e1;
   (*polyr) = p2;
   (*expr) = e2;
   
   /* set remainder poly length */
   (*lenr) = l;

    TMP_END;

    return k;

exp_overflow:
    k = 0;
    l = 0;
    goto cleanup;
}




slong _nmod_mpoly_divrem_monagan_pearce(slong * lenr,
                  mp_limb_t ** polyq,      ulong ** expq, slong * allocq,
                  mp_limb_t ** polyr,      ulong ** expr, slong * allocr,
            const mp_limb_t * coeff2, const ulong * exp2, slong len2,
            const mp_limb_t * coeff3, const ulong * exp3, slong len3,
       slong bits, slong N, const ulong * cmpmask, const nmodf_ctx_t fctx)
{
    slong i, j, k, l, s;
    slong next_loc;
    slong heap_len = 2; /* heap zero index unused */
    mpoly_heap_s * heap;
    mpoly_heap_t * chain;
    slong * store, * store_base;
    mpoly_heap_t * x;
    mp_limb_t * p1 = *polyq;
    mp_limb_t * p2 = *polyr;
    ulong * e1 = *expq;
    ulong * e2 = *expr;
    ulong * exp, * exps;
    ulong ** exp_list;
    slong exp_next;
    ulong mask;
    slong * hind;
    int lt_divides;
    mp_limb_t lc_minus_inv, acc0, acc1, acc2, pp1, pp0;
    TMP_INIT;


    if (N == 1)
        return _nmod_mpoly_divrem_monagan_pearce1(lenr, polyq, expq, allocq,
                                     polyr, expr, allocr, coeff2, exp2, len2,
                                   coeff3, exp3, len3, bits, cmpmask[0], fctx);


    TMP_START;


    /* alloc array of heap nodes which can be chained together */
    next_loc = len3 + 4;   /* something bigger than heap can ever be */
    heap = (mpoly_heap_s *) TMP_ALLOC((len3 + 1)*sizeof(mpoly_heap_s));
    chain = (mpoly_heap_t *) TMP_ALLOC(len3*sizeof(mpoly_heap_t));
    store = store_base = (slong *) TMP_ALLOC(2*len3*sizeof(mpoly_heap_t *));

    /* array of exponent vectors, each of "N" words */
    exps = (ulong *) TMP_ALLOC(len3*N*sizeof(ulong));
    /* list of pointers to available exponent vectors */
    exp_list = (ulong **) TMP_ALLOC(len3*sizeof(ulong *));
    /* space to save copy of current exponent vector */
    exp = (ulong *) TMP_ALLOC(N*sizeof(ulong));
    /* set up list of available exponent vectors */
    exp_next = 0;
    for (i = 0; i < len3; i++)
        exp_list[i] = exps + i*N;

    /* space for flagged heap indicies */
    hind = (slong *) TMP_ALLOC(len3*sizeof(slong));
    for (i = 0; i < len3; i++)
        hind[i] = 1;

    /* mask with high bit set in each word of each field of exponent vector */
    mask = 0;
    for (i = 0; i < FLINT_BITS/bits; i++)
        mask = (mask << bits) + (UWORD(1) << (bits - 1));

    /* quotient and remainder poly indices start at -1 */
    k = -WORD(1);
    l = -WORD(1);
   
    /* s is the number of terms * (latest quotient) we should put into heap */
    s = len3;
   
    /* insert (-1, 0, exp2[0]) into heap */
    x = chain + 0;
    x->i = -WORD(1);
    x->j = 0;
    x->next = NULL;
    heap[1].next = x;
    heap[1].exp = exp_list[exp_next++];
    mpoly_monomial_set(heap[1].exp, exp2, N);

    /* precompute leading cofficient info */
    lc_minus_inv = fctx->mod.n - nmod_inv(coeff3[0], fctx->mod);
   
    while (heap_len > 1)
    {
        mpoly_monomial_set(exp, heap[1].exp, N);

        if (mpoly_monomial_overflows(exp, N, mask))
            goto exp_overflow2;
      
        k++;
        _nmod_mpoly_fit_length(&p1, &e1, allocq, k + 1, N);

        lt_divides = mpoly_monomial_divides(e1 + k*N, exp, exp3, N, mask);

        acc0 = acc1 = acc2 = 0;
        do
        {
            exp_list[--exp_next] = heap[1].exp;
            x = _mpoly_heap_pop(heap, &heap_len, N, cmpmask);
            do
            {
                *store++ = x->i;
                *store++ = x->j;
                if (x->i != -WORD(1))
                    hind[x->i] |= WORD(1);

                if (x->i == -WORD(1))
                {
                    add_sssaaaaaa(acc2, acc1, acc0, acc2, acc1, acc0, WORD(0), WORD(0), fctx->mod.n - coeff2[x->j]);
                } else
                {
                    umul_ppmm(pp1, pp0, coeff3[x->i], p1[x->j]);
                    add_sssaaaaaa(acc2, acc1, acc0, acc2, acc1, acc0, WORD(0), pp1, pp0);
                }
            } while ((x = x->next) != NULL);
        } while (heap_len > 1 && mpoly_monomial_equal(heap[1].exp, exp, N));

        NMOD_RED3(acc0, acc2, acc1, acc0, fctx->mod);

        /* process nodes taken from the heap */
        while (store > store_base)
        {
            j = *--store;
            i = *--store;

            if (i == -WORD(1))
            {
                /* take next dividend term */
                if (j + 1 < len2)
                {
                    x = chain + 0;
                    x->i = i;
                    x->j = j + 1;
                    x->next = NULL;
                    mpoly_monomial_set(exp_list[exp_next], exp2 + x->j*N, N);
                    if (!_mpoly_heap_insert(heap, exp_list[exp_next++], x,
                                      &next_loc, &heap_len, N, cmpmask))
                        exp_next--;
                }
            } else
            {
                /* should we go right? */
                if (  (i + 1 < len3)
                   && (hind[i + 1] == 2*j + 1)
                   )
                {
                    x = chain + i + 1;
                    x->i = i + 1;
                    x->j = j;
                    x->next = NULL;
                    hind[x->i] = 2*(x->j + 1) + 0;
                    mpoly_monomial_add(exp_list[exp_next], exp3 + x->i*N,
                                                           e1   + x->j*N, N);
                    if (!_mpoly_heap_insert(heap, exp_list[exp_next++], x,
                                      &next_loc, &heap_len, N, cmpmask))
                        exp_next--;
                }
                /* should we go up? */
                if (j + 1 == k)
                {
                    s++;
                } else if (  ((hind[i] & 1) == 1)
                          && ((i == 1) || (hind[i - 1] >= 2*(j + 2) + 1))
                          )
                {
                    x = chain + i;
                    x->i = i;
                    x->j = j + 1;
                    x->next = NULL;
                    hind[x->i] = 2*(x->j + 1) + 0;
                    mpoly_monomial_add(exp_list[exp_next], exp3 + x->i*N,
                                                           e1   + x->j*N, N);
                    if (!_mpoly_heap_insert(heap, exp_list[exp_next++], x,
                                      &next_loc, &heap_len, N, cmpmask))
                        exp_next--;
                }
            }
        }

        /* try to divide accumulated term by leading term */
        if (acc0 == 0)
        {
            k--;
            continue;
        }
        if (!lt_divides)
        {
            l++;
            _nmod_mpoly_fit_length(&p2, &e2, allocr, l + 1, N);
            p2[l] = fctx->mod.n - acc0;
            mpoly_monomial_set(e2 + l*N, exp, N);
            k--;
            continue;
        }

        p1[k] = nmod_mul(acc0, lc_minus_inv, fctx->mod);

        /* put newly generated quotient term back into the heap if neccesary */
        if (s > 1)
        {
            i = 1;
            x = chain + i;
            x->i = i;
            x->j = k;
            x->next = NULL;
            hind[x->i] = 2*(x->j + 1) + 0;
            mpoly_monomial_add(exp_list[exp_next], exp3 + x->i*N,
                                                   e1   + x->j*N, N);
            if (!_mpoly_heap_insert(heap, exp_list[exp_next++], x,
                                  &next_loc, &heap_len, N, cmpmask))
                exp_next--;
        }
        s = 1;
    }

    k++;
    l++;

cleanup2:

    (*polyq) = p1;
    (*expq) = e1;
    (*polyr) = p2;
    (*expr) = e2;

    /* set remainder poly length */
    (*lenr) = l;

    TMP_END;

    /* return quotient poly length */
    return k;

exp_overflow2:
    k = 0;
    l = 0;
    goto cleanup2;
}

void nmod_mpoly_divrem_monagan_pearce(nmod_mpoly_t q, nmod_mpoly_t r,
                      const nmod_mpoly_t poly2, const nmod_mpoly_t poly3,
                                                    const nmod_mpoly_ctx_t ctx)
{
    slong exp_bits, N, lenq = 0, lenr = 0;
    ulong * exp2 = poly2->exps, * exp3 = poly3->exps;
    ulong * cmpmask;
    int free2 = 0, free3 = 0;
    nmod_mpoly_t temp1, temp2;
    nmod_mpoly_struct * tq, * tr;
    TMP_INIT;

    if (poly3->length == 0)
        flint_throw(FLINT_DIVZERO, "Divide by zero in nmod_mpoly_divrem_monagan_pearce");

    if (poly2->length == 0)
    {
        nmod_mpoly_zero(q, ctx);
        nmod_mpoly_zero(r, ctx);
        return;
    }

    TMP_START;

    exp_bits = FLINT_MAX(poly2->bits, poly3->bits);

    N = mpoly_words_per_exp(exp_bits, ctx->minfo);
    cmpmask = (ulong*) TMP_ALLOC(N*sizeof(ulong));
    mpoly_get_cmpmask(cmpmask, N, exp_bits, ctx->minfo);

    /* ensure input exponents packed to same size as output exponents */
    if (exp_bits > poly2->bits)
    {
        free2 = 1;
        exp2 = (ulong *) flint_malloc(N*poly2->length*sizeof(ulong));
        mpoly_repack_monomials(exp2, exp_bits, poly2->exps, poly2->bits,
                                                    poly2->length, ctx->minfo);
    }

    if (exp_bits > poly3->bits)
    {
        free3 = 1;
        exp3 = (ulong *) flint_malloc(N*poly3->length*sizeof(ulong));
        mpoly_repack_monomials(exp3, exp_bits, poly3->exps, poly3->bits,
                                                    poly3->length, ctx->minfo);
    }

    /* check divisor leading monomial is at most that of the dividend */
    if (mpoly_monomial_lt(exp3, exp2, N, cmpmask))
    {
        nmod_mpoly_set(r, poly2, ctx);
        nmod_mpoly_zero(q, ctx);
        goto cleanup3;
    }

    /* take care of aliasing */
    if (q == poly2 || q == poly3)
    {
        nmod_mpoly_init2(temp1, poly2->length/poly3->length + 1,                                                                          ctx);
        nmod_mpoly_fit_bits(temp1, exp_bits, ctx);
        temp1->bits = exp_bits;
        tq = temp1;
    } else
    {
        nmod_mpoly_fit_length(q, poly2->length/poly3->length + 1, ctx);
        nmod_mpoly_fit_bits(q, exp_bits, ctx);
        q->bits = exp_bits;
        tq = q;
    }

    if (r == poly2 || r == poly3)
    {
        nmod_mpoly_init2(temp2, poly3->length, ctx);
        nmod_mpoly_fit_bits(temp2, exp_bits, ctx);
        temp2->bits = exp_bits;
        tr = temp2;
    } else
    {
        nmod_mpoly_fit_length(r, poly3->length, ctx);
        nmod_mpoly_fit_bits(r, exp_bits, ctx);
        r->bits = exp_bits;
        tr = r;
    }

    /* do division with remainder */
    while ((lenq = _nmod_mpoly_divrem_monagan_pearce(&lenr, &tq->coeffs, &tq->exps,
                      &tq->alloc, &tr->coeffs, &tr->exps, &tr->alloc, poly2->coeffs, exp2, 
                       poly2->length, poly3->coeffs, exp3, poly3->length, exp_bits,
                                         N, cmpmask, ctx->ffinfo)) == 0
          && lenr == 0 && exp_bits < FLINT_BITS)
   {
        ulong * old_exp2 = exp2, * old_exp3 = exp3;
        slong old_exp_bits = exp_bits;

        exp_bits = mpoly_fix_bits(exp_bits + 1, ctx->minfo);

        N = mpoly_words_per_exp(exp_bits, ctx->minfo);
        cmpmask = (ulong*) TMP_ALLOC(N*sizeof(ulong));
        mpoly_get_cmpmask(cmpmask, N, exp_bits, ctx->minfo);


        exp2 = (ulong *) flint_malloc(N*poly2->length*sizeof(ulong));
        mpoly_repack_monomials(exp2, exp_bits, old_exp2, old_exp_bits,
                                                    poly2->length, ctx->minfo);

        exp3 = (ulong *) flint_malloc(N*poly3->length*sizeof(ulong));
        mpoly_repack_monomials(exp3, exp_bits, old_exp3, old_exp_bits,
                                                    poly3->length, ctx->minfo);

        if (free2)
            flint_free(old_exp2);

        if (free3)
            flint_free(old_exp3);

        free2 = free3 = 1; 

        nmod_mpoly_fit_bits(tq, exp_bits, ctx);
        tq->bits = exp_bits;

        nmod_mpoly_fit_bits(tr, exp_bits, ctx);
        tr->bits = exp_bits;
    }

    if (lenq == 0 && lenr == 0)
        flint_throw(FLINT_EXPOF,
                      "Exponent overflow in nmod_mpoly_divrem_monagan_pearce");

    /* deal with aliasing */
    if (q == poly2 || q == poly3)
    {
        nmod_mpoly_swap(temp1, q, ctx);
        nmod_mpoly_clear(temp1, ctx);
    }

    if (r == poly2 || r == poly3)
    {
        nmod_mpoly_swap(temp2, r, ctx);
        nmod_mpoly_clear(temp2, ctx);
    }

    _nmod_mpoly_set_length(q, lenq, ctx);
    _nmod_mpoly_set_length(r, lenr, ctx);

cleanup3:

    if (free2)
        flint_free(exp2);

    if (free3)
        flint_free(exp3);

    TMP_END;
}
