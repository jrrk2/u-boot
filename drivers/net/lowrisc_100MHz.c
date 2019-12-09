// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2007-2009 Michal Simek
 * (C) Copyright 2003 Lowrisc Inc.
 *
 * Michal SIMEK <monstr@monstr.eu>
 */

// #define DEBUG

#include <common.h>
#include <net.h>
#include <config.h>
#include <dm.h>
#include <console.h>
#include <malloc.h>
#include <asm/io.h>
#include <phy.h>
#include <miiphy.h>
#include <fdtdec.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

#define ENET_ADDR_LENGTH	6
#define ETH_FCS_LEN		4 /* Octets in the FCS */

/* Register offsets (in bytes) for the LowRISC Core */
#define TXBUFF_OFFSET       0x1000          /* Transmit Buffer */

#define MACLO_OFFSET        0x0800          /* MAC address low 32-bits */
#define MACHI_OFFSET        0x0808          /* MAC address high 16-bits and MAC ctrl */
#define TPLR_OFFSET         0x0810          /* Tx packet length */
#define TFCS_OFFSET         0x0818          /* Tx frame check sequence register */
#define MDIOCTRL_OFFSET     0x0820          /* MDIO Control Register */
#define RFCS_OFFSET         0x0828          /* Rx frame check sequence register(read) and last register(write) */
#define RSR_OFFSET          0x0830          /* Rx status and reset register */
#define RBAD_OFFSET         0x0838          /* Rx bad frame and bad fcs register arrays */
#define RPLR_OFFSET         0x0840          /* Rx packet length register array */

#define RXBUFF_OFFSET       0x4000          /* Receive Buffer */

/* MAC Ctrl Register (MACHI) Bit Masks */
#define MACHI_MACADDR_MASK    0x0000FFFF     /* MAC high 16-bits mask */
#define MACHI_FIAD_MASK       0x001F0000     /* PHY address */
#define MACHI_NOPRE_EN        0x00200000     /* No preamble flag */
#define MACHI_ALLPKTS_MASK    0x00400000     /* Rx all packets (promiscuous mode) */
#define MACHI_IRQ_EN          0x00800000     /* Rx packet interrupt enable */
#define MACHI_DIVIDER_MASK    0xFF000000     /* MDIO Clock Divider Mask */

/* MDIO Control Register Bit Masks */
#define MDIOCTRL_CTRLDATA_MASK 0x0000FFFF    /* MDIO Data Mask */
#define MDIOCTRL_RGAD_MASK     0x001F0000    /* MDIO Reg select Mask */
#define MDIOCTRL_WCTRL_MASK    0x00200000    /* MDIO Write Ctrl */
#define MDIOCTRL_RSTAT_MASK    0x00400000    /* MDIO Read Status */
#define MDIOCTRL_SCAN_MASK     0x00800000    /* MDIO Scan Status */
#define MDIOCTRL_BUSY_EN       0x01000000    /* MDIO Busy Status */
#define MDIOCTRL_LINKFAIL_EN   0x02000000    /* MDIO Link Fail */
#define MDIOCTRL_NVALID_EN     0x04000000    /* MDIO Not Valid Status */

/* Transmit Status Register (TPLR) Bit Masks */
#define TPLR_FRAME_ADDR_MASK  0x0FFF0000     /* Tx frame address */
#define TPLR_PACKET_LEN_MASK  0x00000FFF     /* Tx packet length */
#define TPLR_BUSY_MASK        0x80000000     /* Tx busy mask */

/* Receive Status Register (RSR) */
#define RSR_RECV_FIRST_MASK   0x0000000F      /* first available buffer (static) */
#define RSR_RECV_NEXT_MASK    0x000000F0      /* current rx buffer (volatile) */
#define RSR_RECV_LAST_MASK    0x00000F00      /* last available rx buffer (static) */
#define RSR_RECV_DONE_MASK    0x00001000      /* Rx complete */
#define RSR_RECV_IRQ_MASK     0x00002000      /* Rx irq bit */

/* General Ethernet Definitions */
#define HEADER_OFFSET               12      /* Offset to length field */
#define HEADER_SHIFT                16      /* Shift value for length */
#define ARP_PACKET_SIZE             28      /* Max ARP packet size */
#define HEADER_IP_LENGTH_OFFSET     16      /* IP Length Offset */

enum {queuelen = 1024, max_packet = 1536};

struct lowrisc_eth100BaseT {
	int phyaddr;
	struct phy_device *phydev;
	struct mii_dev *bus;
};

