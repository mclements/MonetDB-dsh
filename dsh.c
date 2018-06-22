/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

/* monetdb_config.h must be the first include in each .c file */
#include "monetdb_config.h"
#include "mal.h"
#include "mal_exception.h"

#include "dsh.h"
#include "dsh_impl.h"

#ifdef WIN32
#define dsh_export extern __declspec(dllexport)
#else
#define dsh_export extern
#endif

// FIXME seperate wrappers from implementation
// FIXME support for hseqbase? or already supported?
// FIXME BBPunfix in case of MAL_MALLOC_FAIL

// FIXME is there something better?
#define APPEND(b, TYPE, o) (((TYPE *) b->theap.base)[b->batCount++] = (o))

#define APPENDLEFT() do { \
        APPEND(lc, oid, spos); \
        APPEND(s, bit, TRUE); \
        spos++; \
    } while(0) 

#define APPENDRIGHT() do { \
        APPEND(rc, oid, spos); \
        APPEND(s, bit, FALSE); \
        spos++; \
    } while(0)

#define SET_CAND_PROP(b) do { \
        b->tkey = 1; \
        b->tsorted = 1; \
        b->trevsorted = BATcount(b) <= 1; \
        b->tnil = 0; \
        b->tnonil = BATcount(b) - 1; \
    } while(0)

/* segment-wise append, under the assumption of sortedness */
dsh_export char *
DSHoid_segment_append(bat *sideid, bat *lcandid, bat *rcandid, const bat * lid, const bat * rid)
{
    BAT *s;
    BAT *lc;
    BAT *rc;

    BAT *l;
    BAT *r;

    BUN lcnt;
    BUN rcnt;
    BUN cnt;

    oid * lvals;
    oid * rvals;
    oid * lend;
    oid * rend;
    oid lval;
    oid rval;

    oid spos;

    /* variables needed for void */
    oid lvalend;
    oid rvalend;

    l = BATdescriptor(*lid);
    if (l == NULL) {
        throw(MAL, "dsh.oid_segment_append", RUNTIME_OBJECT_MISSING);
    }
    r = BATdescriptor(*rid);
    if (r == NULL) {
        BBPunfix(l->batCacheid);
        throw(MAL, "dsh.oid_segment_append", RUNTIME_OBJECT_MISSING);
    }

    /* check for sortedness TODO property is not updated */
    //if (!l->tsorted || !r->tsorted) {
    //    BBPunfix(l->batCacheid);
    //    BBPunfix(r->batCacheid);
    //    throw(MAL, "dsh.oid_segment_append", ILLEGAL_ARGUMENT "inputs have to be sorted\n");
    //}

    /* check for correct types TODO void implementation*/
    if (!(l->ttype == TYPE_oid || l->ttype == TYPE_void)
        || !(r->ttype == TYPE_oid || r->ttype == TYPE_void)) {
        BBPunfix(l->batCacheid);
        BBPunfix(r->batCacheid);
        throw(MAL, "dsh.oid_segment_append", ILLEGAL_ARGUMENT "inputs have to be oid BATs\n");
    }

    lcnt = BATcount(l);
    rcnt = BATcount(r);
    cnt = lcnt + rcnt;

    /* sort out trivial cases for empty inputs */
    if (lcnt == 0 && rcnt == 0) {
        lc = BATdense(0, 0, 0);
        rc = BATdense(0, 0, 0);
        s = COLnew(0, TYPE_bit, 0, TRANSIENT);
        goto clean_up_success;
    }
    else if (lcnt == 0) {
        bit t = FALSE;
        lc = BATdense(0, 0, 0);
        rc = BATdense(0, 0, rcnt);
        s = BATconstant(0, TYPE_bit, &t, rcnt, TRANSIENT);
        goto clean_up_success;
    }
    else if (rcnt == 0) {
        bit t = TRUE;
        lc = BATdense(0, 0, lcnt);
        rc = BATdense(0, 0, 0);
        s = BATconstant(0, TYPE_bit, &t, lcnt, TRANSIENT);
        goto clean_up_success;
    }

    s = COLnew(0, TYPE_bit, cnt, TRANSIENT);
    lc = COLnew(0, TYPE_oid, cnt, TRANSIENT);
    rc = COLnew(0, TYPE_oid, cnt, TRANSIENT);

    if (s == NULL || lc == NULL || rc == NULL) {
        BBPunfix(l->batCacheid);
        BBPunfix(r->batCacheid);
        BBPreclaim(s);
        BBPreclaim(lc);
        BBPreclaim(rc);
        throw(MAL, "dsh.oid_segment_append", MAL_MALLOC_FAIL);
    }

    spos = 0;

    /* all inputs have at least one element and all allocations worked */
    if (!BATtdense(l) && !BATtdense(r)) {
        lvals = (oid *) Tloc(l, 0);
        rvals = (oid *) Tloc(r, 0);
        lend = lvals + lcnt;
        rend = rvals + rcnt;

        lval = *lvals;
        lvals++;
        rval = *rvals;
        rvals++;

        while (1) {
            /* start with left side */
            while (lval <= rval) {
                APPENDLEFT();
                if (lvals >= lend) goto spool_right_add_current;
                lval = *lvals;
                lvals++;
            }

            /* either start or continue with right side */
            while (lval > rval) {
                APPENDRIGHT();
                if (rvals >= rend) goto spool_left_add_current;
                rval = *rvals;
                rvals++;
            }
        }
    }
    else if (BATtdense(l) && !BATtdense(r)) {
        lval = l->tseqbase;
        lvalend = lval + lcnt;

        rvals = (oid *) Tloc(r, 0);
        rend = rvals + rcnt;
        rval = *rvals;
        rvals++;

        while (1) {
            if (lval == rval) {
                APPENDLEFT();
                lval++;
                if (lval >= lvalend) goto spool_right_add_current;
                APPENDRIGHT();
                if (rvals >= rend) goto spool_virtual_left_add_current;
                rval = *rvals;
                rvals++;
            }
            else if (lval < rval) {
                /* add one left until we're even with right */
                do {
                    APPENDLEFT();
                    lval++;
                    if (lval >= lvalend) {
                        /* spool right */
                        goto spool_right_add_current;
                    }
                } while (lval < rval);
            }
            /* lval > rval */
            else {
                /* add all from right until we're even with left */
                do {
                    APPENDRIGHT();
                    if (rvals >= rend) {
                        /* right exhausted add remaining virtual left */
                        while (lval < lvalend) {
  spool_virtual_left_add_current:
                            APPENDLEFT();
                            lval++;
                        }
                        goto clean_up_success;
                    }
                    rval = *rvals;
                    rvals++;
                } while (lval > rval);
            }
        }
    }
    else if (!BATtdense(l) && BATtdense(r)) {
        /* dense right side */
        lvals = (oid *) Tloc(l, 0);
        lend = lvals + lcnt;
        lval = *lvals;
        lvals++;
        rval = r->tseqbase;
        rvalend = rval + rcnt;

        while (1) {
            /* assumptions:
                   lval is the current value
                   lvals points to the next value
            */
            if (lval == rval) {
                /* add all from left segment */
                do {
                    APPENDLEFT();
                    if (lvals >= lend) goto spool_virtual_right_add_current;
                    lval = *lvals;
                    lvals++;
                } while (lval == rval);
                APPENDRIGHT();
                rval++;
                if (rval >= rvalend) goto spool_left_add_current;
            }
            else if (lval < rval) {
                /* spool left until we're even with virtual right */
                do {
                    APPENDLEFT();
                    if (lvals >= lend) {
  spool_virtual_right_add_current:
                        /* left exhausted, spool virtual right */
                        do {
                            APPENDRIGHT();
                            rval++;
                        } while (rval < rvalend);
                        goto clean_up_success;
  spool_virtual_right:
                        if (rval < rvalend) goto spool_virtual_right_add_current;
                        goto clean_up_success;
                    }
                    lval = *lvals;
                    lvals++;
                }
                while (lval < rval);
            }
            /* lval > rval */
            else {
                /* catch up with left */
                do {
                    APPENDRIGHT();
                    rval++;
                    if (rval >= rvalend) {
                        goto spool_left_add_current;
                    }
                } while (lval > rval);
            }
        }
    }
    else {
        /* both sides dense */

        lval = l->tseqbase;
        lvalend = lval + lcnt;
        rval = r->tseqbase;
        rvalend = rval + rcnt;

        /* handle prefixes */
        while (lval < rval) {
            /* left prefix */
            APPENDLEFT();
            lval++;
            if (lval >= lvalend) goto spool_virtual_right_add_current;
        }
        while (lval > rval) {
            /* right prefix */
            APPENDRIGHT();
            rval++;
            if (rval >= rvalend) goto spool_virtual_left_add_current;
        }

        /* handle intersection */
        while (1) {
            /* assertion: lval == rval */
            APPENDLEFT();
            APPENDRIGHT();
            lval++;
            if (lval >= lvalend) {
                /* refresh rval */
                rval = lval;
                goto spool_virtual_right;
            }
            if (lval >= rvalend) goto spool_virtual_left_add_current;
        }
        /* any remaining suffix will be handled by spool */
    }
  clean_up_success:

    BBPunfix(l->batCacheid);
    BBPunfix(r->batCacheid);

    /* set up candidate list properties */
    SET_CAND_PROP(lc);
    SET_CAND_PROP(rc);

    *sideid = s->batCacheid;
    BBPkeepref(s->batCacheid);
    *lcandid = lc->batCacheid;
    BBPkeepref(lc->batCacheid);
    *rcandid = rc->batCacheid;
    BBPkeepref(rc->batCacheid);

    return MAL_SUCCEED;

  spool_left_add_current:
    while (1) {
        APPENDLEFT();
        if (lvals >= lend) break;
        lval = *lvals;
        lvals++;
    }
    goto clean_up_success;
  spool_right_add_current:
    while (1) {
        APPENDRIGHT();
        if (rvals >= rend) break;
        rval = *rvals;
        rvals++;
    }
    goto clean_up_success;
}

