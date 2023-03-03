TEENSY30 ?=
TARGET = main

OPT = -O2
MFLAGS = -mcpu=cortex-m4 -mtune=cortex-m4 -mthumb -mfloat-abi=soft
CPPFLAGS = -I. -D_GNU_SOURCE
CFLAGS = -Wall -Wextra -Wshadow -Wdouble-promotion -Wno-unused-parameter \
	 -Wno-misleading-indentation -Wno-missing-field-initializers \
	 -MMD -g $(OPT) $(MFLAGS) -nostdlib
LDFLAGS = $(OPT) -Wl,--gc-sections -Wl,--print-map $(MFLAGS) -Tmk20dx.ld
LIBS = -lc -lm

CPP = arm-none-eabi-cpp
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

ifdef TEENSY30
CPPFLAGS += -DTEENSY30
LOADER = ./loader -mmcu=mk20dx128
else
LOADER = ./loader -mmcu=mk20dx256
endif

SOURCES := callbacks.c display.c filter.c flow.c fonts.c i2c.c	\
	   input.c main.c pid.c power.c profile.c reset.c	\
	   temperature.c time.c usb.c

OBJS := $(SOURCES:.c=.o)
DEPS := $(SOURCES:.c=.d)

-include $(DEPS)

mk20dx.ld: mk20dx.ld.h mk20dx128.ld.h mk20dx256.ld.h
	$(CPP) -E -P $(CPPFLAGS) mk20dx.ld.h -o mk20dx.ld

$(TARGET).elf: $(OBJS) mk20dx.ld
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS) > $(TARGET).map

%.hex: %.elf
	$(SIZE) $<
	$(OBJCOPY) -O ihex -R .flexram $< $@

all: $(TARGET).hex
install: $(TARGET).hex
	-echo "b" > /dev/ttyACM0
	$(LOADER) -w -v $(TARGET).hex

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET).elf $(TARGET).hex $(TARGET).map \
	      mk20dx.ld pid filter

filter: filter.c
	cc -DTEST -g filter.c -lm -o filter -Wall -Wextra

pid: pid.c
	cc -DTEST -g pid.c -lm -o pid -Wall -Wextra -Wno-missing-field-initializers
