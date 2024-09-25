include <gears.scad>
include <BOSL2/std.scad>
include <BOSL2/screws.scad>

$fn = 64;

current_color = "ALL";

module multicolor(color) {
	if (current_color != "ALL" && current_color != color) { 
		// ignore our children.
	} else {
		color(color)
		children();
	}        
}

wormlen = 50;

// we need to hold a spool up to 205mm in diameter and 75mm wide
spoold = 205;
spoolh = 75;
spoolholed = 55;

// we'll want some room around the spool, but the larger our
// chamber, the more heat we lose.
wallz = 3; // bottom thickness; don't want much
gapxy = 1; // gap between spool and walls; spool/walls might expand!
wallxy = 5;
topz = 5; // height of top piece
// spool distance from floor and ceiling. ought add up to 85
// (so that there's space on either side of the fan).
elevation = (85 - spoolh) / 2;
chordxy = 33;

totalxy = spoold + wallxy * 2 + gapxy * 2;
totalz = spoolh + wallz + topz + elevation * 2;
totald = sqrt(totalxy * totalxy + totalxy * totalxy);

ctopz = wallz;
croomz = wallz + ctopz + 90; // 80mm fan; ought just need sin(theta)80

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
    difference(){
        translate([0, h / 2, 0]){
            rotate([90, 0, 0]){
                linear_extrude(h){
                    polygon([
                        [-12.75, 4.25],
                        [4.25, 4.25],
                        [4.25, -12.75]
                    ]);
                }
            }
        }
        rotate([90, 0, 0]){
            // really want 4.3 hrmmm
            screw("M4", length = h);
        }
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
    difference(){
        cube([80, h, 80], true);
        fanmounts(h);
    }
}

// octagon         -- 1mm
// morph to circle -- 3mm
// lips            -- 1.5mm
// 
module top(){
    difference(){
        union(){
            hull(){
                linear_extrude(1){
                    polygon(ipoints);
                }
                translate([0, 0, 1]){
                    linear_extrude(3){
                        circle(totalxy / 2 - 8);
                    }
                }
            }
            translate([0, 0, 4]){
                intersection(){
                    linear_extrude(3){
                        difference(){
                            circle(totalxy / 2 - 6);
                            circle(totalxy / 2 - 8);
                        }
                    }
                    translate([0, 0, 0.75]){
                        cube([totalxy, 30, 1.5], true);
                    }
                }
            }
        }
    }
}

module acadapter(){
    difference(){
        cube([165.1, 60, 30], true);
        translate([0, 0, -15]){
            acadapterscrews(30);
        }
    }
}

loadcellh = 13.5;
module loadcell(){
    // https://amazon.com/gp/product/B07BGXXHSW
    cube([76, loadcellh, loadcellh], true);
}

loadcellmountx = -76 / 2 + 21.05 / 2;
loadcellmountw = 13.5;
loadcellmountl = 21.05;
loadcellmounth = 27;
bearingh = 8;
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

// the upper part of the air shield, which is on the lower coupling
module uppershieldside(){
    rotate([0, 0, 0]){
        //cube([]);
    }
}

module uppershield(){
    uppershieldside();
    mirror([0, 1, 0]){
        uppershieldside();
    }
}

uloadcellmounth = 5;
module lowercoupling(){
    translate([loadcellmountx / 2 - loadcellmountx / 2 - 2, 0, uloadcellmounth / 2]){
        cube([-loadcellmountx, loadcellmountw, uloadcellmounth], true);
    }
    translate([-loadcellmountl / 2 + loadcellmountx / 2 - 2, 0, 0]){
        loadcellmount(uloadcellmounth);
    }
    translate([0, 0, 0]){
        uppershield();
    }
    // recessed area for 22m 608 bearing
    translate([0, 0, uloadcellmounth * 2]){
        // floor and support for bearing
        bearingr = 23 / 2;
        bearwallr = 1;
        cylinder(10, loadcellmountw / 2, bearingr + bearwallr, true);
        translate([0, 0, 2 + bearingh / 2 + 4]){
            difference(){
                cylinder(9, 25 / 2, bearingr + bearwallr, true);
                cylinder(9, 23 / 2, bearingr, true);
            }
        }

    }
}

