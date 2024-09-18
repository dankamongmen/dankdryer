// chamber for a high-temperature (140C) filament dryer
// intended to be printed in polycarbonate + PAHT-CF
// the top is lifted up and out to insert a spool
// the spool sits on corner walls
include <core.scad>
include <BOSL2/std.scad>
include <BOSL2/screws.scad>

// we need to hold a spool up to 205mm in diameter and 75mm wide
spoold = 205;
spoolh = 75;
spoolholed = 55;

// ceramic heater 230C  77x62
// holes: 28.3/48.6 3.5
ceramheat230w = 77;
module ceramheat230(height){
	r = 3.5;
    holegapw = 32;
	holegapl = 52;
    translate([-holegapw / 2, -holegapl / 2, 0]){
        cylinder(height, r / 2, r / 2, true);
        translate([holegapw, 0, 0]){
            cylinder(height, r / 2, r / 2, true);
            translate([0, holegapl, 0]){
                cylinder(height, r / 2, r / 2, true);
            }
        }
        translate([0, holegapl, 0]){
            cylinder(height, r / 2, r / 2, true);
        }
    }
}

// 40x80mm worth of air passage through one quadrant of floor
module floorcuts(){
    d = wallz;
    for(i=[0:1:7]){
        translate([-80, 15 + i * 7, d / 2]){
            rotate([0, 0, 45]){
                cube([40, 4, d], true);
            }
        }
    }
    for(i=[0:1:11]){
        translate([-42, 28 + i * 6, d / 2]){
            cube([40, 4, d], true);
        }
    }
}

module corner(){
    side = 40;
    t = totalxy / 2;
    difference(){
        translate([t - side / 2 + 4, t - side / 2 + 4, side / 2]){
            rotate([0, 0, 45]){
                cylinder(side, side / 2, 1, true, $fn = 4);
            }
        }
        translate([totalxy / 2 - 12, totalxy / 2 - 12, side / 2]){
            cylinder(side, 5 / 2, 5 / 2, true);
        }
    }
}

module corners(){
    // four corners for mating to croom
    corner();
    mirror([0, 1, 0]){
        corner();
    }
    mirror([1, 0, 0]){
        corner();
    }
    mirror([0, 1, 0]){
        mirror([1, 0, 0]){
            corner();
        }
    }
}

module hotbox(){
    difference(){
        union(){
            topbottom(wallz);
            translate([0, 0, wallz]){
                linear_extrude(totalz - wallz - topz){
                    polygon(
                        iipoints,
                    paths=[
                        [0, 1, 2, 3, 4, 5, 6, 7],
                        [8, 9, 10, 11, 12, 13, 14, 15]
                    ]);
                }
            }
            corners();
        }
        // now remove all other interacting pieces
        // 80x80mm worth of air passage cut into the floor
        floorcuts();
        mirror([1, 0, 0]){
            floorcuts();
        }
        // recess for the top
        translate([0, 0, totalz - wallz]){
            mirror([0, 0, 1]){
                top();
            }
        }
        // hole for heating element wires
        translate([ceramheat230w / 2 + 10, totalxy / 4 + 10, 0]){
            cylinder(wallz, 4, 4);
        }
        // exhaust fan hole
        translate([0, -(totalxy - wallxy) / 2, 80 / 2 + 5]){
            fanhole(wallxy);
        }
        // central column
        cylinder(10, 30 / 2, 30 / 2, true);
        // screw holes for the ceramic heating element. we want it entirely
        // underneath the spool, but further towards the perimeter
        // than the center.
        translate([0, totalxy / 4 + 10, wallz / 2]){
            ceramheat230(wallz);
        }
    }
}

// testing
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

/* multicolor("green"){
    spool();
} */

multicolor("purple"){
    hotbox();
}