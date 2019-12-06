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

#define UART_RBR UART_BASE + 0
#define UART_THR UART_BASE + 0
#define UART_INTERRUPT_ENABLE UART_BASE + 4
#define UART_INTERRUPT_IDENT UART_BASE + 8
#define UART_FIFO_CONTROL UART_BASE + 8
#define UART_LINE_CONTROL UART_BASE + 12
#define UART_MODEM_CONTROL UART_BASE + 16
#define UART_LINE_STATUS UART_BASE + 20
#define UART_MODEM_STATUS UART_BASE + 24
#define UART_DLAB_LSB UART_BASE + 0
#define UART_DLAB_MSB UART_BASE + 4

#define _write_reg_u8(addr, value) *((volatile uint8_t *)addr) = value

#define _read_reg_u8(addr) (*(volatile uint8_t *)addr)

#define _is_transmit_empty() (read_reg_u8(UART_LINE_STATUS) & 0x20)

#define _write_serial(a) \
    while (_is_transmit_empty() == 0) {}; \
    write_reg_u8(UART_THR, a); \

void write_reg_u8(uintptr_t addr, uint8_t value) { _write_reg_u8(addr, value); }
uint8_t read_reg_u8(uintptr_t addr) { return _read_reg_u8(addr); }
int is_transmit_empty(uintptr_t UART_BASE) { return _is_transmit_empty(); }
uint8_t uart_line_status(uintptr_t UART_BASE) { return _read_reg_u8(UART_LINE_STATUS); }
void write_serial(uintptr_t UART_BASE, char a) { _write_serial(a); }

void init_uart(uintptr_t UART_BASE, uint16_t baud)
{
    _write_reg_u8(UART_INTERRUPT_ENABLE, 0x00); // Disable all interrupts
    _write_reg_u8(UART_LINE_CONTROL, 0x80);     // Enable DLAB (set baud rate divisor)
    _write_reg_u8(UART_DLAB_LSB, baud & 0xFF);  // Set divisor to 50M/16/baud (lo byte) 115200 baud => 27
    _write_reg_u8(UART_DLAB_MSB, baud >> 8);    //                   (hi byte)
    _write_reg_u8(UART_FIFO_CONTROL, 0xE7);     // Enable FIFO64
    _write_reg_u8(UART_LINE_CONTROL, 0x03);     // 8 bits, no parity, one stop bit
    _write_reg_u8(UART_FIFO_CONTROL, 0xC7);     // Enable FIFO, clear them, with 14-byte threshold
    _write_reg_u8(UART_MODEM_CONTROL, 0x2);     // flow control disabled
}

int get_uart_byte(uintptr_t UART_BASE)
{
  return read_reg_u8(UART_LINE_STATUS) & 0x1 ? read_reg_u8(UART_RBR) : -1;
}

void debug_uart_init(void)
{
  init_uart(CONFIG_DEBUG_UART_BASE, CONFIG_DEBUG_UART_CLOCK/115200 >> 4);
}

void printch(int ch)
{
  write_serial(CONFIG_DEBUG_UART_BASE, ch);
}
