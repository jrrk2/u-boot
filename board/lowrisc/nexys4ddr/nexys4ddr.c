// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <stdint.h>
#include <common.h>
#include <dm.h>
#include <rtc.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <debug_uart.h>

#ifdef CONFIG_MISC_INIT_R

int misc_init_r(void)
{
  /* For now nothing to do here. */

  return 0;
}

#endif

int board_init(void)
{
  /* For now nothing to do here. */
  rtc_reset ();
  return 0;
}

void board_debug_uart_init(void)
{
  /* For now nothing to do here. */

}

static uint64_t * const iobase = (uint64_t *)0x44000000;

void rtc_reset (void)
{
  iobase[7] = 1575653500ULL << 26;
}

enum {t1980=315536400-3600};

int rtc_get (struct rtc_time *tmp)
{
  time_t t1970 = iobase[7] >> 26;
  int t = t1970 - t1980;
  char months[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int d1 = t / 86400;
  int y = d1 * 4 / 1461;
  int hms = t % 86400;
  int h = hms / 3600;
  int m1 = hms % 3600;
  int m = m1 / 60;
  int s = m1 % 60;
  int dom, doy, mon = 0;
  if (y%4 == 0)
    {
      doy = d1 % 1461;
      months[1]++;
    }
  else
    {
      doy = (d1 % 1461 - 366) % 365;
    }
  dom = doy;
  while ((mon < 12) && (dom >= months[mon])) dom -= months[mon++];
  tmp->tm_sec = s;
  tmp->tm_min = m;
  tmp->tm_hour = h;
  tmp->tm_mday = dom+1;
  tmp->tm_mon = mon+1;
  tmp->tm_year = y+1980;
  tmp->tm_wday = (d1+1)%7 + 1;
  tmp->tm_yday = doy;
  tmp->tm_isdst = 0;
  return 0;
}

int rtc_set (struct rtc_time *tmp)
{
  return 0;
}
