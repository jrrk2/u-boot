// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2013 Henrik Nordstrom <henrik@henriknordstrom.net>
 */

#include <common.h>
#include <blk.h>
#include <dm.h>
#include <fdtdec.h>
#include <part.h>
#include <os.h>
#include <malloc.h>
#include <pitonsdblockdev.h>
#include <linux/errno.h>
#include <dm/device-internal.h>

DECLARE_GLOBAL_DATA_PTR;

typedef enum {
        DebugBase    = 0x00000000,
        ROMBase      = 0x00010000,
        CLINTBase    = 0x02000000,
        PLICBase     = 0x0C000000,
        UARTBase     = 0x41000000,
        SDBase       = 0x42000000,
        EthernetBase = 0x43000000,
        GPIOBase     = 0x44000000,
        KeybBase     = 0x45030000,
        VgaBase      = 0x45038000,
        FbBase       = 0x45080000,
        BTBase       = 0x46000000,
        DRAMBase     = 0x80000000
    } soc_bus_start_t;

enum {
  _piton_sd_ADDR_SD     ,
  _piton_sd_ADDR_DMA    ,
  _piton_sd_BLKCNT      ,
  _piton_sd_REQ_RD      ,
  _piton_sd_REQ_WR      ,
  _piton_sd_IRQ_EN      ,
  _piton_sd_SYS_RST     };

enum {
  _piton_sd_ADDR_SD_F=32,
  _piton_sd_ADDR_DMA_F  ,
  _piton_sd_STATUS      ,
  _piton_sd_ERROR       ,
  _piton_sd_INIT_STATE  ,
  _piton_sd_COUNTER     ,
  _piton_sd_INIT_FSM    ,
  _piton_sd_TRAN_STATE  ,
  _piton_sd_TRAN_FSM    };

enum {
  _piton_sd_STATUS_REQ_RD       = (0x00000001),
  _piton_sd_STATUS_REQ_WR       = (0x00000002),
  _piton_sd_STATUS_IRQ_EN       = (0x00000004),
  _piton_sd_STATUS_SD_IRQ       = (0x00000008),
  _piton_sd_STATUS_REQ_RDY      = (0x00000010),
  _piton_sd_STATUS_INIT_DONE    = (0x00000020),
  _piton_sd_STATUS_HCXC         = (0x00000040),
  _piton_sd_STATUS_SD_DETECT    = (0x00000080)
  };

enum { _piton_sd_NUM_MINORS = 16 };

volatile uint64_t *const sd_base = (volatile uint64_t *)SDBase;
volatile uint64_t *const sd_bram = (volatile uint64_t *)(SDBase + 0x8000);

static unsigned long pitonsd_block_read(struct udevice *dev,
				     unsigned long start, lbaint_t blkcnt,
				     void *buffer)
{
	struct pitonsd_block_dev *pitonsd_dev = dev_get_platdata(dev);
	struct blk_desc *block_dev = dev_get_uclass_platdata(dev);
        uint64_t vec;
        uint64_t stat = 0xDEADBEEF;
        uint64_t mask = (1 << blkcnt) - 1;

        if (block_dev->blksz != 512)
          return -1;
        
	pitonsd_dev->off = start;
        pitonsd_dev->buffer = buffer;
        pitonsd_dev->blkcnt = blkcnt;
        
        /* SD sector address */
        sd_base[ _piton_sd_ADDR_SD ] = start;
        /* always start at beginning of DMA buffer */
        sd_base[ _piton_sd_ADDR_DMA ] = 0;
        /* set sector count */
        sd_base[ _piton_sd_BLKCNT ] = blkcnt;
        sd_base[ _piton_sd_REQ_RD ] = 1;
        do
          {
            //            fence(); /* This is needed for a suspected Ariane bug */
            stat = sd_base[_piton_sd_STATUS];
          }
        while (_piton_sd_STATUS_REQ_RDY & ~stat);
        sd_base[ _piton_sd_REQ_RD ] = 0;
        vec = sd_base[ _piton_sd_ERROR ] & mask;
        memcpy(buffer, (void *)sd_bram, blkcnt * block_dev->blksz);
        if (vec==mask)
          return blkcnt;
        else
          return -1;
}

