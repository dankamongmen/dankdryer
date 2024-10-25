// bottom chamber for a high-temperature (140C) filament dryer
// intended to be printed in ASA
// holds the MCU, motor, AC adapter, etc.
include <core.scad>

// thickness of croom xy walls (bottom is wallz)
croomwall = 3;
// the outer radii on our top and bottom
botrad = (totalxy + 14) * sqrt(2) / 2;
toprad = totalxy * sqrt(2) / 2;
// the inner radii on its top and bottom
botinrad = botrad - croomwall * sqrt(2);
topinrad = toprad - croomwall * sqrt(2);
// distances from center to mid-outer wall
topalt = toprad / sqrt(2);
botalt = botrad / sqrt(2);
topinalt = topinrad / sqrt(2);
botinalt = botinrad / sqrt(2);

module croombottom(){
    translate([0, 0, wallz / 2]){
        rotate([0, 0, 45]){
            cylinder(wallz, botrad - wallz, botrad, $fn = 4, true);
        }
    }
}

// the fundamental structure
module croomcore(){
    translate([0, 0, wallz]){
        rotate([0, 0, 45]){
            cylinder(croomz - wallz, botrad, toprad, $fn = 4);
        }
    }
    croombottom();
}

// the vast majority of the interior, removed
module croominnercore(){
    coreh = croomz - wallz;
    translate([0, 0, wallz]){
        rotate([0, 0, 45]){
            cylinder(coreh, botinrad, topinrad, $fn = 4);
        }
    }
}

adj = (botrad - toprad) / sqrt(2);
theta = (90 - atan(-croomz / adj));
// a corner at the top, to which the hotbox is mounted
module corner(){
    translate([-totalxy / 2, -totalxy / 2, croomz - 10]){
        difference(){
            union(){
                cube([44, 44, 20], true);
				translate([0, 22, -20])
				rotate([90, 0, 0])
				linear_extrude(44){
					polygon([
						[-22, -10],
						[22, 10],
						[-22, 10]
					]);
				}
            }
			translate([12, 12, 0]){
                screw("M5", length = 20);
            }
        }
    }
}

module rot(deg){
    rotate([theta + deg, 0, 0]){
        children();
    }
}

module acadaptermount(l){
    translate([165.1 / 2 - 2, -22, wallz + l / 2]){
        difference(){
            cube([7, 7, l], true);
			children();
        }
    }
}

// 60mm wide total
// screw holes are 6mm in from sides, so they start at
// 6mm (through 10mm) and 50mm (through 54mm)
module acadapterscrews(l){
    translate([0, 60, 0]){
        acadaptermount(l){
			screw("M5", length = l);
		}
        mirror([0, 1, 0]){
            acadaptermount(l);
			mirror([1, 0, 0]){
                acadaptermount(l){
					screw("M5", length = l);
				}
            }
        }
        mirror([1, 0, 0]){
            acadaptermount(l);
        }
    }
}

// mounts for the hotbox
module corners(){
    // cut top corners out of removal, leaving supports for top
    corner();
    mirror([1, 0, 0]){
        corner();
    }
    mirror([0, 1, 0]){
        corner();
    }
    mirror([1, 0, 0]){
		mirror([0, 1, 0]){
			corner();
		}
    }
}

module lmsmount(l){
	translate([0, 0, wallz]){
		screw("M4", length = wallz);
	}
}

// 32.14 on the diagonal
module lmsmounts(){
    // 12->5V mounts
    translate([39.8, -totalxy / 2 + 20, 0]){
       lmsmount(mh + 6);
    }
    translate([39.8, -totalxy / 2 + 36.4, 0]){
       lmsmount(mh + 6);
    }
    translate([8, -totalxy / 2 + 36.4, 0]){
       lmsmount(mh + 6);
    }
    translate([8, -totalxy / 2 + 20, 0]){
       lmsmount(mh + 6);
    }
}

module perfmount(h){
    screw("M4", length = wallz);
}

module perfmounts(){
    h = mh + 6;
    translate([-mh, -0.95 * totalxy / 2 + mh, wallz]){
        perfmount(h);
        translate([-80 - 3.3, 0, 0]){
            perfmount(h);
        }
        translate([0, 61.75 + 3.3, 0]){
            perfmount(h);
            translate([-80 - 3.3, 0, 0]){
                perfmount(h);
            }
        }
    }
}

// hole for AC wire
module achole(){
    translate([-botalt + croomwall / 2, 60, 0]){
        rotate([0, 180 - theta, 0]){
            translate([0, 0, wallz + 20]){
                rotate([0, 90, 0]){
                    cylinder(croomwall * 2, 7, 7, true);
                }
            }
        }
    }
}

// channel for ac wires running from adapter to heater
module wirechannel(){
    channelh = 20;
    translate([-botalt + croomwall, -20, wallz]){
        rotate([90, 0, 0]){
            intersection(){
                difference(){
                    cylinder(totalxy / 2, channelh / 2, channelh / 2, true);
                    cylinder(totalxy / 2, (channelh - 2) / 2, (channelh - 2) / 2, true);
                }
                translate([channelh / 2, channelh / 2, 0]){
                    cube([channelh, channelh, totalxy / 2], true);
                }

            }
        }
    }
}

module wirechannels(){
	mirror([1, 0, 0]){
		wirechannel();
	}
}

// hole for hotbox fan wires
module fancablehole(){
    rotate([180 - theta, 0, 0]){
        translate([-40, botinalt + 2, croomz - 8]){
            rotate([90, 0, 0]){
                cylinder(croomwall + 2, 3, 3, true);
            }
        }
    }
}

// we want to keep air from moving across the load
// cell's surface, so we put a shield around it.
module lowershield(){
    upperh = loadcellh;
    translate([0, -1, wallz + (loadcellmounth + upperh) / 2]){
        difference(){
            cube([shieldw + 4, 36 / 2, loadcellmounth + upperh], true);
            cube([shieldw + 2, 34 / 2, loadcellmounth + upperh], true);
			// we need to still be able to install the
			// load cell, though, so the shield is low
			// on the back.
			translate([0, 1, loadcellmounth / 2]){
				cube([shieldw + 2, 34 / 2, upperh], true);
			}
        }
    }
}

// hollow frustrum
module croom(){
    difference(){
        croomcore();
        difference(){
            croominnercore();
            corners();
        }
        translate([0, -botalt + 10, croomz / 2]){
            rot(0){
                fanhole(20);
            }
        }
		perfmounts();
		lmsmounts();
        achole();
        fancablehole();
    }
    lowershield();
    acadapterscrews(6);
    translate([loadcellmountx, 0, wallz]){
        loadcellmount(loadcellmounth);
    }
    wirechannels();
    dropmotormount();
}

croom();

// testing + full assemblage
/*
multicolor("black"){
    assembly();
}

multicolor("pink"){
    dropmotor();
}
*/
