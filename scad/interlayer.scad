// interlayer to be placed between the lower and
// upper chamber. they'll not always mate precisely,
// so a TPU interlayer keeps things airtight.
include <core.scad>

module interlayer(){
	h = 4;
	r = wallxy + 2;
	difference(){
		cube([totalxy + r / 2, totalxy + r / 2, h], true);
		cube([totalxy - r / 2, totalxy - r / 2, h], true);
	}
}

interlayer();