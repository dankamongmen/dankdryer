include <core.scad>
use <threads.scad>

module top(){
	toph = 1.8; // multiple of both 0.3 and 0.2
    // fills in the top of the hotbox, having radius
    // innerr. give us a bit of smidge so it's not
    // a tight fit.
    translate([0, 0, toph]){
        difference(){
			ScrewThread(innerr * 2, ttopz, tpitch);
			translate([0, 0, ttopz / 2]){
				cylinder(ttopz, innerr - 4, innerr - 4, true);
			}
        }
    }
    linear_extrude(toph){
        circle(innerr + 2);
    }
}

multicolor("lightyellow"){
    top();
}
