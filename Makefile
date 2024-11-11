.PHONY: all firmware clean

OUT:=out
STL:=$(addsuffix .stl, \
 $(addprefix $(OUT)/scad/, boardlifter coupling croom hotbox top))

SCADFLAGS=--hardwarnings --backend Manifold --summary all

# allow OSCAD (openscad binary) to be set externally
OSCAD?=openscad

all: firmware $(STL)

# add actual esp-idf CMake output
firmware:
	@cd esp32-s3 && idf.py build

$(OUT)/scad/%.stl: scad/%.scad scad/core.scad
	@mkdir -p $(@D)
	time $(OSCAD) $(SCADFLAGS) -o $@ $<

clean:
	rm -rf $(OUT)
	@cd esp32-s3 && idf.py clean
