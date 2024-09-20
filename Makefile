.PHONY: all deps clean

OUT:=out
ESP32HEX:=$(addprefix esp32-s3/, $(addsuffix .ino.elf, dankdryer/dankdryer))
HEX:=$(addprefix $(OUT)/, $(ESP32HEX))
CFLAGS:=--warnings all
ACLI:=arduino-cli
STL:=$(addsuffix .stl, $(addprefix $(OUT)/scad/, croom hotbox top))

all: $(HEX) $(STL)

deps:
	arduino-cli lib download HX711
	arduino-cli lib install HX711

$(OUT)/esp32-s3/dankdryer/dankdryer.ino.elf: $(addprefix esp32-s3/dankdryer/, dankdryer.ino dryer-network.h) $(ESPCOMMON)
	@mkdir -p $(@D)
	$(ACLI) compile $(CFLAGS) -b esp32:esp32:esp32s3 -v --output-dir $(@D) $<

$(OUT)/scad/hotbox.stl: scad/hotbox.scad scad/core.scad scad/tween_example6.scad
	@mkdir -p $(@D)
	time openscad -o $@ $<

$(OUT)/scad/%.stl: scad/%.scad scad/core.scad
	@mkdir -p $(@D)
	time openscad -o $@ $<

clean:
	rm -rf $(OUT)