#define SPTR_LOOP(TVALID, TCASE, EVALID, ECASE) \
    for (; sptr < send; sptr++) { \
        if (*sptr) { \
            if ( ( TVALID ) ) { \
                TCASE \
            } \
            else { \
                goto exhausted; \
            } \
        } \
        else { \
            if ( ( EVALID ) ) { \
                ECASE \
            } \
            else { \
                goto exhausted; \
            } \
        } \
    }

#define BITMERGE_LOOP_SWITCH_ENTRY(TYPE) \
    lptr = (bte *) Tloc(l, 0); \
    rptr = (bte *) Tloc(r, 0); \
    lend = lptr + BATcount(l) * sizeof(TYPE); \
    rend = rptr + BATcount(r) * sizeof(TYPE); \
    SPTR_LOOP( lptr < lend , APPEND(b, TYPE, * (TYPE *) lptr); lptr += sizeof(TYPE); , rptr < rend , APPEND(b, TYPE, * (TYPE *) rptr); rptr += sizeof(TYPE); ) \
    break;

dsh_export char *
DSHbitmerge(bat *res, const bat *side, const bat *left, const bat *right) {
    BAT * b;
    BAT * r;
    BAT * s;
    BAT * l;

    bit * sptr;
    bit * send;
    int tpe;

    bte * lptr;
    bte * lend;
    bte * rptr;
    bte * rend;

    BUN tsize;
    BUN lidx;
    BUN ridx;
    BATiter li;
    BATiter ri;

    BUN lcnt;
    BUN rcnt;
    BUN cnt;

    /* variables for dense implementation */
    oid lpos;
    oid rpos;

    s = BATdescriptor(*side);
    if (s == NULL) {
        throw(MAL, "dsh.bitmerge", RUNTIME_OBJECT_MISSING);
    }
    if (s->ttype != TYPE_bit) {
        BBPunfix(s->batCacheid);
        throw(MAL, "dsh.bitmerge", ILLEGAL_ARGUMENT "side is required to have bits");
    }

    l = BATdescriptor(*left);
    if (l == NULL) {
        BBPunfix(s->batCacheid);
        throw(MAL, "dsh.bitmerge", RUNTIME_OBJECT_MISSING);
    }
    r = BATdescriptor(*right);
    if (r == NULL) {
        BBPunfix(s->batCacheid);
        BBPunfix(l->batCacheid);
        throw(MAL, "dsh.bitmerge", RUNTIME_OBJECT_MISSING);
    }

    lcnt = BATcount(l);
    rcnt = BATcount(r);
    cnt = lcnt + rcnt;

    if (cnt != BATcount(s)) {
        BBPunfix(s->batCacheid);
        BBPunfix(l->batCacheid);
        BBPunfix(r->batCacheid);
        throw(MAL, "dsh.bitmerge", ILLEGAL_ARGUMENT ": bats have wrong size");
    }

    tpe = l->ttype == TYPE_void ? TYPE_oid : r->ttype == TYPE_void ? TYPE_oid : l->ttype;

    b = COLnew(0, tpe, cnt, TRANSIENT);
    if (b == NULL) {
        BBPunfix(s->batCacheid);
        BBPunfix(l->batCacheid);
        BBPunfix(r->batCacheid);
        throw(MAL, "dsh.bitmerge", MAL_MALLOC_FAIL);
    }

    sptr = (bit *) Tloc(s, 0);
    send = sptr + cnt;

    if (l->ttype == TYPE_void && r->ttype == TYPE_void) {
        /* dense implementation */
        lpos = 0;
        rpos = 0;
        // the actual value is composed by the tail sequence based
        // and the position
        SPTR_LOOP( lpos < lcnt
                 , APPEND(b, oid, l->tseqbase + lpos); lpos++;
                 , rpos < rcnt
                 , APPEND(b, oid, r->tseqbase + rpos); rpos++;
                 )
        goto clean_up_success;
    }
    else if (l->ttype == TYPE_void && r->ttype == TYPE_oid) {
        /* dense left implementation */
        lpos = 0;
        rptr = (bte *) Tloc(r, 0);
        rend = rptr + BATcount(r) * sizeof(oid);
        SPTR_LOOP( lpos < lcnt
                 , APPEND(b, oid, l->tseqbase + lpos); lpos++;
                 , rptr < rend
                 , APPEND(b, oid, * (oid *) rptr); rptr += sizeof(oid);
                 )
        goto clean_up_success;
    }
    else if (l->ttype == TYPE_oid && r->ttype == TYPE_void) {
        /* dense right implementation */
        lptr = (bte *) Tloc(l, 0);
        lend = lptr + BATcount(l) * sizeof(oid);
        rpos = 0;
        SPTR_LOOP( lptr < lend
                 , APPEND(b, oid, * (oid *) lptr); lpos += sizeof(oid);
                 , rpos < rcnt
                 , APPEND(b, oid, r->tseqbase + rpos); rpos++;
                 )
        goto clean_up_success;
    }
    else if (l->ttype != r->ttype) {
        BBPunfix(s->batCacheid);
        BBPunfix(l->batCacheid);
        BBPunfix(r->batCacheid);
        throw(MAL, "dsh.bitmerge", ILLEGAL_ARGUMENT ": types don't match");
    }

    switch (tpe) {
        case TYPE_bit: BITMERGE_LOOP_SWITCH_ENTRY(bit);
        case TYPE_bte: BITMERGE_LOOP_SWITCH_ENTRY(bte);
        case TYPE_sht: BITMERGE_LOOP_SWITCH_ENTRY(sht);
        case TYPE_int: BITMERGE_LOOP_SWITCH_ENTRY(int);
        case TYPE_lng: BITMERGE_LOOP_SWITCH_ENTRY(lng);
#ifdef HAVE_HGE
        case TYPE_hge: BITMERGE_LOOP_SWITCH_ENTRY(hge);
#endif
        case TYPE_flt: BITMERGE_LOOP_SWITCH_ENTRY(flt);
        case TYPE_dbl: BITMERGE_LOOP_SWITCH_ENTRY(dbl);
        case TYPE_oid: BITMERGE_LOOP_SWITCH_ENTRY(oid);
        default:
            /* l and r have same type */
            li = bat_iterator(l);
            ri = bat_iterator(r);
            tsize = Tsize(l);
            
            lidx = 0;
            ridx = 0;

            SPTR_LOOP( lidx < lcnt
                     , bunfastapp_nocheck(b, BUNlast(b), BUNtail(li, lidx), tsize); lidx++;
                     , ridx < rcnt
                     , bunfastapp_nocheck(b, BUNlast(b), BUNtail(ri, ridx), tsize); ridx++;
                     )
    }

  clean_up_success:

    /* FIXME lazy setup */
    b->tkey = 0;
    b->tsorted = cnt <= 1;
    b->trevsorted = cnt <= 1;
    b->tnonil = l->tnonil && r->tnonil;

    BBPunfix(l->batCacheid);
    BBPunfix(r->batCacheid);

    BBPkeepref(b->batCacheid);
    *res = b->batCacheid;

    return MAL_SUCCEED;

  bunins_failed:
    BBPunfix(l->batCacheid);
    BBPunfix(r->batCacheid);
    BBPreclaim(b);
    throw(MAL, "dsh.bitmerge", "bun insert failed");

  exhausted:
    BBPunfix(l->batCacheid);
    BBPunfix(r->batCacheid);
    BBPreclaim(b);
    throw(MAL, "dsh.bitmerge", ILLEGAL_ARGUMENT "single bat has wrong size");
}
#undef BITMERGE_LOOP_SWITCH_ENTRY
#undef SPTR_LOOP

