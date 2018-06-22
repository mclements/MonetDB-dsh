/* instantiate number types */

#ifdef DSH_WIN_FUN_IMPL_TYPE
#undef DSH_WIN_FUN_IMPL_TYPE
#endif

#define DSH_WIN_FUN_IMPL_TYPE bte
#include "dsh_win_fun_impl_pp.h"
#undef DSH_WIN_FUN_IMPL_TYPE
#define DSH_WIN_FUN_IMPL_TYPE sht
#include "dsh_win_fun_impl_pp.h"
#undef DSH_WIN_FUN_IMPL_TYPE
#define DSH_WIN_FUN_IMPL_TYPE int
#include "dsh_win_fun_impl_pp.h"
#undef DSH_WIN_FUN_IMPL_TYPE
#define DSH_WIN_FUN_IMPL_TYPE lng
#include "dsh_win_fun_impl_pp.h"
#undef DSH_WIN_FUN_IMPL_TYPE
#define DSH_WIN_FUN_IMPL_TYPE dbl
#include "dsh_win_fun_impl_pp.h"
#undef DSH_WIN_FUN_IMPL_TYPE
#define DSH_WIN_FUN_IMPL_TYPE flt
#include "dsh_win_fun_impl_pp.h"
#ifdef HAVE_HGE
#undef DSH_WIN_FUN_IMPL_TYPE
#define DSH_WIN_FUN_IMPL_TYPE hge
#include "dsh_win_fun_impl_pp.h"
#endif
#undef DSH_WIN_FUN_IMPL_TYPE