static unsigned long pitonsd_block_write(struct udevice *dev,
				      unsigned long start, lbaint_t blkcnt,
				      const void *buffer)
{
	struct pitonsd_block_dev *pitonsd_dev = dev_get_platdata(dev);
	struct blk_desc *block_dev = dev_get_uclass_platdata(dev);

        if (block_dev->blksz != 512)
          return -1;
        
	pitonsd_dev->off = start;
        pitonsd_dev->buffer = buffer;
        pitonsd_dev->blkcnt = blkcnt;
        
	return blkcnt;
}

int pitonsd_dev_unbind(struct udevice *dev)
{
  //	struct pitonsd_block_dev *pitonsd_dev = dev_get_platdata(dev);
	int ret;

        printf("pitonsd_dev_unbind(%p);\n", dev);

        ret = device_remove(dev, DM_REMOVE_NORMAL);
        if (ret)
          return ret;
        return device_unbind(dev);
}

int pitonsd_dev_bind(struct udevice *dev)
{
	struct pitonsd_block_dev *pitonsd_dev = dev_get_platdata(dev);
	char dev_name[20], *str;
	int ret;

        pitonsd_dev_unbind(dev);
        
        printf("pitonsd_dev_bind(%p);\n", dev);

	snprintf(dev_name, sizeof(dev_name), "pitonsd%d", pitonsd_dev->devnum);
	str = strdup(dev_name);
	if (!str)
		return -ENOMEM;

	ret = blk_create_device(gd->dm_root, "lowrisc-pitonsd", str,
				IF_TYPE_MMC, pitonsd_dev->devnum, 512,
				blk_cnt, &dev);
	if (ret)
		goto err;

	pitonsd_dev = dev_get_platdata(dev);
        pitonsd_dev->dev_name = str;
        
	ret = device_probe(dev);
	if (ret) {
		device_unbind(dev);
		goto err;
	}

	return 0;
err:
	free(str);
	return ret;
}

int pitonsd_dev_probe(struct udevice *dev)
{
	struct pitonsd_block_dev *pitonsd_dev = dev_get_platdata(dev);
        struct blk_desc *block_dev = dev_get_uclass_platdata(dev);
	int ret = 0;
        block_dev->if_type = IF_TYPE_MMC;
        block_dev->devnum = 0;
        block_dev->bdev = dev;
        block_dev->blksz = 512;
        block_dev->sig_type = SIG_TYPE_MBR;
        pitonsd_dev->dev_name = "pitonsd";
        
        return ret;
}

int pitonsd_get_dev_err(int devnum, struct blk_desc **blk_devp)
{
	struct udevice *dev;
	int ret;

	ret = blk_get_device(IF_TYPE_MMC, devnum, &dev);
	if (ret)
		return ret;
	*blk_devp = dev_get_uclass_platdata(dev);

	return 0;
}

static const struct udevice_id lowrisc_pitonsd_ids[] = {
	{ .compatible = "lowrisc-pitonsd" },
	{ }
};

static const struct blk_ops lowrisc_pitonsd_blk_ops = {
	.read	= pitonsd_block_read,
	.write	= pitonsd_block_write,
};

U_BOOT_DRIVER(lowrisc_pitonsd_blk) = {
	.name		= "lowrisc-pitonsd",
	.id		= UCLASS_BLK,
	.of_match	= lowrisc_pitonsd_ids,
	.ops		= &lowrisc_pitonsd_blk_ops,
	.probe		= pitonsd_dev_probe,
	.platdata_auto_alloc_size = sizeof(struct pitonsd_block_dev),
	.flags	        = DM_FLAG_PRE_RELOC,
};
