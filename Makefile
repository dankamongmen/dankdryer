.PHONY: all deps clean

OUT:=out
ESP32HEX:=$(addprefix esp32-s3/, $(addsuffix .ino.elf, dankdryer/dankdryer))
HEX:=$(addprefix $(OUT)/, $(ESP32HEX))

ACLI:=arduino-cli

all: $(HEX)

deps:
	arduino-cli lib download HX711
	arduino-cli lib install HX711

$(OUT)/esp32-s3/dankdryer/dankdryer.ino.elf: $(addprefix esp32-s3/dankdryer/, dankdryer.ino) $(ESPCOMMON)
	@mkdir -p $(@D)
	$(ACLI) compile $(CFLAGS) -b esp32:esp32:node32s -v --output-dir $(@D) $<

clean:
	rm -rf $(OUT)
