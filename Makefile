.PHONY: all images firmware clean

OUT:=out
SCADBASE:=$(addprefix scad/, coupling croom hotbox top complete)
STL:=$(addsuffix .stl, $(addprefix $(OUT)/, $(SCADBASE)))
IMAGES:=$(addsuffix .png, $(addprefix $(OUT)/, $(SCADBASE) scad/open))

SCADFLAGS=--hardwarnings --viewall --autocenter --imgsize=1920,1080 \
					--render --backend Manifold --summary all --colorscheme=Starnight

# allow OSCAD (openscad binary) to be set externally
OSCAD?=openscad

# building $(IMAGES) requires running under X =[
all: firmware $(STL)

images: $(IMAGES)

# add actual esp-idf CMake output
firmware:
	@cd esp32-c6 && idf.py build

$(OUT)/scad/%.stl: scad/%.scad scad/core.scad
	@mkdir -p $(@D)
	time $(OSCAD) -DOPENTOP=0 $(SCADFLAGS) -o $@ $<

$(OUT)/scad/%.png: scad/%.scad scad/core.scad
	@mkdir -p $(@D)
	time $(OSCAD) -DOPENTOP=0 $(SCADFLAGS) -o $@ $<

$(OUT)/scad/open.png: scad/complete.scad scad/core.scad
	@mkdir -p $(@D)
	time $(OSCAD) -DOPENTOP=1 $(SCADFLAGS) -o $@ $<

clean:
	rm -rf $(OUT)
	rm -rf esp32-c6/build
