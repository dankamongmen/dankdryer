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
columnr = 25;

// we need to hold a spool up to 205mm in diameter and 75mm wide
spoold = 205;
spoolh = 75;
spoolholed = 55;

// we'll want some room around the spool, but the larger our
// chamber, the more heat we lose.
wallz = 2; // bottom thickness; don't want much
gapxy = 1; // gap between spool and walls; spool/walls might expand!
wallxy = 5;
topz = 5; // height of top piece
// spool distance from floor and ceiling. ought add up to 85
// (so that there's space on either side of the fan).
elevation = (85 - spoolh) / 2;
chordxy = 33;
innerr = spoold / 2 + gapxy;
totalxy = spoold + wallxy * 2 + gapxy * 2;
totalz = spoolh + wallz + topz + elevation * 2;
totald = sqrt(totalxy * totalxy + totalxy * totalxy);

ctopz = wallz;
croomz = wallz + ctopz + 80; // 80mm fan; ought just need sin(theta)80

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

ipoints = [[-totalxy / 2 + (chordxy + wallxy), -totalxy / 2 + wallxy],
                [totalxy / 2 - (chordxy + wallxy), -totalxy / 2 + wallxy],
                [totalxy / 2 - wallxy, -totalxy / 2 + (chordxy + wallxy)],
                [totalxy / 2 - wallxy, totalxy / 2 - (chordxy + wallxy)],
                [totalxy / 2 - (chordxy + wallxy), totalxy / 2 - wallxy],
                [-totalxy / 2 + (chordxy + wallxy), totalxy / 2 - wallxy],
                [-totalxy / 2 + wallxy, totalxy / 2 - (chordxy + wallxy)],
                [-totalxy / 2 + wallxy, -totalxy / 2 + (chordxy + wallxy)]];
iipoints = concat(opoints, ipoints);

