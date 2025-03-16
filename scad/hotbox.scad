// chamber for a high-temperature (150C) filament dryer
// intended to be printed in PA6-[CG]F.
include <core.scad>

// ceramic heater 230C  77x62
// holes: 28.3/48.6 3.5
ceramheat230w = 77; // width
ceramheat230l = 62; // full length
ceramheat230ml = 44; // main section length
ceramheat230sidel =
 (ceramheat230l - ceramheat230ml) / 2; // side length
ceramheat230h = 6.3; // main section height
ceramheat230sideh = 1.8; // side section height
ceramheat230yoff = totalxy / 4 + 8;
ceramholegapw = 32;
ceramholegapr = 3.4 / 2;
// total height plus wallz ends up being:
//  3 (base) + 6.3 (ceramheat230h) == 9.3
// (the spool must clear this)

// the ceramic heating element has two thin sides
// into which the mounting holes are drilled
module heaterside(){
	r = ceramholegapr;
	difference(){
		cube([ceramheat230w,
			  ceramheat230sidel,
			  ceramheat230sideh], true);
		translate([ceramholegapw / 2, 0, 0]){
			cylinder(ceramheat230sideh, r, r, true);
		}
		translate([-ceramholegapw / 2, 0, 0]){
			cylinder(ceramheat230sideh, r, r, true);
		}
	}
	translate([-ceramholegapw / 2, 0, 1]){
		import("m4nyloc.stl");
	}
	translate([ceramholegapw / 2, 0, 1]){
		import("m4nyloc.stl");
	}
}

// the ceramic heating element
module heater(){
	translate([0, ceramheat230yoff, wallz + 3]){
		translate([0, 0, ceramheat230h / 2]){
			cube([ceramheat230w, ceramheat230ml, ceramheat230h], true);
		}
		translate([0, (ceramheat230ml + ceramheat230sidel)/ 2, ceramheat230sideh / 2]){
			heaterside();
		}
		translate([0, -(ceramheat230ml + ceramheat230sidel)/ 2, ceramheat230sideh / 2]){
			heaterside();
		}
	}
}

// mounting poles for ceramic heating element
module ceramheat230(){
	rh = ceramheat230sideh * 2;
	th = 5;
	d = ceramholegapr * 2;
	holegapl = 52;
	translate([-ceramholegapw / 2, -holegapl / 2, 0]){
		mount(5, rh, th, d);
		translate([ceramholegapw, 0, 0]){
			mount(5, rh, th, d);
			translate([0, holegapl, 0]){
				mount(5, rh, th, d);
			}
		}
		translate([0, holegapl, 0]){
			mount(5, rh, th, d);
		}
	}
}

// three small holes for throughhole leads
module throughholes(x, y, h){
	translate([x, y, h / 2]){
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
        }
    }
}

module upcorner(){
    side = 40;
    t = totalxy / 2;
    translate([t - 19, t - 19, totalz - side / 2]){
        rotate([0, 0, 45]){
            cylinder(side * 3, side / 3, side / 2, true, $fn = 5);
        }
    }
}

module upcorners(){
	foursquare(){
		upcorner();
	}
}

module core(){
	br = botrad * sqrt(2);
	tr = toprad * sqrt(2);
	prismoid(size1=[br, br],
		size2=[tr, tr],
		h=totalz - ttopz,
		rounding1 = topround,
		rounding2 = botround);
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

// don't put these immediately opposite the
// ones on the inside, spreading the lock action
// (and keeping printing irregularities from
// spreading)
module croomclip(){
	translate([0, botrad * sqrt(2) / 2 + cclipl / 2, ccliph / 2]){
		translate([20 + cclipw, 0, 0]){
			chamberclip();
		}
		translate([-20 - cclipw, 0, 0]){
			chamberclip();
		}
	}
}

module croomclipinner(){
	translate([0, botrad * sqrt(2) / 2 - 3.5, ccliph / 2]){
		translate([20, 0, 0]){
			chamberclipinverse();
		}
		translate([-20, 0, 0]){
			chamberclipinverse();
		}
	}
}

//https://us.store.bambulab.com/products/pc4-m6-pneumatic-connector-for-ptfe-tube
// PTFE connector (M6 x 3mm, "P4-M6")
// place it near the top, and at an angle (the
// latter to make it easier to print).
module ptfeconn(){
	translate([-spoold / 2 / sqrt(2) - 9, -spoold / 2 / sqrt(2) - 9, totalz / 2]){
		rotate([0, 0, 225]){
			rotate([0, -90, 0]){
				ScrewThread(6, 5, pitch=1,
				            tolerance=0.8);
				ptfer = 4.5 / 2;
				cylinder(25, ptfer, ptfer);
			}
		}
	}
}

module hotbox(){
    difference(){
		difference(){
			core();
			// cut away top corners
			upcorners();
		}
		translate([-42, -40, 0]){
            // rc522holes(wallz);
	    }
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
			translate([0, ceramheat230yoff, 0]){
				ceramheat230();
			}
        }
		// now remove all other interacting pieces
        // 80x80mm worth of air passage
        floorcuts();
        mirror([1, 0, 0]){
            floorcuts();
        }
        // exhaust fan hole
        translate([0, -(totalxy - wallxy) / 2, wallz + hotz / 2]){
            fanhole(wallxy + 16);
        }
        // central column
        translate([0, 0, wallz / 2]){
            cylinder(wallz, platforminnerr, platforminnerr, true);
        }
        // hole and mounts for 175C thermocouple and
		// heating element wires
        translate([50, -15, wallz / 2]){
            rotate([0, 0, 45]){
                thermohole();
            }
        }
		// connectors for the croom
		rotate([0, 0, 90]){
			croomclipinner();
			rotate([0, 0, 90]){
				croomclipinner();
				rotate([0, 0, 90]){
					croomclipinner();
				}
			}
			
		}
		// hole in front left corner for PTFE connector
		ptfeconn();
	}
	translate([50, -botinalt + 25, wallz]){
		rotate([0, 0, 225]){
			idtext();
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
	rotate([0, 0, 90]){
		rotate([0, 0, 90]){
			croomclip();
		}   
	}
}

hotbox();

// testing
/*multicolor("silver"){
	heater();
}

multicolor("green"){
	rotate([0, 0, $t]){
		spool();
	}
}

multicolor("orange"){
    top();
}
*/