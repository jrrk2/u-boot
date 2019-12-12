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

int board_init(void)
{
  struct udevice *dev;

  for (uclass_first_device_check(UCLASS_SPI, &dev); dev;
       uclass_next_device_check(&dev)) {
    printf("Scanning SPI %s...\n", dev->name);
  }
  
  printf("u-boot relocated to %lX\n", gd->relocaddr);
  
  return 0;
}

void board_debug_uart_init(void)
{
  /* For now nothing to do here. */
}
