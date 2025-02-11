include <BOSL2/std.scad>
include <BOSL2/screws.scad>

$fn = 128;

current_color = "ALL";

module multicolor(color) {
	if (current_color != "ALL" && current_color != color) {
		// ignore our children.
	} else {
		color(color)
		children();
	}
}

// we need to hold a spool up to 205mm in diameter and 75mm wide
spoold = 205;
spoolh = 75;
spoolholed = 55;

// we'll want some room around the spool,
// but the larger our chamber, the more heat lost.
wallz = 1.8; // bottom thickness; don't want much
gapxy = 1; // gap between spool and walls; spool/walls might expand!
wallxy = 5;
topz = 5; // height of top piece
// spool distance from floor and ceiling.
elevation = (85 - spoolh) / 2;
chordxy = 33;
innerr = spoold / 2 + gapxy;
totalxy = spoold + wallxy * 2 + gapxy * 2;
totalz = spoolh + wallz + topz + elevation * 2;
totald = sqrt(totalxy * totalxy + totalxy * totalxy);

ctopz = wallz;
croomz = wallz + ctopz + 80; // 80mm fan; ought just need sin(theta)80

// thickness of croom xy walls (bottom is wallz)
croomwall = 3;
// the outer radii on our top and bottom
botrad = totalxy * sqrt(2) / 2;
toprad = totalxy * sqrt(2) / 2;
// the inner radii on its top and bottom
botinrad = botrad - croomwall * sqrt(2);
topinrad = toprad - croomwall * sqrt(2);
// distances from center to mid-outer wall
topalt = toprad / sqrt(2);
botalt = botrad / sqrt(2);
topinalt = topinrad / sqrt(2);
botinalt = botinrad / sqrt(2);
botround = 70;
topround = 30;
adj = (botrad - toprad) / sqrt(2);
theta = (90 - atan(-croomz / adj));

flr = -croomz + wallz; // floor z offset
mh = wallz; // mount height
shieldw = 88;

opoints = [
            [-totalxy / 2 + chordxy, -totalxy / 2],
            [totalxy / 2 - chordxy, -totalxy / 2],
            [totalxy / 2, -totalxy / 2 + chordxy],
            [totalxy / 2, totalxy / 2 - chordxy],
            [totalxy / 2 - chordxy, totalxy / 2],
            [-totalxy / 2 + chordxy, totalxy / 2],
            [-totalxy / 2, totalxy / 2 - chordxy],
            [-totalxy / 2, -totalxy / 2 + chordxy]
        ];

module topbottom(height){
    linear_extrude(wallz){
        polygon(opoints);
    }
}

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

// spacing between the 4.3mm diameter holes is 71.5mm, so
// 8.5 / 2 == 4.25 from hole center to side. triangle with
// bases 2 * 8.5 == 17
module fanmountur(h){
    rotate([90, 0, 0]){
        // really want 4.3 hrmmm
        screw("M4", length = h);
    }
}

module fanmountsider(h, d){
    translate([0, 0, d]){
        fanmountur(h);
    }
    mirror([0, 0, 1]){
        translate([0, 0, d]){
            fanmountur(h);
        }
    }
}

module fanmounts(h){
    r = 4.3;
    dist = 71.5;
    translate([dist / 2, 0, 0]){
        fanmountsider(h, dist / 2);
    }
    mirror([1, 0, 0]){
        translate([dist / 2, 0, 0]){
            fanmountsider(h, dist / 2);
        }
    }
}

module fanhole(h){
    rotate([90, 0, 0]){
        cylinder(h, 80 / 2, 80 / 2, true);
    }
    fanmounts(2 * h / 3);
}

loadcellh = 12.7;
//loadcelll = 76;
loadcelll = 80;
module loadcell(){
    // https://amazon.com/gp/product/B07BGXXHSW
    cube([loadcelll, loadcellh, loadcellh], true);
}

// there is 5mm from each side to the center of
// hole closest to it, and 15mm between the centers
// of holes on a side. there are 40mm between the
// centers of the two inner holes. 80mm total.
loadcellmountholegap = 15;
loadcellmountholeside = 5;
loadcellmountholecgap = 40;
loadcellmountl = loadcellmountholeside +
                 loadcellmountholegap +
				 loadcellmountholecgap / 4;
loadcellmounth = 17;
loadcellsupph = 4;
loadcellmountw = loadcellh;

module loadcellmount(baseh, s = "M5"){
    difference(){
        // load cell mounting base
        translate([0, 0, baseh / 2]){
            cube([loadcellmountl, loadcellh, baseh], true);
        }
        translate([-loadcellmountholegap / 2, 0, baseh / 2]){
            screw(s, length = baseh);
        }
        translate([loadcellmountholegap / 2, 0, baseh / 2]){
            screw(s, length = baseh);
        }
    }
}

// height of inverse cone supporting bearing holder
couplingh = 10;
bearingh = 9; // height ("width") of bearing
bearingr = 30 / 2; // bearing outer radius
bearinginnerr = 10 / 2; // bearing inner radius
bearingwall = 2;
shaftr = bearinginnerr - 1;