dsh_export char *
DSHoid_segment_zip(bat *lcand, bat *rcand, const bat *left, const bat *right)
{
    BAT *lc;
    BAT *rc;
    BAT *l;
    BAT *r;

    oid * lvals;
    oid * rvals;
    oid lo;
    oid ro;
    oid lval;
    oid rval;

    BUN lcnt; 
    BUN rcnt;

    BUN cnt;

    /* used for dense implementation */
    oid hi;
    oid valend;


    l = BATdescriptor(*left);
    if (l == NULL) {
        throw(MAL, "dsh.oid_zip", RUNTIME_OBJECT_MISSING);
    }
    r = BATdescriptor(*right);
    if (r == NULL) {
        throw(MAL, "dsh.oid_zip", RUNTIME_OBJECT_MISSING);
    }

    if (!((l->ttype == TYPE_oid || l->ttype == TYPE_void) && (r->ttype == TYPE_oid || r->ttype == TYPE_void))) {
        throw(MAL, "dsh.oid_zip", ILLEGAL_ARGUMENT "wrong input types");
    }

    /* TODO property is not updated by MonetDB */
    //if (!l->tsorted || !r->tsorted) {
    //    throw(MAL, "dsh.oid_zip", ILLEGAL_ARGUMENT "inputs have to be sorted");
    //}

    lcnt = BATcount(l);
    rcnt = BATcount(r);
    /* upper bound, to prevent allocating too much */
    cnt = lcnt < rcnt ? lcnt : rcnt;


    if (BATtdense(l) && BATtdense(r)) {
        /* intersect ranges */

        /* max of lower bound */
        lo = l->tseqbase > r->tseqbase ? l->tseqbase : r->tseqbase;

        /* lend */
        lval = l->tseqbase + lcnt;
        /* rend */
        rval = r->tseqbase + rcnt;

        /* min of upper bound */
        hi =  lval < rval ? lval : rval;

        /* create 2 new candidate lists of type void */
        lc = BATdense(0, lo - l->tseqbase, hi - lo);
        rc = BATdense(0, lo - r->tseqbase, hi - lo);

    }
    else {
        lc = COLnew(0, TYPE_oid, cnt, TRANSIENT);
        rc = COLnew(0, TYPE_oid, cnt, TRANSIENT);
        if (lc == NULL || rc == NULL) {
            BBPunfix(l->batCacheid);
            BBPunfix(r->batCacheid);
            BBPreclaim(lc);
            BBPreclaim(rc);
            throw(MAL, "dsh.oid_zip", MAL_MALLOC_FAIL);
        }

        if (BATtdense(l)) {
            lval = l->tseqbase;
            lo = 0;
            valend = lval + lcnt;
            ro = 0;
            rvals = (oid *) Tloc(r, 0);

            if (lval < valend) {
                if (ro < rcnt) {
                    rval = rvals[ro];

                    /* zip merge loop */
                    while(1) {
                        /* there will be one match at most */
                        if (lval == rval) {
                            /* pair input */
                            APPEND(lc, oid, lo);
                            APPEND(rc, oid, ro);
                            lo++;
                            ro++;
                            if (lval >= valend || ro >= rcnt) break;
                            lval++;
                            rval = rvals[ro];
                        }
                        /* skip left */
                        if (lval < rval) {
                            /* directly jump to the value */
                            lo = lo + rval - lval;
                            lval = rval;
                            if (lval >= valend) goto finish;
                        }
                        /* skip right */
                        while (lval > rval) {
                            ro++;
                            if (ro >= rcnt) goto finish;
                            rval = rvals[ro];
                        }
                    }
                }
            }
        }
        else if (BATtdense(r)) {
            lvals = (oid *) Tloc(l, 0);
            lo = 0;
            ro = 0;
            rval = r->tseqbase;
            valend = rval + rcnt;

            if (lo < lcnt) {
                lval = lvals[lo];
                if (rval < valend) {

                    /* zip merge loop */
                    while(1) {
                        /* there will be one match at most */
                        if (lval == rval) {
                            /* pair input */
                            APPEND(lc, oid, lo);
                            APPEND(rc, oid, ro);
                            lo++;
                            ro++;
                            if (rval >= valend || lo >= lcnt) break;
                            lval = lvals[lo];
                            rval++;
                        }
                        /* skip left */
                        while (lval < rval) {
                            lo++;
                            if (lo >= lcnt) goto finish;
                            lval = lvals[lo];
                        }
                        /* skip right */
                        if (lval > rval) {
                            /* directly jump to the value */
                            ro = ro + lval - rval;
                            rval = lval;
                            if (rval >= valend) goto finish;
                        }
                    }
                }
            }
        }
        else {

            lo = 0;
            ro = 0;
            lvals = (oid *) Tloc(l, 0);
            rvals = (oid *) Tloc(r, 0);

            if (lo < lcnt) {
                lval = lvals[lo];
                
                if (ro < rcnt) {
                    rval = rvals[ro];

                    /* zip merge loop */
                    while(1) {
                        if (lval == rval) {
                            /* pair input */
                            APPEND(lc, oid, lo);
                            APPEND(rc, oid, ro);
                            lo++;
                            ro++;
                            if (lo >= lcnt || ro >= rcnt) break;
                            lval = lvals[lo];
                            rval = rvals[ro];
                        }
                        else {
                            /* skip left */
                            while (lval < rval) {
                                lo++;
                                if (lo >= lcnt) goto finish;
                                lval = lvals[lo];
                            }
                            /* skip right */
                            while (lval > rval) {
                                ro++;
                                if (ro >= rcnt) goto finish;
                                rval = rvals[ro];
                            }
                        }
                    };
                }
            }
        }
    }

  finish:

    BBPunfix(l->batCacheid);
    BBPunfix(r->batCacheid);

    /* could virtualize here, but it's probably a corner-case TODO track through tdense property ? */

    BBPkeepref(lc->batCacheid);
    *lcand = lc->batCacheid;

    BBPkeepref(rc->batCacheid);
    *rcand = rc->batCacheid;

    return MAL_SUCCEED;
}

