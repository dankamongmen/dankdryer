.PHONY: all images firmware clean

OUT:=out
SCADBASE:=$(addprefix scad/, coupling croom hotbox top complete)
STL:=$(addsuffix .stl, $(addprefix $(OUT)/, $(SCADBASE)))
IMAGES:=$(addsuffix .png, $(addprefix $(OUT)/, $(SCADBASE)))

SCADFLAGS=--hardwarnings --viewall --autocenter --imgsize=1920,1080 \
					--render --backend Manifold --summary all --colorscheme=Starnight

# allow OSCAD (openscad binary) to be set externally
OSCAD?=openscad

all: firmware $(STL) $(IMAGES)

images: $(IMAGES)

# add actual esp-idf CMake output
firmware:
	@cd esp32-c6 && idf.py build

$(OUT)/scad/%.stl: scad/%.scad scad/core.scad
	@mkdir -p $(@D)
	time $(OSCAD) $(SCADFLAGS) -o $@ $<

$(OUT)/scad/%.png: scad/%.scad scad/core.scad
	@mkdir -p $(@D)
	time $(OSCAD) $(SCADFLAGS) -o $@ $<

clean:
	rm -rf $(OUT)
	rm -rf esp32-c6/build
