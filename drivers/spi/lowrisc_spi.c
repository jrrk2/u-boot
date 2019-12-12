/*
 * Simulate a SPI port
 *
 * Copyright (c) 2011-2013 The Chromium OS Authors.
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * Licensed under the GPL-2 or later.
 */

#define LOG_CATEGORY UCLASS_SPI

#include <common.h>
#include <dm.h>
#include <malloc.h>
#include <spi.h>
#include <os.h>

#include <linux/errno.h>
// QSPI commands
#define CMD_PP 0x02
#define CMD_READ 0x03
#define CMD_RDSR1 0x05
#define CMD_WREN 0x06
#define CMD_FR 0x0B
#define CMD_4PP 0x12
#define CMD_4READ 0x13
#define CMD_BRWR 0x17
#define CMD_OTPR 0x4B
#define CMD_RDID 0x9F
#define CMD_SE 0xD8
#define CMD_BE 0xC7

uint32_t qspistatus(void);
uint64_t qspi_send(uint8_t len, uint8_t quad, uint16_t data_in_count, uint16_t data_out_count, uint32_t *data);

static phys_addr_t GPIOBase;

uint32_t qspistatus(void)
{
  volatile uint64_t *swp = (volatile uint64_t *)GPIOBase;
  return swp[6]; // {spi_busy, spi_error}
}

uint64_t qspi_send(uint8_t len, uint8_t quad, uint16_t data_in_count, uint16_t data_out_count, uint32_t *data)
{
  uint64_t retval;
  uint32_t i, stat;
  volatile uint64_t *swp = (volatile uint64_t *)GPIOBase;
  swp[5] = (quad<<31) | ((len&127) << 24) | ((data_out_count&4095) << 12) | (data_in_count&4095);
  for (i = 0; i < len; i++)
    swp[5] = data[i];
  i = 0;
  do
    {
      stat = swp[6];
    }
  while ((stat & 0x2) && (++i < 1000));
  retval = swp[4];
#ifdef DEBUG_4READ
  printf("qspi_send(%d,%d,%d,%d,%.8X,%.8X,%.8llX);\n", len, quad, data_in_count, data_out_count, data[0], data[1], retval);
#endif
  return retval;
}

#ifdef DEBUG_4READ
uint8_t *qspi_read4(uint32_t addr)
      {
        static uint8_t dest[8];
        uint64_t rslt;
        uint32_t j, data[2];
        int data_in_count = 39;
        int data_out_count = 65;
        data[0] = CMD_4READ;
        data[1] = addr;
        rslt = qspi_send(2, 0, data_in_count, data_out_count, data);
        for (j = 0; j < 8; j++)
          {
            dest[j] = rslt >> (7-j)*8;
          }
#ifdef QSPI_VERBOSE
        for (j = 0; j < 8; j++)
          {
            puthex(dest[j], 2);
            printf(" ");
          }
        printf("\n");
#endif
        return dest;
      }
#endif

static int lowrisc_qspi_ofdata_to_platdata(struct udevice *bus)
{
        GPIOBase = (phys_addr_t)devfdt_get_addr(bus);
        printf("lowrisc_qspi iobase=%lx\n", GPIOBase);
        return 0;
}

static const char *decod(uint32_t cod)
{
  switch(cod)
    {
    case CMD_PP: return "CMD_PP";
    case CMD_READ: return "CMD_READ";
    case CMD_RDSR1: return "CMD_RDSR1";
    case CMD_WREN: return "CMD_WREN";
    case CMD_FR: return "CMD_FR";
    case CMD_4PP: return "CMD_4PP";
    case CMD_4READ: return "CMD_4READ";
    case CMD_BRWR: return "CMD_BRWR";
    case CMD_OTPR: return "CMD_OTPR";
    case CMD_RDID: return "CMD_RDID";
    case CMD_SE: return "CMD_SE";
    case CMD_BE: return "CMD_BE";
    default: return "CMD_UNKNOWN";
    }
}