/* should also be possible without intermediate candidate list, although it's not sure to pay off */
dsh_export char *
DSHreverse(bat *out, const bat *in)
{
    /* works on every kind of BAT, since only the BAT descriptor is used for BATcount */
    BAT *i;
    BAT *o;
    oid opos;

    i = BATdescriptor(*in);
    if (i == NULL)
        throw(MAL, "dsh.reverse", RUNTIME_OBJECT_MISSING);

    o = COLnew(0, TYPE_oid, BATcount(i), TRANSIENT);
    if (o == NULL) {
        BBPunfix(i->batCacheid);
        throw(MAL, "dsh.reverse", MAL_MALLOC_FAIL);
    }

    opos = BATcount(i);
    for (; opos > 0; opos--) {
        APPEND(o, oid, opos - 1);
    }

    BBPunfix(i->batCacheid);
    BBPkeepref(o->batCacheid);
    *out = o->batCacheid;

    return MAL_SUCCEED;
}

dsh_export char *
DSHoid_segment_reverse(bat *out, const bat *in)
{
    BAT *i;
    BAT *o;
    oid pos;
    oid seg_start_pos;
    oid pos_end;
    oid * vals;
    oid val;
    oid tmp;

    i = BATdescriptor(*in);
    if (i == NULL)
        throw(MAL, "dsh.oid_segment_reverse", RUNTIME_OBJECT_MISSING);

    /* input is key, return a void BAT */
    if (BATtkey(i)) {
        o = BATdense(0, 0, BATcount(i));

        BBPunfix(i->batCacheid);
        BBPkeepref(o->batCacheid);
        *out = o->batCacheid;
        return MAL_SUCCEED;
    }

    if (i->ttype != TYPE_oid)
        throw(MAL, "dsh.oid_segment_reverse", ILLEGAL_ARGUMENT "needs type oid");

    o = COLnew(0, TYPE_oid, BATcount(i), TRANSIENT);
    if (o == NULL) {
        BBPunfix(i->batCacheid);
        throw(MAL, "dsh.oid_segment_reverse", MAL_MALLOC_FAIL);
    }

    pos = 0;
    pos_end = BATcount(i);
    seg_start_pos = 0;
    vals = (oid *) Tloc(i, 0);

    if (pos < pos_end) {
        val = *vals;
        pos++;

        while (1) {

            if (pos < pos_end) {
                tmp = vals[pos];

                /* segment ended by value change */
                if (tmp != val) {
                    val = tmp;
                    tmp = pos;
                    for (; tmp > seg_start_pos; tmp--) {
                        APPEND(o, oid, tmp - 1);
                    }
                    seg_start_pos = pos;
                }
                pos++;
            }
            else {
                /* segment ended by end of BAT */
                for (;pos > seg_start_pos; pos--) {
                    APPEND(o, oid, pos - 1);
                }
                break;
            }
        }
    }

    BBPunfix(i->batCacheid);
    BBPkeepref(o->batCacheid);
    *out = o->batCacheid;

    return MAL_SUCCEED;
}

