include <core.scad>
use <hotbox.scad>
use <croom.scad>
use <coupling.scad>
use <top.scad>

multicolor("lightgreen"){
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

multicolor("black"){
	drop(){
		translate([0, 50, wallz + 7]){
			acadapter();
		}
	}
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

multicolor("dimgrey"){
    drop(){
        croom();
    }
}

multicolor("green"){
	drop(){
		pcb();
	}
}

multicolor("lightblue"){
    drop(){
        assembly();
    }
}

// top portion
rotate([0, 0, $t]){
	drop(){
		spool();
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
}

multicolor("silver"){
    translate([0, 0, totalz + 2]){
        mirror([0, 0, 1]){
            top();
        }
    }
}

multicolor("dimgrey"){
    rotate([0, 0, 180]){
        hotbox();
    }
}