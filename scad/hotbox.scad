// chamber for a high-temperature (150C) filament dryer
// intended to be printed in PA6-[CG]F.
include <core.scad>

// ceramic heater 230C  77x62
// holes: 28.3/48.6 3.5
ceramheat230w = 77;
module ceramheat230(rh, th){
	d = 3.5;
    holegapw = 32;
	holegapl = 52;
	mounth = 2;
	translate([-holegapw / 2, -holegapl / 2, mounth / 2]){
		cube([5, 5, mounth], true);
		translate([holegapw, 0, 0]){
			cube([5, 5, mounth], true);
			translate([0, holegapl, 0]){
				cube([5, 5, mounth], true);
			}
		}
		translate([0, holegapl, 0]){
			cube([5, 5, mounth], true);
		}
	}
	translate([-holegapw / 2, -holegapl / 2, mounth]){
		RodStart(d, rh, th, 0, 0.7);
		translate([holegapw, 0, 0]){
			RodStart(d, rh, th, 0, 0.7);
			translate([0, holegapl, 0]){
				RodStart(d, rh, th, 0, 0.7);
			}
		}
		translate([0, holegapl, 0]){
			RodStart(d, rh, th, 0, 0.7);
		}
	}
}

// three small holes for throughhole leads
module throughholes(x, y, h){
	translate([-40, 10, h / 2]){
		cylinder(h, 1, 1, true);
		translate([2, -2, 0]){
			cylinder(h, 1, 1, true);
			translate([2, -2, 0]){
				cylinder(h, 1, 1, true);
			}
		}
	}
}

// 2500mm² worth of air passage through one side
// of floor. the fan is 80mm x 80mm, suggesting
// 6400 (and thus 3200 per side), but it's actually
// only π40² ~= 5027.
module floorcuts(){
    d = wallz;
    // all floor holes ought be in the actual
	// hotbox, not under the infill (which would
	// cause support problems anyway).
    intersection(){
        translate([0, 0, d / 2]){
            cylinder(d, totalxy / 2 - 5, totalxy / 2 - 5, true);
        }
        union(){
            // a series of arcs
            intersection(){
                translate([-totalxy / 2 - 42, 0, 0]){
                    cube([totalxy / 2, totalxy / 2, wallz]);
                }
                for(i = [0:1:8]){
                    linear_extrude(wallz){
                        difference(){
                            circle(56 + i * 6);
                            circle(56 + i * 6 - 4);
                        }
                    }
                }
            }
            translate([-31, 105, d / 2]){
                cube([62, 14, d], true);
            }
        }
    }
}

module upcorner(){
    side = 40;
    t = totalxy / 2;
    translate([t - 15, t - 15, totalz - side / 2]){
        rotate([0, 0, 45]){
            cylinder(side * 3, side / 3, side / 2, true, $fn = 4);
        }
    }
}

module upcorners(){
	foursquare(){
		upcorner();
	}
}

module core(){
    translate([0, 0, wallz]){
        linear_extrude(totalz - wallz - ttopz){
            polygon(opoints);
        }
    }
}

module bottom(z, tround){
	br = botrad * sqrt(2);
	tr = toprad * sqrt(2);
	prismoid(size1=[br, br],
		size2=[tr, tr],
		h=z,
		rounding1 = topround,
		rounding2 = tround);
}

// hole and mounts for 175C thermocouple
// and heating element wires
module thermohole(){
    r = 8;
    l = 34;
    w = 6;
    cylinder(wallz, r, r, true);
    cube([w, l, wallz], true);
    // mounting holes are orthogonal to where we
	// drop in terminals
    translate([-12.5, 0, wallz / 2]){
        screw("M4", l = wallz * 2);
    }
    translate([12.5, 0, wallz / 2]){
        screw("M4", l = wallz * 2);
    }
}

module croomclip(){
	translate([0, botrad * sqrt(2) / 2 + 1.5, ccliph / 2]){
		translate([20, 0, 0]){
			chamberclip();
		}
		translate([-20, 0, 0]){
			chamberclip();
		}
	}
}

module hotbox(){
    difference(){
        union(){
            bottom(wallz, topround);
			intersection(){
				bottom(totalz, botround);
				difference(){
					core();
					// cut away top corners
					upcorners();
				}
			}
		}
        /*translate([-42, -40, 0]){
            rc522holes(wallz);
	    }*/
		// holes for our throughhole thermo+hall
		throughholes(-40, 10, wallz);
		throughholes(-35, 15, wallz);
        // we want to clear everything in the central
        // cylinder from the floor up.
        cheight = totalz - wallz;
		difference(){
			translate([0, 0, cheight / 2 + wallz]){
				cylinder(cheight, innerr, innerr, true, $fn=256);
			}
			// mounts for the ceramic heating element.
			// we want it entirely underneath the
			// spool, and closer to the perimeter
			// than the center.
			translate([0, totalxy / 4 + 8, wallz]){
				ceramheat230(1, 5);
			}
        }
        // now remove all other interacting pieces
        // 80x80mm worth of air passage
        floorcuts();
        mirror([1, 0, 0]){
            floorcuts();
        }
        // exhaust fan hole
        translate([0, -(totalxy - wallxy) / 2, 80 / 2 + wallz]){
            fanhole(wallxy + 16);
        }
        // central column
        translate([0, 0, wallz / 2]){
            cylinder(wallz, columnr, columnr, true);
        }
        // hole and mounts for 175C thermocouple and
		// heating element wires
        translate([50, -15, wallz / 2]){
            rotate([0, 0, 45]){
                thermohole();
            }
        }
    }
    translate([0, 0, totalz - ttopz]){
		// cut away top to get threading
		ScrewHole(innerr * 2,
				  ttopz,
				  pitch = tpitch){
			linear_extrude(ttopz){
				//difference(){
				circle(innerr + 3);
				//}
			}
		}
    }
	// clips for the croom
	rotate([0, 0, 180]){
		croomclip();
		rotate([0, 0, 90]){
			croomclip();
		}
	}
	croomclip();
}

hotbox();
// testing

/*
multicolor("green"){
	rotate([0, 0, $t]){
		spool();
	}
}

multicolor("orange"){
    top();
}
*/
	
