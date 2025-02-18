include <core.scad>
use <hotbox.scad>
use <croom.scad>
use <coupling.scad>
use <top.scad>

multicolor("slategrey"){
	translate([-110, -10, -60]){
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
	translate([-55, -118, -35]){
		import("antenna_WiFi_RP-SMA_conn.stl");
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
	translate([0, -109, -41]){
		rotate([theta, 0, 0]){
			rotate([0, 90, 90]){
				scale(17){
					import("noctua_nf-a12_fan.stl");
				}
			}
		}
	}
}

multicolor("dimgrey"){
    rotate([0, 0, 180]){
        hotbox();
    }
    drop(){
        croom();
    }
}

multicolor("darkorchid"){
	drop(){
		translate([0, 60, wallz + 7]){
			acadapter();
		}
	}
}

//multicolor("cornsilk"){
    rotate([0, 0, $t]){
        spool();
    }
//}

multicolor("lightblue"){
    drop(){
        assembly();
    }
}

multicolor("silver"){
    translate([0, 0, totalz + 4]){
        mirror([0, 0, 1]){
            top();
        }
    }
}
