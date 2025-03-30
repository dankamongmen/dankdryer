// bottom chamber for a high-temperature (150C)
// filament dryer, intended to be printed in PC
// or PA. holds the PCB, scale, AC adapter, etc.
include <core.scad>

module fullmount(c, h){
	translate([0, 0, wallz + h / 2]){
		cube([c, c, h], true);
	}
}

// width: mid2mid 65mm with M5 holes
// length: mid2mid 90mm with M5 holes
pcbmountc = 6;
module pcbmounts(){
	rh = 4;
	th = 5;
	d = 4.75; // 4.5 is too small, 5 too big (ASA)
    translate([45, -0.95 * totalxy / 2 + mh + 15, 0]){
        mount(pcbmountc, rh, th, d);
        translate([-90, 0, 0]){
			mount(pcbmountc, rh, th, d);
        }
        translate([0, 66, 0]){
			mount(pcbmountc, rh, th, d);
			// this one is off-corner
			translate([-66, 0, 0]){
				mount(pcbmountc, rh, th, d);
			}
        }
    }
}

// RP-SMA female connector will anchor the antenna.
// it is .24in in diameter (6.19mm, 3.095mm radius)
module antennahole(){
	rpsmad = 6.3; // 6.19 is too small (ASA)
	l = croomwall * 3;
	translate([topalt / 2, -botalt + l / 4, croomz - 30]){
		rotate([theta, 0, 0]){
            rotate([90, 0, 0]){
				ScrewThread(rpsmad, l);
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
acmounth = 2;
module acadaptermount(){
	d = 3.8;
	c = 7; // total length of cutout
	// 3 on z matches ct in acadapterstand()
    translate([acadapterl / 2 - 2, -acadapterw / 2 + 7, acmounth]){
		linear_extrude(acmounth){
			hull(){
				circle(d / 2);
				translate([c - d, 0, 0]){
					circle(d / 2);
				}
			}
		}
    }
}

// the stand without the plug, since our AC
// adapters only have two places to screw
module acadapterstand(){
	translate([acadapterl / 2 - 0.5, -acadapterw / 2 + 7, acmounth / 2]){
		cube([8, 6, acmounth], true);
	}
}

// 60mm wide total
// screw holes are 6mm in from sides, so they start at
// 6mm (through 10mm) and 50mm (through 54mm)
// they have 2mm radius and 3mm height
module acadapterscrews(){
	translate([0, acadapterw, 0]){
        acadaptermount();
		acadapterstand();
        mirror([0, 1, 0]){
			acadapterstand();
			mirror([1, 0, 0]){
                acadaptermount();
				acadapterstand();
            }
        }
		mirror([1, 0, 0]){
			acadapterstand();
		}
    }
}

module croombottom(rad, z){
	translate([0, 0, wallz]){
		acadapterscrews();
		loadcellmountx = (-loadcelll + loadcellmountl) / 2;
		translate([loadcellmountx, 0, 0]){
			loadcellmount(loadcellmounth);
		}
		pcbmounts();
	}
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

module hotboxinnerplug(){
	translate([0, botrad * sqrt(2) / 2 - 3.5, croomz]){
		mirror([0, 1, 0]){
			translate([20, 0, 0]){
				chamberinnerplug();
			}
			translate([-20, 0, 0]){
				chamberinnerplug();
			}
		}
	}
}

module hotboxcornerplug(){
	r = botrad * sqrt(2) / 2 - 24;
	translate([r, r, croomz]){
		rotate([0, 0, -45]){
			mirror([0, 1, 0]){
				translate([10, 0, 0]){
					chamberinnerplug();
				}
				translate([-10, 0, 0]){
					chamberinnerplug();
				}
			}
		}
	}
}

module croom(iech = 20){
	difference(){
	union(){
	difference(){
		difference(){
			croomcore();
			difference(){
				croominnercore();
			}
		}
		//lcdset();
		fancablehole();
		rockerhole(iech);
		translate([0, -botalt, croomz / 2]){
			rotate([theta, 0, 0]){
				fanhole(10);
			}
		}
	}
	translate([botinalt - 10, 10, wallz]){
		rotate([0, 0, 270]){
			idtext();
		}
	}
	hotboxinnerplug();
	hotboxcornerplug();
	rotate([0, 0, 90]){
		hotboxinnerplug();
		hotboxcornerplug();
		rotate([0, 0, 90]){
			// plugs on the fan side never seem to
			// work; leave them off
			hotboxcornerplug();
			rotate([0, 0, 90]){
				hotboxinnerplug();
				hotboxcornerplug();
			}
		}
	}
	croombottom(botrad, wallz);
	}
	antennahole();		
	}
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
	translate([0, 50, wallz]){
		acadapter();
	}
}
*/
