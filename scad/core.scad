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
gapz = 2; // spool distance from bottom of top
elevation = 10; // spool distance from bottom
chordxy = 33;

totalxy = spoold + wallxy * 2 + gapxy * 2;
totalz = spoolh + wallz + topz + gapz + elevation;
totald = sqrt(totalxy * totalxy + totalxy * totalxy);

ctopz = wallz;
croomz = wallz + ctopz + 90; // 80mm fan; ought just need sin(theta)80

mh = wallz; // mount height

shieldw = 88;
module shieldscrew(){
    translate([(-shieldw + 6) / 2, (19.5 + 6) / 2, mh / 2]){
        screw("M4", length = mh);
    }
}

module shieldbinder(){
    difference(){
        translate([(-shieldw + 6) / 2, (19.5 + 6) / 2, mh / 2]){
            cube([6, 6, mh], true);
        }
        shieldscrew();
    }
}

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

// screw hole for the top hold
module holdhole(h){
    translate([-10, -10, h / 2]){
        cylinder(h, 4 / 2, 4 / 2, true);
    }
}

module holdholes(h){
    holdhole(h);
    mirror([0, 1, 0]){
        holdhole(h);
    }
    mirror([1, 0, 0]){
        holdhole(h);
    }
    mirror([1, 1, 0]){
        holdhole(h);
    }
}

module top(){
    difference(){
        hull(){
            topbottom(1);
            translate([0, 0, topz]){
                linear_extrude(1){
                    polygon(ipoints);
                }
            }
        }
        holdholes(6);
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
    translate([0, 0, motorboxh]){
        cylinder(motorshafth, motorshaftd / 2, motorshaftd / 2, true);
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

module loadcellmount(baseh){
    difference(){
        // load cell mounting base
        translate([loadcellmountx, 0, baseh / 2]){
            cube([loadcellmountl, loadcellmountw, baseh], true);
        }
        // holes for loadcell screws
        translate([-76 / 2 + 5.425, 0, baseh / 2]){
            screw("M4", length = baseh);
        }
        translate([-76 / 2 + 15.625, 0, baseh / 2]){
            screw("M4", length = baseh);
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

module lowercoupling(){
    loadcellmount(10);
    translate([loadcellmountx / 2, 0, 15]){
        cube([-loadcellmountx + loadcellmountl, loadcellmountw, 10], true);
    }
    translate([0, 0, 0]){
        uppershield();
    }
    // recessed area for 608 bearing
    translate([0, 0, 2 / 2 + 28 / 2]){
        // floor and support for bearing
        cylinder(10, 4, 24 / 2, true);
        translate([0, 0, 2 + bearingh / 2 + 4]){
            difference(){
                cylinder(9, 24 / 2, 24 / 2, true);
                cylinder(9, 22 / 2, 22 / 2, true);
            }
        }

    }
}

teeth = 40;
module gear(){
    worm_gear(modul=1, tooth_number=teeth, thread_starts=2, width=8, length=wormlen, worm_bore=5.5, gear_bore=4, pressure_angle=20, lead_angle=10, optimized=1, together_built=1, show_spur=1, show_worm=0);
}

loadcellmountx = -76 / 2 + 21.05 / 2;
loadcellmountw = 13.5;
loadcellmountl = 21.05;
loadcellmounth = 27;
bearingh = 7;