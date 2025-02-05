// bottom chamber for a high-temperature (150C)
// filament dryer, intended to be printed in PC
// or PA. holds the PCB, scale, AC adapter, etc.
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

// pyramid insets removed from the bottom
module bottomcorners(){
	foursquare(){
		translate([botalt, botalt, -0.5]){
			cylinder(20, 30, 1, $fn=4);
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
module pcbmounts(){
	c = 5;
	h = 4;
    translate([45, -0.95 * totalxy / 2 + mh + 10, 0]){
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

// RP-SMA antenna will be glued in
module antennahole(){
	rpsmar = 12.6 / 2;
	rpsmal = 25;
	translate([-topalt / 2, -botalt + 10, croomz - 30]){
        rotate([theta, 0, 0]){
            rotate([90, 0, 0]){
				cylinder(rpsmal, rpsmar + 1, rpsmar + 1, true);
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
	pcbmounts();
    wirechannels();
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

// the fundamental structure (hollow frustrum)
module croomcore(){
    translate([0, 0, wallz]){
        rotate([0, 0, 45]){
            cylinder(croomz - wallz, botrad, toprad, $fn = 4);
        }
    }
}

module bottomcornerfills(){
	foursquare(){
		translate([botalt - 25, botalt - 25, -14]){
			
		/*a = 0;
		b = -30;
		c = 30;
		multmatrix(m = [
		[cos(a)*cos(b), cos(a)*sin(b)*sin(c) - sin(a)*cos(c), cos(a)*sin(b)*cos(c) + sin(a)*sin(c), 1],
		[sin(a)*cos(b), sin(a)*sin(b)*sin(c) + cos(a)*cos(c), sin(a)*sin(b)*cos(c)-cos(a)*sin(c), 1],
		[-sin(b), cos(b)*sin(c), cos(b)*cos(c), 1],
		[0, 0, 0, 1]
					])*/
					s=29;
					rotate(a=44, v=[1, -1, 0]){
					linear_extrude(wallz)
			polygon([
				[s, s],
				[0, s],
				[s, 0]
				]);
				}
		}
	}
}

module croom(){
	difference(){
		union(){
			difference(){
				difference(){
					croomcore();
					difference(){
						croominnercore();
						corners();
					}
				}
				lcdset();
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
		bottomcorners();
	}
	bottomcornerfills();
}


multicolor("green"){
	croom();
}

/*
// testing + full assemblage
multicolor("white"){
    assembly();
}

multicolor("grey"){
	translate([0, 50, acadapterh / 2 + wallz]){
		acadapter();
	}
}
*/