static inline void eth_write(phys_addr_t iobase, size_t addr, uint64_t data)
{
  volatile uint64_t *const eth_base = (volatile uint64_t *)iobase;
#ifdef DEBUG
  if ((addr < 0x8000) && !(addr&7))
#endif    
    {
#ifdef VERBOSE
      printf("eth_write(%lx,%lx)\n", addr, data);
#endif      
      eth_base[addr >> 3] = data;
    }
#ifdef DEBUG
  else
    printf("eth_write(%lx,%llx) out of range\n", addr, data);
#endif  
}

static inline uint64_t eth_read(phys_addr_t iobase, size_t addr)
{
  volatile uint64_t *const eth_base = (volatile uint64_t *)iobase;
  uint64_t retval = 0xDEADBEEF;
#ifdef DEBUG
  if ((addr < 0x8000) && !(addr&7))
#endif  
    {
      retval = eth_base[addr >> 3];
#ifdef VERBOSE
      printf("eth_read(%lx) returned %lx\n", addr, retval);
#endif      
    }
#ifdef DEBUG  
  else
    printf("eth_read(%lx) out of range\n", addr);
#endif  
  return retval;
}

static int eth100BaseT_read_rom_hwaddr(struct udevice *dev)
{
        struct eth_pdata *pdata = dev_get_platdata(dev);
        uchar *addr = pdata->enetaddr;
        uint64_t lo = eth_read(pdata->iobase, MACLO_OFFSET);
        uint64_t hi = eth_read(pdata->iobase, MACHI_OFFSET) & MACHI_MACADDR_MASK;
        eth_write(pdata->iobase, MACHI_OFFSET, hi & ~MACHI_IRQ_EN); /* disable ints */
        printf("MAC = %llx:%llx\n", hi&MACHI_MACADDR_MASK, lo);
        addr[0] = hi >> 8;
        addr[1] = hi >> 0;
        addr[2] = lo >> 24;
        addr[3] = lo >> 16;
        addr[4] = lo >> 8;
        addr[5] = lo >> 0;
        return 0;
}

static void eth100BaseT_stop(struct udevice *dev)
{
	debug("eth_stop\n");
}

static int eth100BaseT_start(struct udevice *dev)
{
	struct lowrisc_eth100BaseT *eth100BaseT = dev_get_priv(dev);
	struct eth_pdata *pdata = dev_get_platdata(dev);
        eth100BaseT_read_rom_hwaddr(dev);
        eth_write(pdata->iobase, RFCS_OFFSET, 8); /* use 8 buffers */
        assert(eth100BaseT->phyaddr == 1);
	return 0;
}

static int eth100BaseT_send(struct udevice *dev, void *ptr, int len)
{
	struct lowrisc_eth100BaseT *eth100BaseT = dev_get_priv(dev);
	struct eth_pdata *pdata = dev_get_platdata(dev);
        int i, rnd;
        assert(eth100BaseT->phyaddr == 1);
	if (len > PKTSIZE)
		len = PKTSIZE;

        rnd = (((len-1)|7)+1);
        const uint64_t *alloc = ptr;
      #ifdef VERBOSE
        printf("TX pending\n");
      #endif
        eth_write(pdata->iobase, TFCS_OFFSET,0);
      #if 0  
        do busy = eth_read(TPLR_OFFSET);
        while (TPLR_BUSY_MASK & busy);
      #endif  
        for (i = 0; i < rnd/8; i++)
          {
            eth_write(pdata->iobase, TXBUFF_OFFSET+(i<<3), alloc[i]);
          }
        eth_write(pdata->iobase, TPLR_OFFSET,len);
        return 0;
}

