// bottom chamber for a high-temperature (140C) filament dryer
// intended to be printed in ASA
// holds the MCU, motor, AC adapter, etc.
include <core.scad>

// thickness of croom xy walls (bottom is wallz)
croomwall = 3;
// the outer radii on our top and bottom
botrad = (totalxy + 14) * sqrt(2) / 2;
toprad = totalxy * sqrt(2) / 2;
// the inner radii on its top and bottom
botinrad = botrad - croomwall * sqrt(2);
topinrad = toprad - croomwall * sqrt(2);
// distances from center to mid-outer wall
topalt = toprad / sqrt(2);
botalt = botrad / sqrt(2);
topinalt = topinrad / sqrt(2);
botinalt = botinrad / sqrt(2);

module croombottomside(){
    translate([0, -botalt + croomwall, wallz]){
        rotate([0, 90, 0]){
            intersection(){
                cylinder((botalt - croomwall) * 2, croomwall, croomwall, true);
                translate([croomwall, -croomwall, 0]){
                    cube([croomwall * 2, croomwall * 2, botalt * 2], true);
                }
            }
        }
    }
    translate([botalt - croomwall, botalt - croomwall, croomwall]){
        sphere(croomwall);
    }
}

// bottom with rounded corners for better printing
module croombottom(){
    croombottomside();
    rotate([0, 0, 90]){
        croombottomside();
    }
    rotate([0, 0, 180]){
        croombottomside();
    }
    rotate([0, 0, 270]){
        croombottomside();
    }
    translate([0, 0, wallz / 2]){
        cube([(botalt - croomwall) * 2, (botalt - croomwall) * 2, wallz], true);
    }
}

// the fundamental structure
module croomcore(){
    translate([0, 0, wallz]){
        rotate([0, 0, 45]){
            cylinder(croomz - wallz, botrad, toprad, $fn = 4);
        }
    }
    croombottom();
}

// the vast majority of the interior, removed
module croominnercore(){
    coreh = croomz - wallz;
    translate([0, 0, wallz]){
        rotate([0, 0, 45]){
            cylinder(coreh, botinrad, topinrad, $fn = 4);
        }
    }
}

// a corner at the top, to which the hotbox is mounted
module corner(){
    translate([-totalxy / 2, -totalxy / 2, croomz - 10]){
        difference(){
            union(){
                cube([44, 44, 20], true);
                translate([0, 0, -35]){
                    rotate([0, 0, 45]){
                        cylinder(50, 0, sqrt(2) * 22, true, $fn = 4);
                    }
                }
            }
            translate([12, 12, 0]){
                screw("M5", length = 20);
            }
        }
    }
}

adj = (botrad - toprad) / sqrt(2);
theta = (90 - atan(-croomz / adj));
module rot(deg){
    rotate([theta + deg, 0, 0]){
        children();
    }
}

module lowershield(){
    // the lower shield must not support the lower coupling,
    // but it should come right up under it to block air
    //(-shieldw + 6) / 2, (19.5 + 6) / 2
    translate([0, 0, wallz + loadcellmounth / 2]){
        difference(){
            cube([shieldw + 6, 38 / 2, loadcellmounth], true);
            cube([shieldw + 2, 34 / 2, loadcellmounth], true);
        }
    }
}

module acadaptermount(l){
    translate([0, 0, wallz + l / 2]){
        difference(){
            cube([7, 7, l], true);
            screw("M4", length = l);
        }
    }
}

// 60mm wide total
// screw holes are 6mm in from sides, so they start at
// 6mm (through 10mm) and 50mm (through 54mm)
module acadapterscrews(l){
    translate([165.1 / 2 - 2, -22, 0]){
        acadaptermount(l);
    }
    mirror([0, 1, 0]){
        mirror([1, 0, 0]){
            translate([165.1 / 2 - 2, -22, 0]){
                acadaptermount(l);
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

module lmsmount(){
    translate([0, 0, mh / 2]){
        difference(){
            cube([7, 7, mh], true);
            screw("M4", length = mh);
        }
    }
}

// 32.14 on the diagonal
module lmsmounts(){
    // 12->5V mounts
    translate([40, -totalxy / 2 + 20, wallz + mh / 2]){
       lmsmount();
    }
    translate([8, -totalxy / 2 + 20 + 16.4, wallz + mh / 2]){
       lmsmount();
    }
}

module perfmount(h){
    difference(){
        cube([7, 7, h], true);
        screw("M4", length = h);
    }
}

module perfmounts(){
    h = mh + 6;
    translate([-mh, -0.95 * totalxy / 2 + mh, wallz + h / 2]){
        perfmount(h);
        translate([-80 - 3.3, 0, 0]){
            perfmount(h);
        }
        translate([0, 61.75 + 3.3, 0]){
            perfmount(h);
            translate([-80 - 3.3, 0, 0]){
                perfmount(h);
            }
        }
    }
}

// hole for AC wire
module achole(){
    translate([-botalt + croomwall / 2, 60, 0]){
        rotate([0, 180 - theta, 0]){
            translate([0, 0, wallz + 20]){
                rotate([0, 90, 0]){
                    cylinder(croomwall * 2, 7, 7, true);
                }
            }
        }
    }
}

// channel for ac wires running from adapter to heater
module wirechannel(){
    channelh = 20;
    translate([-botalt + croomwall, -20, wallz]){
        rotate([90, 0, 0]){
            intersection(){
                difference(){
                    cylinder(totalxy / 2, channelh / 2, channelh / 2, true);
                    cylinder(totalxy / 2, (channelh - 1) / 2, (channelh - 1) / 2, true);
                }
                translate([channelh / 2, channelh / 2, 0]){
                    cube([channelh, channelh, totalxy / 2], true);
                }

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

// hole for hotbox fan wires
module fancablehole(){
    rotate([180 - theta, 0, 0]){
        translate([-40, botinalt + 2, croomz - 8]){
            rotate([90, 0, 0]){
                cylinder(croomwall + 2, 3, 3, true);
            }
        }
    }
}

// inverted frustrum
module croom(){
    difference(){
        croomcore();
        difference(){
            croominnercore();
            corners();
        }
        // holes for AC adapter mounting screws
        translate([0, 60, 6 / 2]){
            acadapterscrews(6);
        }
        translate([0, -botalt + 10, croomz / 2]){
            rot(0){
                fanhole(20);
            }
        }
        achole();
        fancablehole();
    }
    lowershield();
    translate([loadcellmountx, 0, (loadcellmounth + wallz) / 2]){
        loadcellmount(loadcellmounth);
    }
    wirechannels();
    lmsmounts();
    perfmounts();
    dropmotormount();
}

croom();

// testing + full assemblage
/*
multicolor("black"){
    translate([0, 0, loadcellmounth + wallz + loadcellh / 2]){
        loadcell();
    }
}

multicolor("pink"){
    dropmotor();
}

multicolor("blue"){
    translate([0, 0, loadcellmounth + wallz + loadcellh]){
        mirror([1, 0, 0]){
            lowercoupling();
        }
    }
    translate([0, 0, loadcellmounth + wallz + loadcellh + 55]){
        shaft();
    }
}

multicolor("white"){
    dropgear();
}
*/