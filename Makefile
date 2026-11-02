# Witching Hour — a Halloween shoot-'em-up for the MEGA65 (cc45)
#
#   make              build build/witching.d81: the program (WITCHING) and its
#                     art and sounds (WITCHGFX1-3)
#   make run          build and boot it in xemu-xmega65
#   make demo         the self-playing build (build/demo/), in xemu
#   make screenshot   run the self-playing build headless, save build/screenshot.png
#   make preview      the art as PNG pictures in build/ (preview_*.png)
#   make play         the music on this computer (tune45 play)
#   make clean        remove build/
#
# Variables: M65 (the M65Compiler tree, default ../m65compiler), XEMU,
# SHOT_DELAY (seconds).

M65     ?= ../m65compiler
CC45    := $(M65)/bin/cc45
CA45    := $(M65)/bin/ca45
LN45    := $(M65)/bin/ln45
DISK45  := $(M65)/bin/disk45
TUNE45  := $(M65)/bin/tune45
STDLIB  := $(M65)/lib/build/c45.lib
XEMU    ?= xemu-xmega65
PYTHON  ?= python3

BUILD   ?= build
DEFS    ?=

CFLAGS  := -O2 -I$(M65)/lib/include -Isrc -I$(BUILD) $(DEFS)

TARGET  := $(BUILD)/witching.prg
DISK    := $(BUILD)/witching.d81
C_MODS  := main video sfx game
OBJS    := $(C_MODS:%=$(BUILD)/%.o45) $(BUILD)/load.o45 $(BUILD)/gfx.o45
HEADERS := $(wildcard src/*.h) $(BUILD)/gfx.h $(BUILD)/music.h
ART     := $(BUILD)/witchgfx1.bin $(BUILD)/witchgfx2.bin $(BUILD)/witchgfx3.bin

.PHONY: all run demo screenshot preview play clean

all: $(DISK)

$(BUILD):
	mkdir -p $@

$(BUILD)/gfx.h $(BUILD)/gfx.c $(ART) &: tools/gen_art.py tools/sprites.py tools/sounds.py | $(BUILD)
	$(PYTHON) tools/gen_art.py $(BUILD)

# the music: one blob, loaded into bank 1 at $1C000 (WITCHGFX4)
$(BUILD)/music.h $(BUILD)/music.bin &: assets/music.txt | $(BUILD)
	$(TUNE45) convert --bin --load-at 0xC000 assets/music.txt -o $(BUILD)/music.bin

$(BUILD)/%.o45: src/%.c $(HEADERS) | $(BUILD)
	$(CC45) $(CFLAGS) -c $< -o $@

$(BUILD)/gfx.o45: $(BUILD)/gfx.c $(BUILD)/gfx.h
	$(CC45) $(CFLAGS) -c $< -o $@

$(BUILD)/load.o45: src/load.s45 | $(BUILD)
	$(CA45) -c $< -o $@

$(TARGET): $(OBJS) $(STDLIB)
	$(LN45) -basic -M $(BUILD)/witching.map -o $@ $(OBJS) $(STDLIB)

$(STDLIB):
	$(MAKE) -C $(M65) lib

$(DISK): $(TARGET) $(ART) $(BUILD)/music.bin
	rm -f $@
	$(DISK45) create $@ -n "WITCHING HOUR" -i "WH"
	$(DISK45) add $@ $(TARGET) WITCHING
	$(DISK45) add $@ $(BUILD)/witchgfx1.bin WITCHGFX1
	$(DISK45) add $@ $(BUILD)/witchgfx2.bin WITCHGFX2
	$(DISK45) add $@ $(BUILD)/witchgfx3.bin WITCHGFX3
	$(DISK45) add $@ $(BUILD)/music.bin WITCHGFX4

run: $(DISK)
	$(XEMU) -8 $(abspath $(DISK)) -autoload

demo:
	$(MAKE) BUILD=build/demo DEFS=-DDEMO run

screenshot:
	$(MAKE) BUILD=build/demo DEFS=-DDEMO all
	CAPTURE_DELAY=$(or $(SHOT_DELAY),6) $(PYTHON) tools/screenshot.py build/demo/witching.prg $(abspath build/screenshot.png) build/demo/witching.d81

preview: | $(BUILD)
	$(PYTHON) tools/gen_art.py $(BUILD) --preview

play:
	$(TUNE45) play assets/music.txt

clean:
	rm -rf build
