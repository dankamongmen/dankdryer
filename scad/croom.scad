// bottom chamber for a high-temperature (150C)
// filament dryer, intended to be printed in PC
// or PA. holds the PCB, scale, AC adapter, etc.
include <core.scad>

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

module fullmount(c, h){
	translate([0, 0, wallz + h / 2]){
		cube([c, c, h], true);
	}
}

// 3mm square bottom plus (th + rh) M5 riser
module mount(c, rh, th){
	mounth = 3;
	d = 5;
	translate([0, 0, wallz + mounth / 2]){
		cube([c, c, mounth], true);
		translate([0, 0, (mounth) / 2]){
			RodStart(d, rh, th, 0, 0.7);
		}
	}
}

// width: mid2mid 65mm with M5 holes
// length: mid2mid 90mm with M5 holes
pcbmountc = 6;
module pcbmounts(){
	rh = 2;
	th = 5;
    translate([45, -0.95 * totalxy / 2 + mh + 10, 0]){
        mount(pcbmountc, rh, th);
        translate([-90, 0, 0]){
            mount(pcbmountc, rh, th);
        }
        translate([0, 65, 0]){
            mount(pcbmountc, rh, th);
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
                cylinder(10, 5, 5, true);
            }
        }
    }
}

// hole for four-prong rocker switch + receptacle.
// https://www.amazon.com/dp/B0CW2XJ339
// 45.7mm wide, 20mm high. if we can't print
// the bridge, we'll have to reorient it.
// model with fuse is 30mm
//rockerh = 20;
rockerh = 30;
rockerw = 45.7;
module rockerhole(iech){
	translate([-botinalt - 2, 0, iech / 2 + 15]){
	    rotate([0, 180 - theta, 0]){
			cube([croomwall + 2, rockerw, rockerh], true);
		}
	}
}

//acadapterh = 30; // old model
acadapterh = 22;
acadapterw = 50;
acadapterl = 135;
acmounth = locknuth + 2.5; // 7.5mm
module acadaptermount(l){
	d = 4; // M4
    translate([acadapterl / 2 - 2, -acadapterw / 2 + 7, wallz]){
		RodStart(d, l - locknuth, locknuth, 0, 0.7);
    }
}

// 60mm wide total
// screw holes are 6mm in from sides, so they start at
// 6mm (through 10mm) and 50mm (through 54mm)
module acadapterscrews(l){
    translate([0, acadapterw, 0]){
        acadaptermount(l);
        mirror([0, 1, 0]){
			mirror([1, 0, 0]){
                acadaptermount(l);
            }
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

module hotboxplug(){
	translate([0, botrad * sqrt(2) / 2 + 1, croomz]){
		translate([20, 0, 0]){
			chamberplug();
		}
		translate([-20, 0, 0]){
			chamberplug();
		}
	}
}

module croom(iech = 20){
	difference(){
		difference(){
			croomcore();
			difference(){
				croominnercore();
				corners();
			}
		}
		//lcdset();
		fancablehole();
		rockerhole(iech);
		antennahole();
		translate([0, -botalt, croomz / 2]){
			rotate([theta, 0, 0]){
				fanhole(10);
			}
		}
	}
	rotate([0, 0, 90]){
		hotboxplug();
		rotate([0, 0, 90]){
			hotboxplug();
		}
	}
	hotboxplug();
	croombottom(botrad, wallz);
}


multicolor("lightblue"){
	croom(rockerh);
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

module acadapter(){
	translate([0, 0, acadapterh / 2]){
		cube([acadapterl, acadapterw, acadapterh], true);
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
