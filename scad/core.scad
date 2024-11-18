include <gears.scad>
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
wormlen = 40;

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
module lowercoupling(){
    // the brace comes out to the center of the load cell.
    // the bearing holder rises from the center--the shaft
    // must be in the dead center of the structure.
    bracel = loadcelll / 2 - loadcellmountl;
    translate([-bracel / 2, 0, loadcellsupph / 2]){
        cube([bracel, loadcellmountw, loadcellsupph], true);
    }
    translate([-bracel - loadcellmountl / 2, 0, 0]){
        loadcellmount(loadcellsupph);
    }
    // recessed area for 30mm 6200-2RS bearing
    translate([0, 0, couplingh / 2]){
        coupr = bearingr + 0.2; // a bit of skoosh
        // floor and support for bearing
        bearwallr = 2;
        // go from slab width to bearing diameter
        cylinder(couplingh, loadcellmountw / 2, coupr + bearwallr, true);
        translate([0, 0, couplingh / 2 + bearingh / 2]){
            difference(){
                cylinder(bearingh, coupr + bearwallr, coupr + bearwallr, true);
                cylinder(bearingh, coupr, coupr, true);
            }
        }
    }
}

module drop(){
    translate([0, 0, -croomz]){
        children();
    }
}

// 60 was too big, as was 55 at 20 height
// now try 53 and 17
// 53 was too small; try 54
teeth = 54;
gearboth = 8; // width of gear (height in our context)
// fat cylinder on top so the bearing can be pushed up all the way
// remaining height ought be defined in terms
// of the motor and coupling FIXME.
gearh = gearboth + 12;
gearbore = bearingh + 0.4;
wormbore = 7;
module gear(){
    translate([teeth, 0, -gearh / 2]){
    worm_gear(modul=1, tooth_number=teeth, thread_starts=2,
                width=gearboth, length=wormlen, worm_bore=wormbore,
                gear_bore=gearbore, pressure_angle=20,
                lead_angle=10, optimized=1, together_built=0,
                show_spur=1, show_worm=0);
    }
    // cylinder plugs into bearing and encloses
    // top of shaft, so outside radius is bearing
    // inner radius, and inside radius is shaft
    // radius. this should only be as tall as the
    // bearing itself, and locked off beneath.
	bigh = gearh - gearboth - bearingh;
    translate([0, 0, -gearh / 2 + gearboth]){
        // this should be bigger than the bearing
        translate([0, 0, bigh / 2]){
          difference(){
            cylinder(bigh, bearinginnerr + bearingwall, bearinginnerr + bearingwall, true);
            cylinder(bigh, shaftr, shaftr, true);
          }
        }
        translate([0, 0, bigh + bearingh / 2]){
            difference(){
                cylinder(bearingh, bearinginnerr, bearinginnerr, true);
                cylinder(bearingh, shaftr, shaftr, true);
            }
        }
    }
}

wormwidth = 8;
module wormy(){
translate([0, -0.75, 0]){
    worm_gear(modul=1, tooth_number=teeth, thread_starts=2, width=wormwidth, length=wormlen, worm_bore=wormbore, gear_bore=gearbore, pressure_angle=20, lead_angle=10, optimized=1, together_built=1, show_spur=0, show_worm=1);
}
    // effect the D-shape of the rotor (6.5 vs 7)
    translate([(wormbore - 2) / 2, 0, 0]){
        cube([0.5, wormlen, wormbore], true);
    }
}

// motor is 37x75mm diameter gearbox and 6x14mm shaft
// (with arbitrarily large worm gear on the shaft)
motorboxh = 70;
motorboxd = 38;
motorshafth = wormlen; // sans worm: 14
motorshaftd = 13; // sans worm: 6
motortheta = -60;
motormounth = 61;
// the worm gear on the motor's rotor needs to be tangent to, and at the same
// elevation as, some point on the central gear.
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
        translate([d, 0, 0]){
            cylinder(ch, 1.5, 1.5, true);
        }
        translate([-d, 0, 0]){
            cylinder(ch, 1.5, 1.5, true);
        }
        translate([cos(60) * -d, sin(60) * d, 0]){
            cylinder(ch, 1.5, 1.5, true);
        }
        translate([cos(60) * d, sin(60) * d, 0]){
            cylinder(ch, 1.5, 1.5, true);
        }
        translate([cos(60) * d, sin(60) * -d, 0]){
            cylinder(ch, 1.5, 1.5, true);
        }
        translate([cos(60) * -d, sin(60) * -d, 0]){
            cylinder(ch, 1.5, 1.5, true);
        }
        cylinder(ch, 7, 7, true);
    }
}

// mount for motor. runs horizontal approximately
// a gear radius away from the center.
module motormount(){
    mlength = motorboxh + motorholderh;
    // the bottom is in a 'v' shape to save material.
	translate([-mlength / 2, 0, 0]){
        rotate([90, 0, 90]){
            linear_extrude(mlength){
				h = motormounth - motorboxd / 2;
				sh = motormounth - 11 * motorboxd / 20;
				s2h = sh - (motorboxd / 8);
                polygon([[-motorboxd / 2, 0],
						 [-5 * motorboxd / 12, 0],
						 [-motorboxd / 3, s2h],
						 [0, sh],
						 [motorboxd / 3, s2h],
						 [5 * motorboxd / 12, 0],
                         [motorboxd / 2, 0],
                         [motorboxd / 2, h],
                         [-motorboxd / 2, h]]);
            }
        }
    }
    translate([0, 0, motormounth]){
        difference(){
            translate([0, 0, -motorboxd / 4]){
                cube([mlength, motorboxd, motorboxd / 2], true);
            }
            // now cut a cylinder out of its top
            rotate([0, 90, 0]){
                cylinder(mlength, motorboxd / 2, motorboxd / 2, true);
            }
        }
        // circular front mount.
        translate([-35, 0, 0]){
            rotate([0, 90, 0]){
                motorholder();
            }
        }

    }
}

module dropmotormount(){
    translate([60, -41, wallz]){
        rotate([0, 0, motortheta]){
            motormount();
        }
    }
}

// the actual platform should cover a good chunk of area.
// cuts both allow heat to flow, and reduce weight.
platformtoph = 2;
platformh = elevation + wallz + platformtoph;
module platform(inr, outr){
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

// platform for the top of the shaft. it expands into the
// hotbox and supports the spool. locks down onto top of shaft.
// shaft radius is bearingh / 2
// central column radius is columnr
// to calculate the shaft height, we add the amount in the hotbox
// to the amount in the croom.
shafth = platformh + 35;
module shaft(){
    platforminnerr = columnr - 0.5;
    platformouterd = spoold / 2;
    cylinder(shafth, shaftr, shaftr, true);
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
    // this should fill most of the central hole, so it can't
    // fall over.
    translate([0, 0, shafth / 2 - platformh]){
        platform(platforminnerr, platformouterd / 2);
    }
}

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
			translate([0, 0, couplingh + bearingh]){
				translate([0, 0, 4 + gearh / 2]){
					gear();
				}
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

module dropgear(){
    translate([0, 0, wallz + motormounth]){
        dogear();
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
