// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <rtc.h>

enum {t1980=315536400-3600};

/* Platform data */
struct rtc_pdata {
	phys_addr_t iobase;
};

static int lowrisc_rtc_get(struct udevice *dev, struct rtc_time *time)
{
        struct rtc_pdata *pdata = dev_get_platdata(dev);
        uint64_t *const iobase = (uint64_t *)(pdata->iobase);
        ulong seconds = iobase[7] >> 26;
        rtc_to_tm(seconds, time);
        return 0;
}

static int lowrisc_rtc_set(struct udevice *dev, const struct rtc_time *time)
{
	int ret = 0;
	struct rtc_pdata *pdata = dev_get_platdata(dev);
        uint64_t *const iobase = (uint64_t *)(pdata->iobase);
        ulong seconds = rtc_mktime(time);
        iobase[7] = seconds << 26;
	return ret;
}

static int lowrisc_rtc_reset(struct udevice *dev)
{
	int ret = 0;
	struct rtc_pdata *pdata = dev_get_platdata(dev);
        uint64_t *const iobase = (uint64_t *)(pdata->iobase);
        iobase[7] = 1575653500ULL << 26;
	return ret;
}

static const struct rtc_ops lowrisc_rtc_ops = {
	.get = lowrisc_rtc_get,
	.set = lowrisc_rtc_set,
	.reset = lowrisc_rtc_reset,
};

static int lowrisc_rtc_ofdata_to_platdata(struct udevice *dev)
{
	struct rtc_pdata *pdata = dev_get_platdata(dev);
	pdata->iobase = (phys_addr_t)devfdt_get_addr(dev);
	printf("LOWRISC_RTC: iobase %lX\n", pdata->iobase);
	return 0;
}

static const struct udevice_id lowrisc_rtc_ids[] = {
	{ .compatible = "lowrisc-rtc" },
	{ }
};

U_BOOT_DRIVER(rtc_lowrisc) = {
	.name	= "lowrisc-rtc",
	.id	= UCLASS_RTC,
	.of_match = lowrisc_rtc_ids,
	.ofdata_to_platdata = lowrisc_rtc_ofdata_to_platdata,
	.ops	= &lowrisc_rtc_ops,
	.platdata_auto_alloc_size = sizeof(struct rtc_pdata),
};
