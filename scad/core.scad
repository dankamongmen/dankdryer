include <BOSL2/std.scad>
include <BOSL2/screws.scad>
use <threads.scad>

$fn = 128;

current_color = "ALL";

module multicolor(color, opacity=1) {
	//if (current_color != "ALL" && current_color != color) {
		color(color, opacity){
			children();
		}
	/*} else {
		// ignore
	}*/
}

module idtext(){
  text3d("v2.8.4", h=1.2, size=6);
}

// we need to hold a spool up to 205mm in diameter and 75mm wide
spoold = 205;
spoolh = 75;
// 80mm fan; use sin(theta)80 if we're sloped.
// 81 is a multiple of both 0.3 and 0.2.
hotz = 81;
spoolholed = 55;
tpitch = 1.8; // multiple of 0.3 and 0.2
// we'll want some room around the spool,
// but the larger our chamber, the more heat lost.
wallz = 1.8; // bottom thickness; don't want much
gapxy = 1; // gap between spool and walls; spool/walls might expand!
wallxy = 5;
// height of threaded, mated section at the top
// (and probably soon at the middle FIXME).
// ensure it's a multiple of both 0.2 and 0.3 to
// support both .4mm and .6mm nozzle layer heights.
ttopz = 0.2 * 0.3 * 100; // 6mm
// spool distance from floor and ceiling.
elevation = (hotz - spoolh) / 2;
chordxy = 33;
innerr = spoold / 2 + gapxy;
totalxy = spoold + wallxy * 2 + gapxy * 2;

ctopz = wallz;
croomz = wallz + ctopz + hotz;
totalz = hotz + wallz + ttopz + elevation;
locknuth = 5; // locknuts go atop mounting rods

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
botround = 90;
topround = 70;
adj = (botrad - toprad) / sqrt(2);
theta = (90 - atan(-croomz / adj));

flr = -croomz + wallz; // floor z offset
mh = wallz; // mount height
shieldw = 88;

module topbottom(height){
    linear_extrude(wallz){
        polygon(opoints);
    }
}

// cXcX3mm cube bottom plus (rh + th) d-wide riser
// rh is smooth length, th is threaded length
module mount(c, rh, th, d){
	ct = 3;
	translate([0, 0, ct / 2]){
		cube([c, c, ct], true);
		translate([0, 0, ct / 2]){
			RodStart(d, rh, th,
			         thread_diam=d - 0.5,
					 thread_pitch=0);
		}
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

module rot(deg){
    rotate([theta + deg, 0, 0]){
        children();
    }
}

// spacing between the 4.3mm diameter holes is
// 71.5mm, so 8.5 / 2 == 4.25 from hole center
// to side. triangle with bases 2 * 8.5 == 17
module fanmountur(h){
    rotate([90, 0, 0]){
        // really want 4.3 but 4.5 is a multiple of
		// 0.3, and these will be horizontal
		// FIXME
		//ScrewThread(4.5, h, pitch=3);
		cylinder(h, 4.5 / 2, 4.5 / 2, true);
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

//fanmounts(9);

module fanhole(h){
	fanmm = 80 - 5;
    rotate([90, 0, 0]){
        cylinder(h, fanmm / 2, fanmm / 2, true);
    }
    fanmounts(h);
}

//fanhole(9);

loadcellh = 12.7;
//loadcelll = 76;
loadcelll = 80;
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
// we need the load-side holes of the load cell
// to be centered around the chamber center.
// the load-side center is at (loadcellmountholegap
// plus loadcellmountholeside) / 2.
loadcelloffx =
 (loadcellmountholegap + loadcellmountholeside) / 2
 - (loadcelll / 2);
module loadcell(){
    // https://amazon.com/gp/product/B07BGXXHSW
    cube([loadcelll, loadcellh, loadcellh], true);
}

// load cell mounting base with two threads
module loadcellmount(baseh){
    difference(){
		translate([0, 0, baseh / 2]){
			diff(){
				cuboid([loadcellmountl + 2,
						loadcellh + 2, baseh], 
						rounding=-5, edges=BOT)
					edge_profile(TOP)
						mask2d_roundover(r=2);
			}
        }	
        translate([-loadcellmountholegap / 2, 0, 0]){
			ScrewThread(5, baseh);
        }
        translate([loadcellmountholegap / 2, 0, 0]){
			ScrewThread(5, baseh);
        }
    }
}

//loadcellmount(loadcellmounth);

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

module lowercoupling(){
	// the center must be in the dead center
	// of the structure.
	outerr = (motorboxd + 2) / 2;
	// we add 0.5mm for the polycarbonate (1 for
	// CF/GF), as it has no flex (0.25 to innerr)
	innerr = motorboxd / 2 + 0.25;
	// hole in middle to accommodate 10mm shaft
	holer = 10 / 2;
	boltheadh = 4;
	boltheadr = 3.5;
	th = loadcellsupph + boltheadh;
	couplingh = 40;
	// primary motor holder
	difference(){
		translate([0, 0, couplingh / 2 + loadcellsupph]){
			cylinder(couplingh, outerr, outerr, true);
		}
		translate([0, 0, loadcellsupph + couplingh / 2]){
			cylinder(couplingh, innerr, innerr, true);
		}
	}
	// fill in the bottom
	difference(){
		union(){
			cylinder(loadcellsupph,
					(motorboxd - loadcellsupph) / 2,
					outerr);
			translate([0, 0, loadcellsupph]){
				cylinder(boltheadh, outerr, outerr);
			}
		}
		cylinder(th, holer, holer);
		translate([0, 0, loadcellsupph + boltheadh / 2]){	
			// two M4 screws couple to the
			// load cell, but only the heads
			// remain
			// printing polycarbonate, which
			// tends to shrink). we then need
			// embed the 7x4 head in the floor
			translate([-loadcellmountholegap / 2, 0, loadcellsupph / 2]){
				rotate([180, 0, 0]){
					RodStart(boltheadr * 2,
							 boltheadh,
							 thread_len=loadcellsupph,
							 thread_diam=5);
				}
			}
			translate([loadcellmountholegap / 2, 0, loadcellsupph / 2]){
				rotate([180, 0, 0]){
					RodStart(boltheadr * 2,
							 boltheadh,
							 thread_len=loadcellsupph,
							 thread_diam=5);
				}
			}
		}
		// holes in the bottom for wires, polarity
		// legend, and also boltholes
		translate([0, 0, th / 2]){
			translate([0, innerr - 7, 0]){
				cylinder(th, 3, 3, true);
			}
			translate([0, -innerr + 7, 0]){
				cylinder(th, 3, 3, true);
			}
		}
		// designate positive side
		translate([4, -15, 0]){
			linear_extrude(th){
				text("+", size=7, font="Prosto One");
			}
		}
	}
}

/*bracel = loadcelll / 2 - loadcellmountl;
translate([-bracel + 3, 0, 0]){
	loadcellmount(loadcellsupph + 1);
}*/
//lowercoupling();

// the actual platform should cover a good chunk
// of area, to keep the spool steady. cuts both
// allow heat to flow, and reduce weight.
platformtoph = 2;
platformh = elevation + wallz + platformtoph;
module platform(inr){
	outr = platformouterd / 2;
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
					rotate([0, 0, i]){
					rotate_extrude(angle = 10){
						polygon([
							[inr, 0],
							[inr, platformh - 2],
							[outr, platformh - 2]
						]);
						polygon([
							[inr, 0],
							[inr, platformh],
							[outr, platformh]
						]);
					}
					}
				}
				
				/*	translate([0, 0, (wallz + elevation + platformtoph) / 2]){
						rotate([0, 0, i]){
							cube([5, outr * 2, elevation + wallz + platformtoph], true);
						}
					}
				// cylinder to thin spokes
				translate([0, 0, platformtoph + wallz + 3]){
					cylinder(elevation + wallz + platformtoph, outr - 20, outr, true);
				}*/
			}
		}
		cylinder(cupolah, cupolaw / 2 + 1, cupolaw / 2 + 1, true);
	}
}

