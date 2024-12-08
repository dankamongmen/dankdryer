include <core.scad>
use <hotbox.scad>
include <croom.scad>
use <coupling.scad>
use <top.scad>

translate([-115, 6, -65]){
	rotate([0, 180 - theta, 0]){
		intersection(){
			rotate([270, 0, 0]){
				rotate([0, 0, 270]){
					import("iec.stl");
				}
			}
			cube([40, 120, 30], true);
		}
	}
}

multicolor("lightgreen"){
	translate([0, 109, 43]){
		rotate([0, 90, 90]){
			scale(17){
				import("noctua_nf-a12_fan.stl");
			}
		}
	}
	translate([0, -113, -41]){
		rotate([theta, 0, 0]){
			rotate([0, 90, 90]){
				scale(17){
					import("noctua_nf-a12_fan.stl");
				}
			}
		}
	}
}

multicolor("grey"){
    rotate([0, 0, 180]){
        hotbox();
    }
    drop(){
        croom();
    }
}

multicolor("cornsilk"){
    rotate([0, 0, $t]){
        spool();
    }
}

multicolor("black"){
    drop(){
        translate([0, 60, wallz + acadapterh / 2]){
            acadapter();
        }
    }
}

multicolor("lightblue"){
    drop(){
        assembly();
    }
}

multicolor("orange"){
    translate([0, 0, totalz + 4]){
        mirror([0, 0, 1]){
            top();
        }
    }
}