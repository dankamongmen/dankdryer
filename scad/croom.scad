// bottom chamber for a high-temperature (150C)
// filament dryer, intended to be printed in PC
// or PA. holds the MCU, motor, AC adapter, etc.
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

module foursquare(){
    children();
    mirror([1, 0, 0]){
        children();
		mirror([0, 1, 0]){
			children();
		}
    }
    mirror([0, 1, 0]){
        children();
    }
}

// the fundamental structure
module croomcore(){
    translate([0, 0, wallz]){
        rotate([0, 0, 45]){
            cylinder(croomz - wallz, botrad, toprad, $fn = 4);
        }
    }
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
			translate([12, 12, 0]){
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
			screw("M5", length = l, thread=true);
		}
        mirror([0, 1, 0]){
            acadaptermount(l);
			mirror([1, 0, 0]){
                acadaptermount(l){
					screw("M5", length = l, thread=true);
				}
            }
        }
        mirror([1, 0, 0]){
            acadaptermount(l);
        }
    }
}

module fullmount(c, h){
	translate([0, 0, wallz + h / 2]){
		cube([c, c, h], true);
	}
}

// M4 screw hole through a cXcXh cube.
module mount(c, h){
	difference(){
		fullmount(c, h);
		translate([0, 0, wallz + h / 2]){
			screw("M4", length = wallz + h, thread=true);
		}
	}
}

// width: mid2mid 65mm with 5mm holes
// length: mid2mid 90mm with 5mm holes
module perfmounts(){
	c = 5;
	h = 4;
    translate([-mh - 15, -0.95 * totalxy / 2 + mh + 10, 0]){
        mount(c, h);
        translate([-90, 0, 0]){
            mount(c, h);
        }
        translate([0, 65, 0]){
            mount(c, h);
            translate([-90, 0, 0]){
                mount(c, h);
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
    translate([-40, botinalt - 6, croomz]){
		rotate([180 - theta, 0, 0]){
            rotate([90, 0, 0]){
                cylinder(croomwall + 1, 5, 5, true);
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
	translate([-botinalt, botinalt / 3, rockerh / 2 + 10]){
	    rotate([0, 180 - theta, 0]){
			cube([croomwall + 2, rockerw, rockerh], true);
		}
	}
}

module croombottom(rad, z){
    translate([0, 0, z / 2]){
		difference(){
			rotate([0, 0, 45]){
				cylinder(z, rad, rad, $fn = 4, true);
			}
		}
    }
    acadapterscrews(5);
    translate([loadcellmountx, 0, wallz]){
        loadcellmount(loadcellmounth);
    }
	perfmounts();
    wirechannels();
    dropmotormount();
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
            rotate([theta, 0, 0]){
                fanhole(20);
            }
        }
        fancablehole();
		rockerhole();
    }
	croombottom(botrad, wallz);
}

multicolor("green"){
	croom();
}
// testing + full assemblage
/*
multicolor("black"){
    assembly();
}

multicolor("pink"){
    dropmotor();
}
*/
