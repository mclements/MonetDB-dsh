#define U_CONCAT_4(a, b, c, d) a##b##c##d
/* the function name */
#define FN(wf, ty) U_CONCAT_4(DSHwin_fun_,wf,_,ty)
#define AT DSH_WIN_FUN_IMPL_ACC_TYPE(DSH_WIN_FUN_IMPL_TYPE)
#define APP APPEND(o, DSH_WIN_FUN_IMPL_TYPE, DSH_WIN_FUN_IMPL_FINISH(acc[buff_pos], acc[(buff_pos - 1) % w]))

/* example: DSHwin_fun_sum_int */
str FN(DSH_WIN_FUN_IMPL_WINFUN, DSH_WIN_FUN_IMPL_TYPE) (BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w)
{
    lng tmp;
    lng pos;
    lng buff_pos;
    AT * acc;
    AT sng;

    if (cnt != 0) {
        /* no window ? */
        if (w <= 0) {
            DSH_WIN_FUN_IMPL_INIT_ACC(sng, * (DSH_WIN_FUN_IMPL_TYPE *) Tloc(i, 0));
            APPEND(o, DSH_WIN_FUN_IMPL_TYPE, DSH_WIN_FUN_IMPL_FINISH(sng, sng));
            for (pos = 1; pos < cnt; pos++) {
                /* segment done ? */
                if (* (bit *) Tloc(d, pos)) {
                    /* reset value */
                    DSH_WIN_FUN_IMPL_INIT_ACC(sng, * (DSH_WIN_FUN_IMPL_TYPE *) Tloc(i, pos));
                }
                else {
                    /* init from current value */
                    DSH_WIN_FUN_IMPL_INIT_ACC(sng, * (DSH_WIN_FUN_IMPL_TYPE *) Tloc(i, pos));
                }
                APPEND(o, DSH_WIN_FUN_IMPL_TYPE, DSH_WIN_FUN_IMPL_FINISH(sng, sng));
            }
        }
        else {
            acc = GDKmalloc(w * sizeof( AT ));

            if (acc == NULL) throw(MAL, "dsh.win_fun_sum", MAL_MALLOC_FAIL);

            /* init with first value */
            for (tmp = 0; tmp < w; tmp++) {
                DSH_WIN_FUN_IMPL_INIT_ACC(acc[tmp], * (DSH_WIN_FUN_IMPL_TYPE *) Tloc(i, 0));
            }
            buff_pos = 0;
            APP;
            for (pos = 1; pos < cnt; pos++) {
                /* segment done ? */
                if (* (bit *) Tloc(d, pos)) {
                    /* reset cyclic buffer entries */
                    for (tmp = 0; tmp < w; tmp++) {
                        DSH_WIN_FUN_IMPL_INIT_ACC(acc[tmp], * (DSH_WIN_FUN_IMPL_TYPE *) Tloc(i, pos));
                    }
                }
                else {
                    /* init buff_pos from current value */
                    DSH_WIN_FUN_IMPL_INIT_ACC(acc[buff_pos], * (DSH_WIN_FUN_IMPL_TYPE *) Tloc(i, pos));
                    /* for every other position combine */
                    for (tmp = (buff_pos + 1) % w; tmp != buff_pos; tmp = (tmp + 1) % w) {
                        DSH_WIN_FUN_IMPL_COMBINE(acc[tmp], acc[tmp], * (DSH_WIN_FUN_IMPL_TYPE *) Tloc(i, pos));
                    }
                    buff_pos = (buff_pos + 1) % w;
                }
                APP;
            }

            GDKfree(acc);
        }
    }

    return MAL_SUCCEED;
}

#undef AT
#undef FN
#undef U_CONCAT_4