#define U_CONCAT_2(a,b) a##b
#define U_CONCAT_4(a,b,c,d) a##b##c##d
#define WIN_FUN_STR(n) "dsh.win_fun_" #n
#define WIN_FUN_PRE(name) \
    dsh_export str U_CONCAT_2(DSHwin_fun_,name) (bat *out, const bat *diff, const bat *in, const lng * wptr) \
    { \
        BAT *o; \
        BAT *d; \
        BAT *i; \
        \
        BUN cnt; \
        str msg; \
        \
        i = BATdescriptor(*in); \
        if (i == NULL) { \
            throw(MAL, WIN_FUN_STR(name), RUNTIME_OBJECT_MISSING); \
        } \
        d = BATdescriptor(*diff); \
        if (d == NULL) { \
            BBPunfix(i->batCacheid); \
            throw(MAL, WIN_FUN_STR(name), RUNTIME_OBJECT_MISSING); \
        } \
        \
        if (d->ttype != TYPE_bit) { \
            BBPunfix(i->batCacheid); \
            BBPunfix(d->batCacheid); \
            throw(MAL, WIN_FUN_STR(name), ILLEGAL_ARGUMENT ": consecutive difference of type :bat[:bit] expected"); \
        } \
        \
        cnt = BATcount(i); \
        \
        if (BATcount(d) != cnt) { \
            BBPunfix(i->batCacheid); \
            BBPunfix(d->batCacheid); \
            throw(MAL, WIN_FUN_STR(name), ILLEGAL_ARGUMENT ": input lengths don't match"); \
        } \
        \
        o = COLnew(0, i->ttype, cnt, TRANSIENT); \
        if (o == NULL) { \
            BBPunfix(i->batCacheid); \
            BBPunfix(d->batCacheid); \
            throw(MAL, WIN_FUN_STR(name), MAL_MALLOC_FAIL); \
        } \
        \
        switch (i->ttype) {

#define WIN_FUN_POST(name) \
            default: throw(MAL, WIN_FUN_STR(name), ILLEGAL_ARGUMENT ": invalid type"); \
            WIN_FUN_POST_NO_DEF

#define WIN_FUN_POST_NO_DEF \
        } \
        BBPunfix(i->batCacheid); \
        BBPunfix(d->batCacheid); \
        \
        if (msg == MAL_SUCCEED) { \
        \
            BBPkeepref(o->batCacheid); \
            *out = o->batCacheid; \
        } \
        return msg; \
    }