static int eth100BaseT_recv(struct udevice *dev, int flags, uchar **packetp)
{
	u32 retval = 0;
	struct eth_pdata *pdata = dev_get_platdata(dev);
	struct lowrisc_eth100BaseT *eth100BaseT = dev->priv;
        assert(eth100BaseT->phyaddr == 1);
        
        if (eth_read(pdata->iobase, RSR_OFFSET) & RSR_RECV_DONE_MASK)
          {
            int i;
            int rsr = eth_read(pdata->iobase, RSR_OFFSET);
            int buf = rsr & RSR_RECV_FIRST_MASK;
            int errs = eth_read(pdata->iobase, RBAD_OFFSET);
            int len = eth_read(pdata->iobase, RPLR_OFFSET+((buf&7)<<3)) - 4;
            if ((len >= 14) && (len <= max_packet) && ((0x101<<(buf&7)) & ~errs))
              {
                int rnd, start = (RXBUFF_OFFSET>>3) + ((buf&7)<<8);
                static uint64_t alloc[ETH_FRAME_LEN/sizeof(uint64_t)+1];
                volatile uint64_t *const eth_base = (volatile uint64_t *)(pdata->iobase);
                uint32_t *alloc32 = (uint32_t *)(eth_base+start);
                // Do we need to read the packet at all ??
                uint16_t rxheader = alloc32[HEADER_OFFSET >> 2];
                int proto_type = ntohs(rxheader) & 0xFFFF;
                switch (proto_type)
                    {
                    case ETH_P_IP:
                    case ETH_P_ARP:
                      rnd = (((len-1)|7)+1); /* round to a multiple of 8 */
                      for (i = 0; i < rnd/8; i++)
                        {
                          alloc[i] = eth_base[start+i];
                        }
                      debug("Packet receive from 0x%p, length %d bytes\n", eth_base, len);
                      *packetp = (uchar *)alloc;
                      retval = len;
                      break;            
                    case ETH_P_IPV6:
                      break;
                    }
              }
            eth_write(pdata->iobase, RSR_OFFSET, buf+1); /* acknowledge */
          }

	return retval;
}

static int eth100BaseT_miiphy_read(struct mii_dev *bus, int addr,
				int devad, int reg)
{
	u32 ret;
	u16 val = 0;

	ret = 0;
	debug("eth100BaseT: Read MII 0x%x, 0x%x, 0x%x, %d\n", addr, reg, val, ret);
	return val;
}

static int eth100BaseT_miiphy_write(struct mii_dev *bus, int addr, int devad,
				 int reg, u16 value)
{
	debug("eth100BaseT: Write MII 0x%x, 0x%x, 0x%x\n", addr, reg, value);
	return 0;
}

static int eth100BaseT_probe(struct udevice *dev)
{
	struct lowrisc_eth100BaseT *eth100BaseT = dev_get_priv(dev);
	int ret;

	eth100BaseT->bus = mdio_alloc();
	eth100BaseT->bus->read = eth100BaseT_miiphy_read;
	eth100BaseT->bus->write = eth100BaseT_miiphy_write;
	eth100BaseT->bus->priv = eth100BaseT;

	ret = mdio_register_seq(eth100BaseT->bus, dev->seq);
	if (ret)
		return ret;

	return 0;
}

static int eth100BaseT_remove(struct udevice *dev)
{
	struct lowrisc_eth100BaseT *eth100BaseT = dev_get_priv(dev);

	free(eth100BaseT->phydev);
	mdio_unregister(eth100BaseT->bus);
	mdio_free(eth100BaseT->bus);

	return 0;
}

static const struct eth_ops eth100BaseT_ops = {
	.start = eth100BaseT_start,
	.send = eth100BaseT_send,
	.recv = eth100BaseT_recv,
	.stop = eth100BaseT_stop,
        .read_rom_hwaddr = eth100BaseT_read_rom_hwaddr,
};

static int eth100BaseT_ofdata_to_platdata(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_platdata(dev);
	struct lowrisc_eth100BaseT *eth100BaseT = dev_get_priv(dev);
	int offset = 0;

	pdata->iobase = (phys_addr_t)devfdt_get_addr(dev);

	eth100BaseT->phyaddr = 1;

	offset = fdtdec_lookup_phandle(gd->fdt_blob, dev_of_offset(dev),
				      "phy-handle");
	if (offset > 0)
		eth100BaseT->phyaddr = fdtdec_get_int(gd->fdt_blob, offset,
						   "reg", -1);

	printf("ETH100BASET: phyaddr %d\n", eth100BaseT->phyaddr);

	return 0;
}

static const struct udevice_id eth100BaseT_ids[] = {
	{ .compatible = "lowrisc-eth" },
	{ }
};

U_BOOT_DRIVER(eth100BaseT) = {
	.name   = "eth100BaseT",
	.id     = UCLASS_ETH,
	.of_match = eth100BaseT_ids,
	.ofdata_to_platdata = eth100BaseT_ofdata_to_platdata,
	.probe  = eth100BaseT_probe,
	.remove = eth100BaseT_remove,
	.ops    = &eth100BaseT_ops,
	.priv_auto_alloc_size = sizeof(struct lowrisc_eth100BaseT),
	.platdata_auto_alloc_size = sizeof(struct eth_pdata),
};
