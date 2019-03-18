/*
 * log.h
 *
 *  Created on: Mar 15, 2019
 *      Author: osboxes
 */

#ifndef __LOG_H__
#define  __LOG_H__

#include <dlog.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "STAPP"

#define _E(fmt, args...) dlog_print(DLOG_ERROR, LOG_TAG, "%s : %s(%d) > "fmt"\n", rindex(__FILE__, '/') + 1, __func__, __LINE__, ##args)
#define _W(fmt, args...) dlog_print(DLOG_WARN, LOG_TAG, "%s : %s(%d) > "fmt"\n", rindex(__FILE__, '/') + 1, __func__, __LINE__, ##args)
#define _I(fmt, args...)  dlog_print(DLOG_INFO, LOG_TAG, "%s : %s(%d) > "fmt"\n", rindex(__FILE__, '/') + 1, __func__, __LINE__, ##args)
#define _D(fmt, args...) dlog_print(DLOG_DEBUG, LOG_TAG, "%s : %s(%d) > "fmt"\n", rindex(__FILE__, '/') + 1, __func__, __LINE__, ##args)

#define START dlog_print(DLOG_DEBUG, LOG_TAG, "%s : %s(%d) >>>>>>>> called", rindex(__FILE__, '/') + 1, __func__, __LINE__)
#define END dlog_print(DLOG_DEBUG, LOG_TAG, "%s : %s(%d) <<<<<<<< ended", rindex(__FILE__, '/') + 1, __func__, __LINE__)

#ifdef __cplusplus
}
#endif


#endif /* __LOG_H__ */