#define WIN_FUN_ENTRY(t,n) case U_CONCAT_2(TYPE_,t): \
    msg = U_CONCAT_4(DSHwin_fun_,n,_,t)(o, d, i, cnt, *wptr); \
    break;
/*

    Required window functions:
        - sum
        - min (also includes all)
        - max (also includes any)
        - avg
        - count (defined as the minimum of row_number and the window length and as row_number with infinite window)
        - first value
*/

WIN_FUN_PRE(sum)
    WIN_FUN_ENTRY(bte, sum)
    WIN_FUN_ENTRY(sht, sum)
    WIN_FUN_ENTRY(int, sum)
    WIN_FUN_ENTRY(lng, sum)
#ifdef HAVE_HGE
    WIN_FUN_ENTRY(hge, sum)
#endif
    WIN_FUN_ENTRY(flt, sum)
    WIN_FUN_ENTRY(dbl, sum)
WIN_FUN_POST(sum)

WIN_FUN_PRE(min)
    WIN_FUN_ENTRY(bit, min)
    WIN_FUN_ENTRY(bte, min)
    WIN_FUN_ENTRY(sht, min)
    WIN_FUN_ENTRY(int, min)
    WIN_FUN_ENTRY(lng, min)
#ifdef HAVE_HGE
    WIN_FUN_ENTRY(hge, min)
#endif
    WIN_FUN_ENTRY(flt, min)
    WIN_FUN_ENTRY(dbl, min)
WIN_FUN_POST(min)

WIN_FUN_PRE(max)
    WIN_FUN_ENTRY(bit, max)
    WIN_FUN_ENTRY(bte, max)
    WIN_FUN_ENTRY(sht, max)
    WIN_FUN_ENTRY(int, max)
    WIN_FUN_ENTRY(lng, max)
#ifdef HAVE_HGE
    WIN_FUN_ENTRY(hge, max)
#endif
    WIN_FUN_ENTRY(flt, max)
    WIN_FUN_ENTRY(dbl, max)
WIN_FUN_POST(max)

WIN_FUN_PRE(avg)
    WIN_FUN_ENTRY(bte, avg)
    WIN_FUN_ENTRY(sht, avg)
    WIN_FUN_ENTRY(int, avg)
    WIN_FUN_ENTRY(lng, avg)
#ifdef HAVE_HGE
    WIN_FUN_ENTRY(hge, avg)
#endif
    WIN_FUN_ENTRY(flt, avg)
    WIN_FUN_ENTRY(dbl, avg)
WIN_FUN_POST(avg)

/* sum window function

    DSH_WIN_FUN_IMPL_WINFUN
        - the name of the window function
    DSH_WIN_FUN_IMPL_INIT_ACC(lvexp, v)
        - an action to initially assing the accumulator denoted by lvexp a value
        v
    DSH_WIN_FUN_IMPL_COMBINE(lvexp, acc, v)
        - an action to combine the old accumulator acc with a value v as a new
        acc denoted by lvexp
    DSH_WIN_FUN_IMPL_FINISH(acc, accdel)
        - extract a value from the accumulator acc and the one that would be
        deleted in the next step accdel
    DSH_WIN_FUN_IMPL_ACC_TYPE(t)
        - the accumulator type parametrized over the value type
    DSH_WIN_FUN_IMPL_RES_TYPE(t)
        - the result type parametrized over the value type

*/
#define DSH_WIN_FUN_IMPL_WINFUN sum
#define DSH_WIN_FUN_IMPL_INIT_ACC(lvexp, v) (lvexp) = (v)
#define DSH_WIN_FUN_IMPL_COMBINE(lvexp, acc, v) (lvexp) = (acc) + (v)
#define DSH_WIN_FUN_IMPL_FINISH(acc, accdel) (acc)
#define DSH_WIN_FUN_IMPL_ACC_TYPE(t) t
#define DSH_WIN_FUN_IMPL_RES_TYPE(t) t

#include "dsh_win_fun_impl_inst_num_pp.h"

/* min window function */
#undef DSH_WIN_FUN_IMPL_WINFUN
#undef DSH_WIN_FUN_IMPL_COMBINE
#define DSH_WIN_FUN_IMPL_WINFUN min
#define DSH_WIN_FUN_IMPL_COMBINE(lvexp, acc, v) (lvexp) = (acc) < (v) ? (acc) : (v)

#define DSH_WIN_FUN_IMPL_TYPE bit
#include "dsh_win_fun_impl_pp.h"
#include "dsh_win_fun_impl_inst_num_pp.h"

/* max window function */
#undef DSH_WIN_FUN_IMPL_WINFUN
#undef DSH_WIN_FUN_IMPL_COMBINE
#define DSH_WIN_FUN_IMPL_WINFUN max
#define DSH_WIN_FUN_IMPL_COMBINE(lvexp, acc, v) (lvexp) = (acc) < (v) ? (v) : (acc)

#define DSH_WIN_FUN_IMPL_TYPE bit
#include "dsh_win_fun_impl_pp.h"
#include "dsh_win_fun_impl_inst_num_pp.h"

#undef DSH_WIN_FUN_IMPL_WINFUN
#undef DSH_WIN_FUN_IMPL_INIT_ACC
#undef DSH_WIN_FUN_IMPL_COMBINE
#undef DSH_WIN_FUN_IMPL_FINISH
#undef DSH_WIN_FUN_IMPL_ACC_TYPE
#define DSH_WIN_FUN_IMPL_WINFUN avg
#define DSH_WIN_FUN_IMPL_INIT_ACC(lvexp, v) do { \
        (lvexp).sum = v; \
        (lvexp).count = 1; \
    } while (0)