static int lowrisc_qspi_xfer(struct udevice *slave, unsigned int bitlen,
			    const void *dout, void *din, unsigned long flags)
{
        int i, args = 2;
        int data_in_count = 39;
        int data_out_count = 65;
        static uint32_t data[128];
        uint64_t rslt;
        uint8_t *iptr = (uint8_t *)dout;
        uint8_t *optr = (uint8_t *)din;
#ifdef DEBUG_4READ
        uint8_t *read4;
#endif
        switch(flags)
          {
          case SPI_XFER_BEGIN:
          case SPI_XFER_BEGIN|SPI_XFER_END:
            data[1] = 0;
            for (i = 0; i < bitlen/8 - 1; i++)
                  {
#ifdef DEBUG            
                    printf(" %.2X", iptr[i]);
#endif
                    if (i)
                      data[1] |= iptr[i] << (bitlen - (i+2)*8);
                  }
            switch(*iptr)
              {
              case CMD_PP:
                data[0] = CMD_4PP;
                break;
              case CMD_FR:
                data[0] = CMD_4READ;
#ifdef DEBUG_4READ
                printf("\nqspi_read4(%.4X): ", data[1]);
                read4 = qspi_read4(data[1]);
                for (i = 0; i < 8; i++)
                  printf(" %.2X", read4[i]);
#endif                
                break;
              default:
                data[0] = *iptr;
                break;
              }
#ifdef DEBUG            
            printf("\nlowrisc_qspi iobase=%lx (cmd=%.2x): ", GPIOBase, *iptr);
#endif
            if (SPI_XFER_END & ~flags) return 0;
            break;
          case SPI_XFER_END:
            switch(data[0])
              {
              case CMD_4PP:
                memset(data+2, 0, bitlen/4);
                for (i = 0; i < bitlen/8; i++)
                  {
                    printf(" %.2X", iptr[i]);
                    args = 3 + i/4;
                    data[2+i/4] |= iptr[i] << (bitlen - i*8);
                  }
                data_in_count += bitlen;
                break;
              default:
                printf("bitlen = %d\n", bitlen);
              }
            break;
          }
        printf("\n%s: %.8X %.8X\n", decod(data[0]), data[0], data[1]);
        rslt = qspi_send(args, 0, data_in_count, data_out_count, data);
#ifdef DEBUG            
        printf("lowrisc_qspi iobase=%lx: ", GPIOBase);
#endif
        for (i = 0; i < bitlen/8; i++)
              {
                *optr = rslt >> (7-i)*8;
#ifdef DEBUG            
                printf(" %.2X", *optr);
#endif                
                ++optr;
              }
#ifdef DEBUG            
        printf("\n");
#endif                
	return 0;
}

static int lowrisc_qspi_set_speed(struct udevice *bus, uint speed)
{
	return 0;
}

static int lowrisc_qspi_set_mode(struct udevice *bus, uint mode)
{
	return 0;
}

static int lowrisc_cs_info(struct udevice *bus, uint cs,
			   struct spi_cs_info *info)
{
	/* Always allow activity on CS 0 */
	if (cs >= 1)
		return -EINVAL;

	return 0;
}

static const struct dm_spi_ops lowrisc_qspi_ops = {
	.xfer		= lowrisc_qspi_xfer,
	.set_speed	= lowrisc_qspi_set_speed,
	.set_mode	= lowrisc_qspi_set_mode,
	.cs_info	= lowrisc_cs_info,
};

static const struct udevice_id lowrisc_qspi_ids[] = {
	{ .compatible = "lowrisc-qspi" },
	{ }
};

U_BOOT_DRIVER(spi_lowrisc) = {
	.name	= "lowrisc-qspi",
	.id	= UCLASS_SPI,
	.of_match = lowrisc_qspi_ids,
	.ops	= &lowrisc_qspi_ops,
        .ofdata_to_platdata = lowrisc_qspi_ofdata_to_platdata,
};
