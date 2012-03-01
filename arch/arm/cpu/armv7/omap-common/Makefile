#
# (C) Copyright 2000-2003
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

include $(TOPDIR)/config.mk

LIB	= $(obj)libomap-common.o

SOBJS	:= reset.o

COBJS	:= timer.o
COBJS	+= utils.o
COBJS	+= gpio.o
COBJS	+= vc.o

ifdef CONFIG_SPL_BUILD
COBJS	+= spl.o
endif

ifdef CONFIG_FASTBOOT
COBJS	+= fastboot.o
endif

SRCS	:= $(SOBJS:.o=.S) $(COBJS:.o=.c)
OBJS	:= $(addprefix $(obj),$(SOBJS) $(COBJS))

ifdef CONFIG_SPL_BUILD
EXT_COBJ_FILES-$(CONFIG_SPL_VERIFY_IMAGE) += lib/crc32.o
SRCS	+= $(addprefix $(SRCTREE)/,$(EXT_COBJ_FILES-y:.o=.c))
OBJS	+= $(addprefix $(obj),$(notdir $(EXT_COBJ_FILES-y)))

# Rule to build generic library C files
$(obj)%.o: $(SRCTREE)/lib/%.c
	$(CC) -g $(CFLAGS) -c -o $@ $<

endif

all:	$(obj).depend $(LIB)

$(LIB):	$(OBJS)
	$(call cmd_link_o_target, $(OBJS))

#########################################################################

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
