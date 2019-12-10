# SPDX-License-Identifier: GPL-2.0+

dtb-$(CONFIG_TARGET_AX25_AE350) += ae350_32.dtb ae350_64.dtb
dtb-$(CONFIG_TARGET_LOWRISC_NEXYS4DDR) += lowrisc_nexys4ddr.dts.dtb

targets += $(dtb-y)

DTC_FLAGS += -R 4 -p 0x1000

PHONY += dtbs
dtbs: $(addprefix $(obj)/, $(dtb-y))
	@:

$(obj)/lowrisc_nexys4ddr.dts.dtb:
	sed -e "s/VENDOR/sifive/" -e "s/CPU/rocket/" -e "s/MEMSIZE/0x7E00000/" -e "s/BOARD/nexys4_ddr/" -e "s/ns16750/ns16550a/" $(obj)/generic.dts | dtc -I dts -O dtb -o $@

clean-files := *.dtb