module drop(){
    translate([0, 0, -croomz]){
        children();
    }
}

// motor is 37x75mm diameter gearbox and 6x14mm shaft
motorboxh = 70;
motorboxd = 37;
motorshafth = 40; // sans worm: 14
motortheta = -60;
motormounth = 61;
module motor(){
    cylinder(motorboxh, motorboxd / 2, motorboxd / 2, true);
    translate([0, 0, motorboxd]){
        cylinder(motorshafth, 6 / 2, 6 / 2, true);
    }
}

motorholderh = 3;
mlength = motorboxh + motorholderh;

tril = loadcelll / 2;
module lowercouplingtri(l){
	linear_extrude(loadcellsupph){
		polygon([
			[-l + tril, loadcellmountw / 2],
			[-l + tril, (motorboxd - loadcellsupph) / 2],
			[-l, loadcellmountw / 2]
		]);
	}
}

module lowercoupling(){
	// the brace comes out to the center
	// of the load cell. the bearing
	// holder rises from the center--the
	// shaft must be in the dead center
	// of the structure.
	outerr = (motorboxd + 2) / 2;
	innerr = motorboxd / 2;
	// total length of the lower coupling is
	// one side of the load cell through the
	// opposite outside of the coupling, aka:
	couplingl = loadcelll / 2 + outerr;
	difference(){
		union(){
			bracel = couplingl / 2 - loadcellmountl;
			translate([-loadcellmountl / 2, 0, 0]){
				loadcellmount(loadcellsupph, "M4");
			}
			lowercouplingtri(couplingl / 2);
			mirror([0, 1, 0]){
				lowercouplingtri(couplingl / 2);
			}
			couplingh = 40;
			// primary motor holder
			translate([couplingl / 2 - outerr, 0, 0]){
				difference(){
					translate([0, 0, couplingh / 2 + loadcellsupph]){
						cylinder(couplingh, outerr, outerr, true);
					}
					translate([0, 0, loadcellsupph + couplingh / 2]){
						cylinder(couplingh, innerr, innerr, true);
					}
				}
				cylinder(loadcellsupph,
						(motorboxd - loadcellsupph) / 2,
						outerr);
			}
		}
		// holes in the bottom for wires, polarity
		// legend, and center for load cell bump
		translate([couplingl / 2 - outerr, 0, loadcellsupph / 2]){
			cube([innerr, loadcellh, loadcellsupph], true);
		}
		translate([couplingl / 2 - outerr, innerr - 7, loadcellsupph / 2]){
			cylinder(loadcellsupph, 3, 3, true);
		}
		translate([couplingl / 2 - outerr, -innerr + 7, loadcellsupph / 2]){
			cylinder(loadcellsupph, 3, 3, true);
		}
		translate([couplingl / 2 - outerr - 3, -13, 0]){
			rotate([0, 0, 90]){
				linear_extrude(loadcellsupph){
					text("+", size=7, font="Prosto One");
				}
			}
		}
		// "redrill" the inner screw so that it
		// penetrates the cylinders. screw from below.
		translate([-couplingl / 2 + 22.25, 0, loadcellsupph / 2]){
            screw("M4", length = loadcellsupph);
        }	
	}
}
/*
bracel = loadcelll / 2 - loadcellmountl;
translate([-bracel + 3, 0, 0]){
	loadcellmount(loadcellsupph);
}*/
//lowercoupling();

// the actual platform should cover a good chunk
// of area, to keep the spool steady. cuts both
// allow heat to flow, and reduce weight.
platformtoph = 2;
platformh = elevation + wallz + platformtoph;
module platform(inr, outr){
	difference(){
		union(){
			difference(){
				union(){
					translate([0, 0, wallz / 2]){
						cylinder(wallz, inr, inr, true);
					}
					// platform blossoms out to full width
					translate([0, 0, wallz + elevation / 2]){
						cylinder(elevation, inr, outr, true);
					}
					// now a fixed-radius plate at the top
					translate([0, 0, wallz + elevation + platformtoph / 2]){
						cylinder(platformtoph, outr, outr, true);
					}
				}	
				// cut rings out from the platform as stands
				translate([0, 0, platformh / 2]){
					step = outr / 5;
					istep = inr / 2;
					for(i = [0 : istep : inr]){
						difference(){
							cylinder(platformh, i + step - 2, i + step - 2, true);
							cylinder(platformh, i, i, true);
						}
					}
					ostep = (outr - inr) / 3;
					for(i = [inr : ostep : outr]){
						difference(){
							cylinder(elevation, i + step - 2, i + step - 2, true);
							cylinder(elevation, i, i, true);
						}
					}
				}
			}
			// add the six spokes
			intersection(){
				// chunky spokes
				for(i = [0 : 60 : 360]){
					translate([0, 0, (wallz + elevation + platformtoph) / 2]){
						rotate([0, 0, i]){
							cube([5, outr * 2, elevation + wallz + platformtoph], true);
						}
					}
				} // cylinder to thin spokes
				translate([0, 0, wallz + elevation / 2]){
					cylinder(elevation, inr, outr, true);
				}
			}
		}
		cylinder(cupolah, cupolaw / 2, cupolaw / 2, true);
	}
}