module topbottom(height){
    linear_extrude(wallz){
        polygon(opoints);
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

acadapterh = 30;
module acadapter(){
    difference(){
        cube([165.1, 60, acadapterh], true);
        translate([0, 0, -15]){
            acadapterscrews(30);
        }
    }
}

loadcellh = 13.5;
loadcelll = 76;
module loadcell(){
    // https://amazon.com/gp/product/B07BGXXHSW
    cube([loadcelll, loadcellh, loadcellh], true);
}

loadcellmountx = -76 / 2 + 21.05 / 2;
loadcellmountw = 13.5;
loadcellmountl = 21.05;
loadcellmounth = 17;
loadcellsupph = 4;

module loadcellmount(baseh){
    difference(){
        // load cell mounting base
        translate([0, 0, baseh / 2]){
            cube([loadcellmountl, loadcellmountw, baseh], true);
        }
        // holes for loadcell screws; need match M5 of load cell
        translate([-5.1, 0, baseh / 2]){
            screw("M5", length = baseh);
        }
        translate([5.1, 0, baseh / 2]){
            screw("M5", length = baseh);
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
motorboxd = 38;
motorshafth = 40; // sans worm: 14
motortheta = -60;
motormounth = 61;
module motor(){
    cylinder(motorboxh, motorboxd / 2, motorboxd / 2, true);
    translate([0, 0, motorboxd]){
        cylinder(motorshafth, 6 / 2, 6 / 2, true);
    }
}

module dropworm(){
    translate([motorboxd - 15, 12, motorboxh / 2 + motorboxd + wormwidth / 2]){
        rotate([0, 0, 90 + motortheta]){
            wormy();
        }
    }
}

// circular mount screwed into the front of the motor through six holes
motorholderh = 3;
module motorholder(){
    d = 31 / 2;
    ch = motorholderh;
    difference(){
        cylinder(ch, motorboxd / 2, motorboxd / 2, true);
        translate([-d, 0, 0]){
            cylinder(ch, 1.5, 1.5, true);
        }
        translate([cos(60) * -d, sin(60) * d, 0]){
            cylinder(ch, 1.5, 1.5, true);
        }
        translate([cos(60) * -d, sin(60) * -d, 0]){
            cylinder(ch, 1.5, 1.5, true);
        }
		translate([d, 0, 0]){
            cylinder(ch, 1.5, 1.5, true);
        }
        translate([cos(60) * d, sin(60) * d, 0]){
            cylinder(ch, 1.5, 1.5, true);
        }
        translate([cos(60) * d, sin(60) * -d, 0]){
            cylinder(ch, 1.5, 1.5, true);
        }
        cylinder(ch, 7, 7, true);
    }
}

mlength = motorboxh + motorholderh;

module lowercoupling(){
	translate([mlength / 2, -30, 0]){
		rotate([90, 0, 0]){
			rotate([0, 90, 0]){
				// the brace comes out to the center
				// of the load cell. the bearing
				// holder rises from the center--the
				// shaft must be in the dead center
				// of the structure.
				bracel = loadcelll / 2 - loadcellmountl;
				translate([-bracel / 2 + 15, 0, loadcellsupph / 2]){
					cube([bracel, loadcellmountw, loadcellsupph], true);
				}
				translate([-bracel / 2, 0, 0]){
					loadcellmount(loadcellsupph);
				}
			}
		}
	}
}

// this partially sheathes the motor, and provides
// the platform. it sits atop the rotor sheathe's
// base, and can thus rotate.
cupolah = 30;
cupolat = 2;
cupolaw = motorboxd + cupolat;
cupolarimh = 2;

module cupolacylinder(){
	cylinder(cupolah, (cupolaw + 2) / 2, (cupolaw + 2) / 2, true);
}

module cupola(){
	difference(){
		cupolacylinder();
		// cut out the core, representing the motor
		translate([0, 0, -cupolarimh / 2]){
			cylinder(cupolah - cupolarimh, (motorboxd + 2) / 2, (motorboxd + 2) / 2, true);
		}
		// cut out a smaller section on the top rim
		translate([0, 0, (cupolah - cupolarimh) / 2]){
			cylinder(cupolarimh, (motorboxd - 8) / 2,
		         (motorboxd - 8) / 2, true);
		}
	}
	bottoml = 4;
	translate([0, 0, -cupolah / 2]){
		rotate_extrude(){
			translate([(cupolaw + 2) / 2, 0, 0]){
				polygon([
					[0, 0], [bottoml, 0], [0, bottoml]
				]);
			}
		}
	}
}

//cupola();
		
// the actual platform should cover a good chunk of area.
// cuts both allow heat to flow, and reduce weight.
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
			intersection(){
				for(i = [0 : 60 : 360]){
					translate([0, 0, (wallz + elevation + platformtoph) / 2]){
						rotate([0, 0, i]){
							cube([5, outr * 2, elevation + wallz + platformtoph], true);
						}
					}
				}
				union(){
					translate([0, 0, wallz + elevation / 2]){
						cylinder(elevation, inr, outr, true);
					}
					translate([0, 0, wallz + elevation + platformtoph / 2]){
						cylinder(platformtoph, outr, outr, true);
					}
				}
			}
		}
		cupolacylinder();
	}
}

//platform(platforminnerr, platformouterd / 2);
	
// platform for the top of the shaft. it expands
// into the hotbox and supports the spool, locking
// down onto the rotor.
// shaft radius is bearingh / 2
// central column radius is columnr
// to calculate the shaft height, add the amount
// in the hotbox to the amount in the croom.
shafth = platformh + 35;
platforminnerr = columnr - 0.5;
platformouterd = spoold / 2;
module shaft(){
    cylinder(shafth, shaftr, shaftr, true);
	/*
    // fatten the shaft so that gear can be pushed
	// only up to this point
	unfath = gearh - bearingh;
	fath = shafth - unfath;
    translate([0, 0, (shafth - fath) / 2]){
        cylinder(fath, shaftr + 2, shaftr + 2, true);
		difference(){
			cylinder(fath, platforminnerr, platforminnerr, true);
			cylinder(fath, platforminnerr - 2, platforminnerr - 2, true);
		}
    }
	*/
    // this should fill most of the central hole, so it can't
    // fall over.
    translate([0, 0, shafth / 2 - platformh]){
        platform(platforminnerr, platformouterd / 2);
    }
}

// the platform sheathes the rotor, then descends to
// the hotbox floor, and expands.
wormbore = 6.8;
wormlen = 15;
wormthick = 2;
rotorh = wormlen + 2;
module rotor(){
    difference(){
        cylinder(wormlen, (wormbore + wormthick) / 2, (wormbore + wormthick) / 2, true);
        cylinder(wormlen, wormbore / 2, wormbore / 2, true);
    }
    // effect the D-shape of the rotor (6.2 vs 6.5)
	ddiff = 0.3;
	ch = rotorh - wormlen;
    translate([(wormbore - ddiff) / 2, 0, -ch / 2]){
        cube([ddiff, wormbore * 0.8, wormlen], true);
    }
	translate([0, 0, -wormlen / 2]){
		difference(){
			cylinder(ch, motorboxd / 2, motorboxd / 2, true);
			cylinder(ch, wormbore / 2, wormbore / 2, true);
		}
	}
}

//rotor();

// put together for testing / visualization, never printed
module assembly(){
	translate([0, 0, loadcellmounth + wallz]){
		translate([0, 0, loadcellh / 2]){
			loadcell();
		}
		translate([0, 0, loadcellh]){
			mirror([1, 0, 0]){
				lowercoupling();
			}
			translate([0, 0, shafth / 2]){
				shaft();
			}
		}
	}
}

// the motor object, rotated and placed as it exists in the croom
module dropmotor(){
    translate([59, -40, wallz + motormounth]){
        rotate([0, 270, motortheta]){
            motor();
        }
    }
}

module spool(){
    translate([0, 0, wallz + elevation]){
        linear_extrude(spoolh){
            difference(){
                circle(spoold / 2);
                circle(spoolholed / 2);
            }
        }
    }
}