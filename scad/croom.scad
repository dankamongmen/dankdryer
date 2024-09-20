// bottom chamber for a high-temperature (140C) filament dryer
// intended to be printed in ASA
// holds the MCU, motor, AC adapter, etc.
include <core.scad>

// thickness of croom xy walls (bottom is wallz)
croomwall = 2;

// the outer radii on our top and bottom
toprad = (totalxy + 14) * sqrt(2) / 2;
botrad = totalxy * sqrt(2) / 2;
// the inner radii on its top and bottom
botinrad = botrad - 2 * sqrt(croomwall);
topinrad = toprad - 2 * sqrt(croomwall);
// distances from center to mid-outer wall
topalt = toprad / sqrt(2);
botalt = botrad / sqrt(2);
topinalt = topinrad / sqrt(2);
botinalt = botinrad / sqrt(2);

// motor is 37x75mm diameter gearbox and 6x14mm shaft
// (with arbitrarily large worm gear on the shaft)
motorboxh = 75;
motorboxd = 37;
motorshafth = wormlen; // sans worm: 14
motorshaftd = 13; // sans worm: 6
motortheta = -60;

module croomcore(){
    rotate([0, 0, 45]){
        mirror([0, 0, 1]){
            cylinder(croomz, botrad, toprad, $fn = 4);
        }
    }
}

flr = -croomz + wallz; // floor z offset
module croominnercore(){
    coreh = croomz - wallz;
    translate([0, 0, flr]){
        rotate([0, 0, 45]){
            cylinder(coreh, topinrad, botinrad, $fn = 4);
        }
    }
}

module corner(){
    translate([-totalxy / 2, -totalxy / 2, -10]){
        cube([44, 44, 20], true);
        translate([0, 0, -35]){
            rotate([0, 0, 45]){
                cylinder(50, 0, sqrt(2) * 22, true, $fn = 4);
            }
        }
    }
}

module cornercut(){
    translate([-totalxy / 2 + 12, -totalxy / 2 + 12, -10]){
        screw("M5", length = 20);
    }
}

adj = (botrad - toprad) / sqrt(2);
theta = -(90 - atan(-croomz / adj));
module rot(deg){
    rotate([theta + deg, 0, 0]){
        children();
    }
}

// circular mount screwed into the front of the motor through six holes
module motorholder(){
    d = (28.75 + 3) / 2;
    ch = 3;
    difference(){
        cylinder(ch, 37 / 2, 37 / 2, true);
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
        cylinder(ch, 12 / 2, 12 / 2, true);
    }
}

// mount for motor. runs horizontal approximately a gear
// radius away from the center.
module motormount(){
    cube([66, 37, 37], true);
    translate([0, 0, 37]){
        difference(){
            translate([0, 0, -37 / 2]){
                cube([66, 37, 37], true);
            }
            rotate([0, 90, 0]){
                cylinder(66, 37 / 2, 37 / 2, true);
            }
        }
    }
    translate([-31.5, 0, 38]){
        rotate([0, 90, 0]){
            motorholder();
        }
    }
}

module lowershield(){
    // the lower shield must not support the lower coupling,
    // but it should come right up under it to block air
    //(-shieldw + 6) / 2, (19.5 + 6) / 2
    translate([0, 0, flr + loadcellmounth / 2]){
        difference(){
            cube([shieldw + 6, 25.5 / 2, loadcellmounth], true);
            cube([shieldw + 2, 20 / 2, loadcellmounth], true);
        }
    }
}

// 60mm wide total
// screw holes are 6mm in from sides, so they start at
// 6mm (through 10mm) and 50mm (through 54mm)
module acadapterscrews(l){
    translate([165.1 / 2 - 2, -22, 0]){
        screw("M4", length = l);
    }
    mirror([0, 1, 0]){
        mirror([1, 0, 0]){
            translate([165.1 / 2 - 2, -22, 0]){
                screw("M4", length = l);
            }
        }
    }
}

// mounts for the hotbox
module corners(){
    // cut top corners out of removal, leaving supports for top
    corner();
    mirror([1, 0, 0]){
        corner();
    }
    mirror([0, 1, 0]){
        corner();
    }
    mirror([1, 1, 0]){
        corner();
    }
}

module cornercuts(){
    // holes in corners for mounting hotbox
    cornercut();
    mirror([1, 0, 0]){
        cornercut();
    }
    mirror([0, 1, 0]){
        cornercut();
    }
    mirror([1, 1, 0]){
        cornercut();
    }
}

module lmsmount(){
    difference(){
        cube([7, 7, mh], true);
        screw("M3", length = mh);
    }
}

// 32.14 on the diagonal
module lmsmounts(){
    // 12->5V mounts
    translate([40, -totalxy / 2 + 20, flr + mh / 2]){
       lmsmount();
    }
    translate([8, -totalxy / 2 + 20 + 16.4, flr + mh / 2]){
       lmsmount();
    }
}

module perfmount(){
    difference(){
        cube([7, 7, mh + 6], true);
        screw("M4", length = mh + 6);
    }
}

module perfmounts(){
    translate([-mh, -0.95 * totalxy / 2 + mh, flr + mh / 2]){
        perfmount();
        translate([-80 - 3.3, 0, 0]){
            perfmount();
        }
        translate([0, 61.75 + 3.3, 0]){
            perfmount();
            translate([-80 - 3.3, 0, 0]){
                perfmount();
            }
        }
    }
}

// hole for AC wire
module achole(){
    translate([-totalxy / 2, 60, flr + 20]){
        rotate([0, 90 - theta, 0]){
            // FIXME should only need be one croomwall thick
            cylinder(20, 7, 7, true);
        }
    }
}

// channel for ac wires running from adapter to heater
module wirechannel(){
    translate([-botalt - 5, -20, flr + 5 / 2]){
        intersection(){
            rotate([90, 0, 0]){
                difference(){
                    cylinder(totalxy / 2, 5, 5, true);
                    cylinder(totalxy / 2, 4, 4, true);
                }
            }
            translate([0, -totalxy / 4, -5 / 2]){
                cube([10, totalxy / 2, 10]);
            }
        }
    }
}

module wirechannels(){
    wirechannel();
    mirror([1, 0, 0]){
        wirechannel();
    }
}

// inverted frustrum
module controlroom(){
    difference(){
        croomcore();
        difference(){
            croominnercore();
            corners();
        }
        // holes for AC adapter mounting screws
        translate([0, 60, -croomz + 2]){
            acadapterscrews(6);
        }
        rotate([theta, 0, 0]){
            translate([0, -botalt + 10, flr + 40]){
                fanhole(20);
            }
        }
        cornercuts();
        achole();
    }
    lowershield();
    wirechannels();
    lmsmounts();
    perfmounts();
    translate([0, 0, flr]){
        loadcellmount(loadcellmounth);
    }
    translate([55, -36, flr + 37 / 2]){
        rotate([0, 0, motortheta]){
            motormount();
        }
    }
}

controlroom();