shafth = platformh + 35;
platformouterd = spoold / 2;

// the platform sheathes the rotor, then descends to
// the hotbox floor, and expands.
cupolah = 24;
cupolat = 4;
cupolaw = motorboxd + cupolat;
columnr = 25;
platforminnerr = columnr - 0.5;
wormbore = 6.5; // taken from specsheet + 0.5 skoosh
wormlen = 15;
wormthick = 2;
bottoml = 4;
ch = (cupolaw - motorboxd) / 2;
rotorh = cupolah + ch;
module rotor(){
	translate([0, 0, -1]){ // z-center it properly
		difference(){
			cylinder(cupolah, cupolaw / 2, cupolaw / 2, true);
			// cut out the core, representing the motor
			// plus a small amount of skoosh (radius)
			innersk = (motorboxd + 1) / 2;
			cylinder(cupolah, innersk, innersk, true);
		}
			
		// lower rim, with triangular supports
		translate([0, 0, -cupolah / 2]){
			rotate_extrude(){
				translate([cupolaw / 2, 0, 0]){
					polygon([
						[0, 0], [bottoml, 0], [0, bottoml]
					]);
				}
			}
		}
		// motor-wide cylinder transfixed by the rotor.
		// it is this to which torque is imparted.
		translate([0, 0, (cupolah + ch) / 2]){
			translate([0, 0, -ch / 2]){
				rotate_extrude(){
					translate([motorboxd / 2, 0, 0]){
						polygon([
							[0, 0], [ch, 0], [0, ch]
						]);
					}
				}
			}
			r = motorboxd / 2;
			difference(){
				cylinder(ch, r, r, true);
				cylinder(ch, wormbore / 2, wormbore / 2, true);
			}
			// effect the D-shape of the rotor (6.2 vs 6.5)
			ddiff = 0.6;
			translate([(wormbore - ddiff) / 2, 0, 0]){
				cube([ddiff, wormbore * 0.8, ch], true);
			}
		}
	}
}

//rotor();
//cube([1, 1, rotorh], true);

// testing / visualization, never printed
module assembly(){
	translate([0, 0, loadcellmounth + wallz]){
		translate([0, 0, loadcellh / 2]){
			loadcell();
		}
		translate([-14.5, 0, loadcellh]){
			mirror([0, 1, 0]){
				lowercoupling();
			}
			translate([15, 0, platformh / 2 + cupolah + loadcellh + 30]){
			    platform(platforminnerr, platformouterd / 2);
			}
		}
		translate([0, 0, motorboxh]){
			motor();
			translate([0, 0, 25]){
				rotor();
			}
		}
	}
}

//assembly();

module spoolcut(){
	polygon([
		[10, 40],
		[30, 80],
		[-30, 80],
		[-10, 40]
	]);
	rotate([0, 0, -60])
	for(y = [1 : 1 : 11]){
		for(x = [-y / 2: 1 : y / 2]){
			translate([x * 6, 25 + y * 6, 0]){
				circle(3);
			}
		}
	}
}

// do a single layer of filament all the way through
module fullspoollayer(h, fild){
	for(d = [spoolholed : 3.5 : fild]){
		translate([0, 0, h]){
			rotate_extrude(){
				translate([d / 2, 0, 0]){
					circle(1.75 / 2);
				}
			}
		}
	}
}

module tracespoollayer(h, fild){
	translate([0, 0, h]){
		rotate_extrude(){
			translate([fild / 2, 0, 0]){
				circle(1.75 / 2);
			}
			translate([spoolholed / 2, 0, 0]){
				circle(1.75 / 2);
			}
		}
	}
}

module spool(){
	fild = spoolholed + 1.75 * 55;
	filh = 1.75 * 35;
	reelh = (spoolh - filh) / 2;
    translate([0, 0, wallz + elevation + platformtoph + 5]){
		color([1, 1, 1]){
			difference(){
				linear_extrude(spoolh){
					difference(){
						circle(spoold / 2);
						circle(spoolholed / 2);
						spoolcut();
						rotate([0, 0, 120]){
							spoolcut();
						}
						rotate([0, 0, 240]){
							spoolcut();
						}
					}
				}
				translate([0, 0, reelh]){
					linear_extrude(spoolh - reelh * 2){
						circle(spoold);
					}
				}
			}
		}
		color("indigo"){
			fullspoollayer(reelh + 1.75 / 2, fild);
			for(h = [reelh + 3 * 1.75 / 2 : 1.75 : spoolh - reelh]){
				tracespoollayer(h, fild);
			}
			fullspoollayer(spoolh - reelh - 1.75 / 2, fild);
		}
	}
}

//spool();
