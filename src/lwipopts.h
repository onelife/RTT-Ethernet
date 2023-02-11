/***************************************************************************//**
 * @file    lwipopts.h
 * @brief   Arduino RTT-Ethernet library LwIP options header
 * @author  onelife <onelife.real[at]gmail.com>
 ******************************************************************************/
#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#ifdef __has_include
# if __has_include("lwipopts_rtt.h")
#  include "lwipopts_rtt.h"
# else
#  include "lwipopts_default.h"
#  if __has_include("lwipopts_extra.h")
#   include "lwipopts_extra.h"
#  endif
# endif
#else /* __has_include */
# include "lwipopts_default.h"
#endif /* __has_include*/

#endif /* __LWIPOPTS_H__ */
