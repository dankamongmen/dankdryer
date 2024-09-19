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
        cylinder(20, 5 / 2, 5 / 2, true);
    }
}

module lmsmount(){
    difference(){
        cube([7, 7, mh], true);
        cylinder(mh, 3.25 / 2, 3.25 / 2, true);
    }
}

adj = (botrad - toprad) / sqrt(2);
theta = -(90 - atan(-croomz / adj));
echo("adj: ", adj, " theta: ", theta, " croomz: " , croomz);
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
    translate([-31.5, 0, 37]){
        rotate([0, 90, 0]){
            motorholder();
        }
    }
}

module shieldscrews(){
    // screw holes for central shield
    translate([0, 0, flr - mh / 2]){
        shieldscrew();
        mirror([1, 0, 0]){
            shieldscrew();
        }
        mirror([0, 1, 0]){
            shieldscrew();
        }
        mirror([0, 1, 0]){
            mirror([1, 0, 0]){
                shieldscrew();
            }
        }
    }
}

// 60mm wide total
// screw holes are 6mm in from sides, so they start at
// 6mm (through 10mm) and 50mm (through 54mm)
module acadapterscrews(l){
    translate([165.1 / 2 - 2, -22, 0]){
        cylinder(l, 2, 2);
    }
    mirror([0, 1, 0]){
        mirror([1, 0, 0]){
            translate([165.1 / 2 - 2, -22, 0]){
                cylinder(l, 2, 2);
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

module perfmount(){
    cube([mh, mh, mh], true);
    cylinder(mh + 6, 2, 2, true);
}

loadcellmountx = -76 / 2 + 21.05 / 2;
loadcellmountw = 13.5;
loadcellmountl = 21.05;
loadcellmounth = 27;
module loadcellmount(baseh, hlen){
    difference(){
        // load cell mounting base
        translate([loadcellmountx, 0, baseh / 2]){
            cube([loadcellmountl, loadcellmountw, baseh], true);
        }
        // holes for loadcell screws
        translate([-76 / 2 + 5.425, 0, baseh - hlen / 2]){
            cylinder(hlen, 2, 2, true);
        }
        translate([-76 / 2 + 15.625, 0, baseh - hlen / 2]){
            cylinder(hlen, 2, 2, true);
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
        shieldscrews();
        achole();
    }
    wirechannel();
    lmsmounts();
    perfmounts();
    translate([0, 0, flr]){
        loadcellmount(loadcellmounth, 14);
    }
    translate([55, -36, flr + 37 / 2]){
        rotate([0, 0, motortheta]){
            motormount();
        }
    }
}

module motor(){
    cylinder(motorboxh, motorboxd / 2, motorboxd / 2);
    translate([0, 0, motorboxh]){
        cylinder(motorshafth, motorshaftd / 2, motorshaftd / 2);
    }
}

controlroom();

module acadapter(){
    difference(){
        cube([165.1, 60, 30], true);
        translate([0, 0, -15]){
            acadapterscrews(30);
        }
    }
}

module loadcell(){
    // https://amazon.com/gp/product/B07BGXXHSW
    cube([76, 13.5, 13.5], true);
}

/*multicolor("pink"){
    translate([79, -77, -20]){
        rotate([0, 270, motortheta]){
            motor();
        }
    }
}

multicolor("silver"){
    translate([0, 60, flr + 30 / 2]){
        acadapter();
    }
    translate([0, 0, flr + loadcellmounth]){
        loadcell();
    }
}

translate([teeth / 2, 0, flr + motorboxd]){
    gear();
}*/

module lowercoupling(){
    loadcellmount(10, 5);
    translate([0, 0, 15]){
        cube([-loadcellmountx * 2 + loadcellmountl, loadcellmountw, 10], true);
    }
    // recessed area for 608 bearing
    translate([0, 0, 2 / 2 + 40 / 2]){
        cylinder(2, 24 / 2, 24 / 2, true);
        translate([0, 0, 2 + 7 / 2]){
            difference(){
                cylinder(9, 24 / 2, 24 / 2, true);
                cylinder(9, 22 / 2, 22 / 2, true);
            }
        }

    }
}
