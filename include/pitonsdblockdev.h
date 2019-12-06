/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2013, Henrik Nordstrom <henrik@henriknordstrom.net>
 */

#ifndef __SANDBOX_BLOCK_DEV__
#define __SANDBOX_BLOCK_DEV__

enum {blk_cnt=1<<25}; // a temporary hack

struct pitonsd_block_dev {
  const char *dev_name;
  loff_t off, blkcnt;
  const void *buffer;
  int devnum;
};

int pitonsd_dev_bind(struct udevice *dev);

#endif