#define DSH_WIN_FUN_IMPL_COMBINE(lvexp, acc, v) do { \
        (lvexp).sum = (acc).sum + (v); \
        (lvexp).count = (acc).count + 1; \
    } while (0)
#define DSH_WIN_FUN_IMPL_FINISH(acc, accdel) (acc).sum / (acc).count
#define DSH_WIN_FUN_IMPL_ACC_TYPE(t) struct U_CONCAT_2(avg_cache_, t)

#include "dsh_win_fun_impl_inst_num_pp.h"

/* clean up */
#undef DSH_WIN_FUN_IMPL_WINFUN
#undef DSH_WIN_FUN_IMPL_INIT_ACC
#undef DSH_WIN_FUN_IMPL_COMBINE
#undef DSH_WIN_FUN_IMPL_FINISH
#undef DSH_WIN_FUN_IMPL_TYPE
#undef DSH_WIN_FUN_IMPL_ACC_TYPE
#undef DSH_WIN_FUN_IMPL_RES_TYPE

#undef WIN_FUN_ENTRY
#undef WIN_FUN_POST

dsh_export str DSHwin_fun_first_value_any(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w)
{
    lng pos;
    lng seg_start;

    seg_start = 0;
    for (pos = 0; pos < cnt; pos++) {
        /* segment done ? */
        if (* (bit *) Tloc(d, pos)) {
            /* reset segment start position */
            seg_start = pos;
        }
        /* check whether we can look back w, otherwise use the segment start position */
        bunfastapp_nocheck(o, BUNlast(o), Tloc(i, (pos - seg_start < w ? seg_start : pos - w + 1 )), Tsize(o));
    }

    return MAL_SUCCEED;

  bunins_failed:
    throw(MAL, "dsh.win_fun_first_value", "bun insert failed");
}

WIN_FUN_PRE(first_value)
    default:
        msg = DSHwin_fun_first_value_any(o, d, i, cnt, *wptr < 0 ? 0 : *wptr);
    break;
WIN_FUN_POST_NO_DEF

#undef WIN_FUN_STR
#undef WIN_FUN_PRE
#undef WIN_FUN_POST_NO_DEF
#undef U_CONCAT_4
#undef U_CONCAT_2

#undef APPENDLEFT
#undef APPENDRIGHT

#define APPENDLEFT() do { \
        APPEND(lc, oid, pppos); \
        APPEND(pp, oid, lpos); \
        lpos++; \
        pppos++; \
    } while(0) 

#define APPENDRIGHT() do { \
        APPEND(rc, oid, pppos); \
        APPEND(pp, oid, rpos + lcnt); \
        rpos++; \
        pppos++; \
    } while(0)

