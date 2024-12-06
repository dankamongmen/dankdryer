include <core.scad>
use <hotbox.scad>
include <croom.scad>
use <coupling.scad>
use <top.scad>

multicolor("lightgreen"){
	translate([0, 109, 45]){
		rotate([0, 90, 90]){
			scale(17){
				import("noctua_nf-a12_fan.stl");
			}
		}
	}
	translate([0, -112, -40]){
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

/*
multicolor("orange"){
    translate([0, 0, totalz + 4]){
        mirror([0, 0, 1]){
            top();
        }
    }
}
*/