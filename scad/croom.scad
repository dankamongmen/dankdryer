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

module croombottom(rad, z){
	brad = rad - z;
    translate([0, 0, z / 2]){
        rotate([0, 0, 45]){
            cylinder(z, brad, rad, $fn = 4, true);
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
    croombottom(botrad, wallz);
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
	h = 15;
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
    translate([0, 50, 0]){
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

// M4 screw hole through a 5x5x5 cube.
module mount(){
	c = 5;
	translate([0, 0, wallz + c / 2]){
		difference(){
			cube([c, c, c], true);
			screw("M4", length = wallz + c);
		}
	}
}

// 32.14 on the diagonal
module lmsmounts(){
	// holes are 31.8 and 16.4 apart at center
	// rotate it through motortheta
	// upper (most negative) left (most positive)
    translate([20, -40, 0]){
		mount();
	    // upper right ought be 16.4 away
		translate([-cos(motortheta) * 16.4,
					sin(motortheta) * 16.4,	0]){
			mount();
			// lower right ought be 31.8 away from UR
			translate([cos(motortheta) * 31.8,
						sin(motortheta) * 31.8, 0]){
				mount();
			}
        }
		// lower left ought be 31.8 from UL
		translate([cos(motortheta) * 31.8,
					sin(motortheta) * 31.8, 0]){
			mount();
		}
    }
}

// width: exterior 35mm with 3mm holes (32mm)
// length: exterior 75.3mm with 3mm holes (72.3mm)
module perfmounts(){
    translate([-mh - 15, -0.95 * totalxy / 2 + mh + 20, 0]){
        mount();
        translate([-72.3, 0, 0]){
            mount();
        }
        translate([0, 32, 0]){
            mount();
            translate([-72.3, 0, 0]){
                mount();
            }
        }
    }
}

// 2 relay mounts
module relay3vside(l){
    r = 1.5;
	holegapl = 10;
	holegapw = 63;
	translate([-holegapw / 2 - r, -holegapl / 2 - r, l / 2]){
		mount();
	}
	translate([holegapw / 2 + r, -holegapl / 2 - r, l / 2]){
		mount();
	}
}

// 3v3 relay for the motor. we want to replace
// this with a MOSFET.
module relay3v(height){
	translate([40, -botinrad + 60, 0]){
		relay3vside(height);
		mirror([0, 1, 0]){
			relay3vside(height);
		}
	}
}

// hole for AC wire
module achole(){
    translate([-botalt + croomwall / 2, 80, 0]){
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
					cylinder(2 * totalxy / 5, channelh / 2, channelh / 2, true);
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
		achole();
        fancablehole();
    }
    lowershield();
    acadapterscrews(6);
    translate([loadcellmountx, 0, wallz]){
        loadcellmount(loadcellmounth);
    }
	perfmounts();
	relay3v(wallz);
	lmsmounts();
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
