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

  return 0;
}

#if 1
void board_debug_uart_init(void)
{
  /* For now nothing to do here. */

}
#endif
