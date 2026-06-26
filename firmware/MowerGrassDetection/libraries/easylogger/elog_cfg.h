#ifndef _ELOG_CFG_H_
#define _ELOG_CFG_H_

/*---------------------------------------------------------------------------*/
#define ELOG_OUTPUT_ENABLE /* enable log output. */
#define ELOG_ASSERT_ENABLE /* enable assert check */

#define ELOG_OUTPUT_LVL             ELOG_LVL_DEBUG /* setting static output log level. range: from ELOG_LVL_ASSERT to ELOG_LVL_VERBOSE */
#define ELOG_LINE_BUF_SIZE          2048          /* buffer size for every line's log */
#define ELOG_LINE_NUM_MAX_LEN       5             /* output line number max length */
#define ELOG_FILTER_TAG_MAX_LEN     30            /* output filter's tag max length */
#define ELOG_FILTER_KW_MAX_LEN      16            /* output filter's keyword max length */
#define ELOG_FILTER_TAG_LVL_MAX_NUM 5             /* output filter's tag level max num */
#define ELOG_NEWLINE_SIGN           "\r\n"          /* output newline sign */

/*---------------------------------------------------------------------------*/
/* enable log color */
// #define ELOG_COLOR_ENABLE

/* change the some level logs to not default color if you want */
#define ELOG_COLOR_ASSERT  (F_MAGENTA B_NULL S_NORMAL)
#define ELOG_COLOR_ERROR   (F_RED B_NULL S_BOLD)
#define ELOG_COLOR_WARN    (F_YELLOW B_NULL S_NORMAL)
#define ELOG_COLOR_INFO    (F_GREEN B_NULL S_NORMAL)
#define ELOG_COLOR_DEBUG   (F_WHITE B_NULL S_NORMAL)
#define ELOG_COLOR_VERBOSE (F_BLUE B_NULL S_NORMAL)
/*---------------------------------------------------------------------------*/

#endif /* _ELOG_CFG_H_ */