module drop(){
    translate([0, 0, -croomz]){
        children();
    }
}

teeth = 48;
module gear(){
    worm_gear(modul=1, tooth_number=teeth, thread_starts=2, width=8, length=wormlen, worm_bore=bearingh, gear_bore=bearingh, pressure_angle=20, lead_angle=10, optimized=1, together_built=1, show_spur=1, show_worm=0);
}

module wormy(){
    difference(){
        union(){
            worm_gear(modul=1, tooth_number=teeth, thread_starts=2, width=8, length=wormlen, worm_bore=bearingh, gear_bore=bearingh, pressure_angle=20, lead_angle=10, optimized=1, together_built=1, show_spur=0, show_worm=1);
    
            // fill in the central worm gear hole, save for our rotor cutout
            translate([6, -10.6, 0]){
                cube([6, 10, 6], true);
            }
        }
        translate([6, 12, 0]){
            rotate([90, 0, 0]){
                difference(){
                    cylinder(20, 7 / 2, 7 / 2, true);
                    // effect the D-shape of the rotor (6.5 vs 7)
                    translate([3.25, -3, 0]){
                        cube([0.5, 10, 20], true);
                    }
                }
            }
        }
    }
}

// motor is 37x75mm diameter gearbox and 6x14mm shaft
// (with arbitrarily large worm gear on the shaft)
motorboxh = 75;
motorboxd = 37;
motorshafth = wormlen; // sans worm: 14
motorshaftd = 13; // sans worm: 6
motortheta = -60;
motormounth = 37;
// the worm gear on the motor's rotor needs to be tangent to, and at the same
// elevation as, some point on the central gear.
module motor(){
    cylinder(motorboxh, motorboxd / 2, motorboxd / 2, true);
    translate([0, 0, motorboxd]){
        cylinder(motorshafth, 6 / 2, 6 / 2, true);
    }
}

// the motor object, rotated and placed as it exists in the croom
module dropmotor(){
    translate([59, -40, wallz + motormounth * 2]){
        rotate([0, 270, motortheta]){
            motor();
        }
    }
}

module dropworm(){
    translate([motorboxd - 13, 12, motorboxh + 2]){
        rotate([0, 0, 90 + motortheta]){
            wormy();
        }
    }
}

// circular mount screwed into the front of the motor through six holes
motorholderh = 3;
module motorholder(){
    d = (28.75 + 3) / 2;
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

// mount for motor. runs horizontal approximately a gear
// radius away from the center.
module motormount(){
    mlength = motorboxh + motorholderh;
    difference(){
        cube([mlength, motorboxd, motormounth * 2], true);
        // now cut a cylinder out of its top
        translate([0, 0, motormounth]){
            rotate([0, 90, 0]){
                cylinder(mlength, motorboxd / 2, motorboxd / 2, true);
            }
        }
    }
    // circular front mount. without the fudge factor, openscad
    // considers the manifold broken, which i don't understand
    // FIXME until then leave it in
    translate([-37.5, 0, motormounth - 0.001]){
        rotate([0, 90, 0]){
            motorholder();
        }
    }
}

module dropmotormount(){
    translate([58, -39, wallz + motormounth]){
        rotate([0, 0, motortheta]){
            motormount();
        }
    }
}

shafth = bearingh + croomz - loadcellmounth;

module shaft(){
    cylinder(shafth, bearingh / 2, bearingh / 2, true);
}

module dogear(){
    translate([teeth / 2, 0, 0]){
        gear();
    }
}

module dropgear(){
    translate([0, 0, wallz + motormounth * 2]){
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