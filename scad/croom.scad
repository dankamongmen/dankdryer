// bottom chamber for a high-temperature (150C)
// filament dryer, intended to be printed in PC
// or PA. holds the PCB, scale, AC adapter, etc.
include <core.scad>

// the vast majority of the interior, removed
module croominnercore(){
    coreh = croomz - wallz;
    translate([0, 0, wallz]){
		br = botinrad * sqrt(2);
		tr = topinrad * sqrt(2);
		prismoid(size1=[br, br],
			size2=[tr, tr],
			h=coreh,
			rounding1 = botround,
			rounding2 = topround);
    }
}


// a corner at the top, to which the hotbox is mounted
module corner(){
	h = 5;
	s = 40;
    translate([-totalxy / 2, -totalxy / 2, croomz - h / 2]){
        difference(){
            union(){
                cube([s, s, h], true);
				translate([0, s / 2, -h / 2 - 10]){
					rotate([90, 0, 0]){
						linear_extrude(44){
							polygon([
								[-s / 2, -10],
								[s / 2, 10],
								[-s / 2, 10]
							]);
						}
					}
				}
            }
			translate([14, 14, 0]){
                screw("M5", length = 10);
            }
        }
    }
}

// mounts for the hotbox
module corners(){
    // cut top corners out of removal,
	// leaving supports for top
	foursquare(){
		corner();
	}
}

module rot(deg){
    rotate([theta + deg, 0, 0]){
        children();
    }
}

//acadapterh = 30;
acadapterh = 22;
acadapterw = 50;
acadapterl = 135;
acmounth = 7;
module acadaptermount(l){
    translate([acadapterl / 2 - 2, -acadapterw / 2 + 7, wallz + l / 2]){
        children();
    }
}

// 60mm wide total
// screw holes are 6mm in from sides, so they start at
// 6mm (through 10mm) and 50mm (through 54mm)
module acadapterscrews(l){
	r = 4 / 2;
    translate([0, acadapterw, 0]){
        acadaptermount(l){
			cylinder(l, r, r, true);
		}
        mirror([0, 1, 0]){
			mirror([1, 0, 0]){
                acadaptermount(l){
					cylinder(l, r, r, true);
				}
            }
        }
    }
}

module acadapter(){
	translate([0, 0, acadapterh / 2]){
		cube([acadapterl, acadapterw, acadapterh], true);
	}
}

module fullmount(c, h){
	translate([0, 0, wallz + h / 2]){
		cube([c, c, h], true);
	}
}

// 3mm square bottom plus (h - 3mm) M5 riser
module mount(c, h, t){
	mounth = 3;
	riserh = h - mounth;
	r = 5 / 2;
	translate([0, 0, wallz + mounth / 2]){
		cube([r * 2 + 1, r * 2 + 1, mounth], true);
		translate([0, 0, mounth + riserh / 2]){
			cylinder(riserh, r, r, true);
		}
	}
}

// width: mid2mid 65mm with M5 holes
// length: mid2mid 90mm with M5 holes
pcbmountc = 6;
module pcbmounts(){
	h = 6;
    translate([45, -0.95 * totalxy / 2 + mh + 10, 0]){
        mount(pcbmountc, h, "M5");
        translate([-90, 0, 0]){
            mount(pcbmountc, h, "M5");
        }
        translate([0, 65, 0]){
            mount(pcbmountc, h, "M5");
        }
    }
}

// RP-SMA antenna will be glued in
module antennahole(){
	rpsmar = 12 / 2;
	rpsmal = 25;
	translate([-topalt / 2, -botalt + 10, croomz - 30]){
        rotate([theta, 0, 0]){
            rotate([90, 0, 0]){
				cylinder(rpsmal, rpsmar + 0.5, rpsmar + 0.5, true);
			}
        }
    }
}

// hole for hotbox fan wires
module fancablehole(){
    translate([-40, botinalt + croomwall / 2, croomz]){
		rotate([180 - theta, 0, 0]){
            rotate([90, 0, 0]){
                cylinder(croomwall, 5, 5, true);
            }
        }
    }
}

// hole for four-prong rocker switch + receptacle.
// https://www.amazon.com/dp/B0CW2XJ339
// 45.7mm wide, 20mm high. if we can't print
// the bridge, we'll have to reorient it.
rockerh = 20;
rockerw = 45.7;
module rockerhole(){
	translate([-botinalt - 2, botinalt / 3 - 15, rockerh / 2 + 15]){
	    rotate([0, 180 - theta, 0]){
			cube([croomwall + 2, rockerw, rockerh], true);
		}
	}
}

module croombottom(rad, z){
	acadapterscrews(acmounth);
	loadcellmountx = (-loadcelll + loadcellmountl) / 2;
    translate([loadcellmountx, 0, wallz]){
        loadcellmount(loadcellmounth);
    }
	pcbmounts();
}

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

// the fundamental structure (hollow frustrum
// continuously deformed into hollow cylinder)
module croomcore(){
	br = botrad * sqrt(2);
	tr = toprad * sqrt(2);
	prismoid(size1=[br, br],
		size2=[tr, tr],
		h=croomz,
		rounding1 = botround,
		rounding2 = topround);
}

module croom(){
	difference(){
		difference(){
			croomcore();
			difference(){
				croominnercore();
				corners();
			}
		}
		//lcdset();
		translate([0, -botalt + 3, (croomz + wallz) / 2]){
			rotate([theta, 0, 0]){
				fanhole(10);
			}
		}
		fancablehole();
		rockerhole();
		antennahole();
	}
	croombottom(botrad, wallz);
}


multicolor("blue"){
	croom();
}

module pcb(){
	pcbl = 100;
	pcbw = 75;
	pcbh = 1.6;
	translate([0, -pcbw / 2 - 22, wallz + pcbmountc]){
		rotate([0, 0, 180]){
			import("dankdryer.stl");
		}
	}
}

/*
// testing + full assemblage
multicolor("white"){
    assembly();
}

multicolor("green"){
	pcb();
}

multicolor("grey"){
	translate([0, 50, wallz + acmounth]){
		acadapter();
	}
}
*/
