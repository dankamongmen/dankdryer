include <core.scad>
use <threads.scad>

hhw = 10;
module handhold(){
	hhr = 10;
	translate([0, -hhw, 0]){
		rotate([90, 0, 0]){
			linear_extrude(hhw){
				circle(hhr);
			}
		}
	}
}

module top(){
	difference(){
		union(){
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
			difference(){
				intersection(){
					sphere(hhw * 2.5);
					translate([0, 0, hhw * 6 / 2]){
						cube(hhw * 6, true);
					}
				}
				translate([0, 0, hhw * 4]){
					cube(hhw * 5, true);
				}
			}
		}
		union(){
			handhold();
			mirror([0, 1, 0]){
				handhold();
			}
		}
	}
}

multicolor("lightyellow"){
    top();
}