/* segment-wise append, under the assumption of sortedness, improved version:

   Instead of using bitmerge, append is used and a post-append mapping is
   created to avoid issues with branch prediction.

*/
dsh_export char *
DSHoid_segment_append2(bat *postproj, bat *lcandid, bat *rcandid, const bat * lid, const bat * rid)
{
    BAT *pp;
    BAT *lc;
    BAT *rc;

    BAT *l;
    BAT *r;

    BUN lcnt;
    BUN rcnt;
    BUN cnt;

    oid lpos;
    oid rpos;
    oid * lvals;
    oid * rvals;
    oid * lend;
    oid * rend;
    oid lval;
    oid rval;

    oid pppos;

    /* variables needed for void */
    oid lvalend;
    oid rvalend;

    l = BATdescriptor(*lid);
    if (l == NULL) {
        throw(MAL, "dsh.oid_segment_append", RUNTIME_OBJECT_MISSING);
    }
    r = BATdescriptor(*rid);
    if (r == NULL) {
        BBPunfix(l->batCacheid);
        throw(MAL, "dsh.oid_segment_append", RUNTIME_OBJECT_MISSING);
    }

    /* check for sortedness TODO property is not updated */
    //if (!l->tsorted || !r->tsorted) {
    //    BBPunfix(l->batCacheid);
    //    BBPunfix(r->batCacheid);
    //    throw(MAL, "dsh.oid_segment_append", ILLEGAL_ARGUMENT "inputs have to be sorted\n");
    //}

    /* check for correct types TODO void implementation*/
    if (!(l->ttype == TYPE_oid || l->ttype == TYPE_void)
        || !(r->ttype == TYPE_oid || r->ttype == TYPE_void)) {
        BBPunfix(l->batCacheid);
        BBPunfix(r->batCacheid);
        throw(MAL, "dsh.oid_segment_append", ILLEGAL_ARGUMENT "inputs have to be oid BATs\n");
    }

    lcnt = BATcount(l);
    rcnt = BATcount(r);
    cnt = lcnt + rcnt;

    /* sort out trivial cases for empty inputs */
    if (lcnt == 0 && rcnt == 0) {
        lc = BATdense(0, 0, 0);
        rc = BATdense(0, 0, 0);
        pp = BATdense(0, 0, 0);
        goto clean_up_success;
    }
    else if (lcnt == 0) {
        lc = BATdense(0, 0, 0);
        rc = BATdense(0, 0, rcnt);
        pp = BATdense(0, 0, rcnt);
        goto clean_up_success;
    }
    else if (rcnt == 0) {
        lc = BATdense(0, 0, lcnt);
        rc = BATdense(0, 0, 0);
        pp = BATdense(0, 0, lcnt);
        goto clean_up_success;
    }

    pp = COLnew(0, TYPE_oid, cnt, TRANSIENT);
    lc = COLnew(0, TYPE_oid, cnt, TRANSIENT);
    rc = COLnew(0, TYPE_oid, cnt, TRANSIENT);

    if (pp == NULL || lc == NULL || rc == NULL) {
        BBPunfix(l->batCacheid);
        BBPunfix(r->batCacheid);
        BBPreclaim(pp);
        BBPreclaim(lc);
        BBPreclaim(rc);
        throw(MAL, "dsh.oid_segment_append", MAL_MALLOC_FAIL);
    }

    pppos = 0;
    lpos = 0;
    rpos = 0;

    /* all inputs have at least one element and all allocations worked */
    if (!BATtdense(l) && !BATtdense(r)) {
        lvals = (oid *) Tloc(l, 0);
        rvals = (oid *) Tloc(r, 0);
        lend = lvals + lcnt;
        rend = rvals + rcnt;

        lval = *lvals;
        lvals++;
        rval = *rvals;
        rvals++;

        while (1) {
            /* start with left side */
            while (lval <= rval) {
                APPENDLEFT();
                if (lvals >= lend) goto spool_right_add_current;
                lval = *lvals;
                lvals++;
            }

            /* either start or continue with right side */
            while (lval > rval) {
                APPENDRIGHT();
                if (rvals >= rend) goto spool_left_add_current;
                rval = *rvals;
                rvals++;
            }
        }
    }
    else if (BATtdense(l) && !BATtdense(r)) {
        lval = l->tseqbase;
        lvalend = lval + lcnt;

        rvals = (oid *) Tloc(r, 0);
        rend = rvals + rcnt;
        rval = *rvals;
        rvals++;

        while (1) {
            if (lval == rval) {
                APPENDLEFT();
                lval++;
                if (lval >= lvalend) goto spool_right_add_current;
                APPENDRIGHT();
                if (rvals >= rend) goto spool_virtual_left_add_current;
                rval = *rvals;
                rvals++;
            }
            else if (lval < rval) {
                /* add one left until we're even with right */
                do {
                    APPENDLEFT();
                    lval++;
                    if (lval >= lvalend) {
                        /* spool right */
                        goto spool_right_add_current;
                    }
                } while (lval < rval);
            }
            /* lval > rval */
            else {
                /* add all from right until we're even with left */
                do {
                    APPENDRIGHT();
                    if (rvals >= rend) {
                        /* right exhausted add remaining virtual left */
                        while (lval < lvalend) {
  spool_virtual_left_add_current:
                            APPENDLEFT();
                            lval++;
                        }
                        goto clean_up_success;
                    }
                    rval = *rvals;
                    rvals++;
                } while (lval > rval);
            }
        }
    }
    else if (!BATtdense(l) && BATtdense(r)) {
        /* dense right side */
        lvals = (oid *) Tloc(l, 0);
        lend = lvals + lcnt;
        lval = *lvals;
        lvals++;
        rval = r->tseqbase;
        rvalend = rval + rcnt;

        while (1) {
            /* assumptions:
                   lval is the current value
                   lvals points to the next value
            */
            if (lval == rval) {
                /* add all from left segment */
                do {
                    APPENDLEFT();
                    if (lvals >= lend) goto spool_virtual_right_add_current;
                    lval = *lvals;
                    lvals++;
                } while (lval == rval);
                APPENDRIGHT();
                rval++;
                if (rval >= rvalend) goto spool_left_add_current;
            }
            else if (lval < rval) {
                /* spool left until we're even with virtual right */
                do {
                    APPENDLEFT();
                    if (lvals >= lend) {
  spool_virtual_right_add_current:
                        /* left exhausted, spool virtual right */
                        do {
                            APPENDRIGHT();
                            rval++;
                        } while (rval < rvalend);
                        goto clean_up_success;
  spool_virtual_right:
                        if (rval < rvalend) goto spool_virtual_right_add_current;
                        goto clean_up_success;
                    }
                    lval = *lvals;
                    lvals++;
                }
                while (lval < rval);
            }
            /* lval > rval */
            else {
                /* catch up with left */
                do {
                    APPENDRIGHT();
                    rval++;
                    if (rval >= rvalend) {
                        goto spool_left_add_current;
                    }
                } while (lval > rval);
            }
        }
    }
    else {
        /* both sides dense */

        lval = l->tseqbase;
        lvalend = lval + lcnt;
        rval = r->tseqbase;
        rvalend = rval + rcnt;

        /* handle prefixes */
        while (lval < rval) {
            /* left prefix */
            APPENDLEFT();
            lval++;
            if (lval >= lvalend) goto spool_virtual_right_add_current;
        }
        while (lval > rval) {
            /* right prefix */
            APPENDRIGHT();
            rval++;
            if (rval >= rvalend) goto spool_virtual_left_add_current;
        }

        /* handle intersection */
        while (1) {
            /* assertion: lval == rval */
            APPENDLEFT();
            APPENDRIGHT();
            lval++;
            if (lval >= lvalend) {
                /* refresh rval */
                rval = lval;
                goto spool_virtual_right;
            }
            if (lval >= rvalend) goto spool_virtual_left_add_current;
        }
        /* any remaining suffix will be handled by spool */
    }
  clean_up_success:

    BBPunfix(l->batCacheid);
    BBPunfix(r->batCacheid);

    /* set up candidate list properties */
    SET_CAND_PROP(lc);
    SET_CAND_PROP(rc);

    *postproj = pp->batCacheid;
    BBPkeepref(pp->batCacheid);
    *lcandid = lc->batCacheid;
    BBPkeepref(lc->batCacheid);
    *rcandid = rc->batCacheid;
    BBPkeepref(rc->batCacheid);

    return MAL_SUCCEED;

  spool_left_add_current:
    while (1) {
        APPENDLEFT();
        if (lvals >= lend) break;
        lval = *lvals;
        lvals++;
    }
    goto clean_up_success;
  spool_right_add_current:
    while (1) {
        APPENDRIGHT();
        if (rvals >= rend) break;
        rval = *rvals;
        rvals++;
    }
    goto clean_up_success;
}

#undef APPEND
#undef APPENDLEFT
#undef APPENDRIGHT
#undef SET_CAND_PROP