//platform(platforminnerr);
shafth = platformh + 35;
platformouterd = spoold / 2;

// the platform sheathes the rotor, then descends to
// the hotbox floor, and expands.
cupolah = 24;
cupolaw = motorboxd + 4;
platforminnerr = 25;
wormbore = 6.8; // taken from specsheet + 0.8 skoosh
bottoml = 4;
ch = (cupolaw - motorboxd) / 2;
rotorh = cupolah + ch;
module rotor(){
	translate([0, 0, -1]){ // z-center it properly
		// main core
		difference(){
			cylinder(cupolah, cupolaw / 2, cupolaw / 2, true);
			// cut out the core, representing the motor
			// plus a small amount of skoosh (radius)
			innersk = (motorboxd + 1) / 2 + 0.5;
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
			// bend inwards at the top
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
			// inner core of the top
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
			translate([loadcelloffx, 0, 0]){
				loadcell();
			}
		}
		translate([0, 0, loadcellh]){
			mirror([0, 1, 0]){
				lowercoupling();
			}
			translate([0, 0, motorboxh / 2 + loadcellsupph]){
				motor();
				translate([0, 0, 25]){
					rotor();
					translate([0, 0, -10]){
						platform(platforminnerr);
					}
				}
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
    translate([0, 0, croomz + elevation + platformtoph + 4]){
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
//assembly();

cclipr = croomwall; // inner size of clip (hole)
ccliph = cclipr;
cclipw = 10;
cclipl = cclipr + 2; // 2 is size of clip wall
// a stud which mates into a chamber clip
module clipplug(w){
	// fits into the clip
	translate([0, 0, ccliph / 2]){
		cube([w, cclipr, ccliph], true);
	}
	translate([-w / 2, -cclipr / 2, 0]){
		rotate([0, 90, 0]){
			linear_extrude(w){
				polygon([
					[0, 0],
					[3 * cclipr / 2, 0],
					[0, ccliph]
				]);
			}
		}
	}
}

module chamberinnerplug(){
	clipplug(cclipw - 2);
}

// ought be included as a difference()
// meant to be internal. better in terms
// of both support and aesthetics than
// external chamberclip().
// slightly wider than the chamberclip, because
// we can be.
// skoosh = 0.2 is good for GF
// skoosh = 0.5 is more up to speed for CF
module chamberclipinverse(skoosh = 0.5){
	cube([cclipw - 2 + skoosh, cclipr + skoosh, ccliph], true);
}

// interior, for mating with chamberclipinverse()
module chamberpluginverse(){
	mirror([0, 1, 0]){
		chamberplug();
	}
}

/*translate([0, cclipl / 2, ccliph / 2]){
	chamberclip();
}*/
/*chamberplug();
chamberpluginverse();*/
