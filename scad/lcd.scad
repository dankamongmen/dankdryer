// ST7789V
lcdh = 37;
lcdw = 62; // total width (including mounts)
lcdaw = 51.5; // visible width
lcdt = 23; // thickness of mounts
module lcd(){
	lcdmt = 4;
	// 2.6mm diameter
	//3.25 + 3.4 mm offsets
	// want four circular mounts
	translate([-4, -lcdaw / 2, 0]){
		difference(){
			cube([lcdmt, (lcdw - lcdaw) / 2, lcdh], true);
			translate([0, -1.5, 15]){
				rotate([0, 90, 0]){
					cylinder(2, 1, 1);
				}
			}
			translate([0, -1.5, -15]){
				rotate([0, 90, 0]){
					cylinder(2, 1, 1);
				}
			}
		}
	}
	cube([lcdt, lcdaw, lcdh], true);
	translate([-4, lcdaw / 2, 0]){
		difference(){
			cube([lcdmt, (lcdw - lcdaw) / 2, lcdh], true);
			translate([0, 1.5, 15]){
				rotate([0, 90, 0]){
					cylinder(2, 1, 1);
				}
			}
			translate([0, 1.5, -15]){
				rotate([0, 90, 0]){
					cylinder(2, 1, 1);
				}
			}
		}
	}
}

module lcdset(){
	translate([botinalt, 0, croomz - lcdh / 2 - 10]){
		rotate([0, 180 + theta, 0]){
			lcd();
		}
	}
}

module rc522side(l){
    holer = 3.1 / 2;
    translate([-35.5 / 2 - holer, -32.25 / 2 - holer, l / 2]){
        screw("M4", length = l);
    }
    translate([34 / 2 + holer, -22.25 / 2 - holer, l / 2]){
        screw("M4", length = l);
    }
}

module rc522holes(l){
    rc522side(l);
    mirror([0, 1, 0]){
        rc522side(l);
    }
}
