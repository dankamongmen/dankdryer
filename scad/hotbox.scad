// chamber for a high-temperature (140C) filament dryer
// intended to be printed in polycarbonate + PAHT-CF
// the top is lifted up and out to insert a spool
// the spool sits on corner walls
include <core.scad>
use <tween.scad>

// ceramic heater 230C  77x62
// holes: 28.3/48.6 3.5
ceramheat230w = 77;
module ceramheat230(height){
	r = 3.5;
    holegapw = 32;
	holegapl = 52;
    translate([-holegapw / 2, -holegapl / 2, 0]){
        screw("M3", length = height);
        translate([holegapw, 0, 0]){
            screw("M3", length = height);
            translate([0, holegapl, 0]){
                screw("M3", length = height);
            }
        }
        translate([0, holegapl, 0]){
            screw("M3", length = height);
        }
    }
}

// 2500mm² worth of air passage through one side of floor.
// the fan is 80mm x 80mm, suggesting 6400 (and thus 3200 per
// side), but it's actually only π40² or ~5027.
module floorcuts(){
    d = wallz;
    // all floor holes ought be in the actual hotbox, not under
    // the infill (which would cause support problems anyway).
    intersection(){
        translate([0, 0, d / 2]){
            cylinder(d, totalxy / 2 - 5, totalxy / 2 - 5, true);
        }
        union(){
            // 8 x 40 x 4
            for(i=[0:1:8]){
                translate([-80, 6 + i * 8, d / 2]){
                    rotate([0, 0, 45]){
                        cube([40, 4, d], true);
                    }
                }
            }
            // 2x 62 x 4
            translate([-31, 100, d / 2]){
                cube([62, 4, d], true);
            }
            translate([-31, 22, d / 2]){
                cube([62, 4, d], true);
            }
            // 12 x 20 x 4
            for(i=[0:1:11]){
                translate([-52, 28 + i * 6, d / 2]){
                    cube([20, 4, d], true);
                }
            }
        }
    }
}

module hbcorner(){
    side = 50;
    t = totalxy / 2;
    difference(){
        translate([t - 18, t - 18, 2 * side / 6]){
            rotate([0, 0, 45]){
                cylinder(2 * side / 3, side / 2, 1, true, $fn = 4);
            }
        }
        translate([totalxy / 2 - 12, totalxy / 2 - 12, 2 * side / 6]){
            screw("M5", length = 2 * side / 3);
        }
    }
}

module hbcorners(){
    // four corners for mating to croom
    hbcorner();
    mirror([0, 1, 0]){
        hbcorner();
    }
    mirror([1, 0, 0]){
        hbcorner();
    }
    mirror([0, 1, 0]){
        mirror([1, 0, 0]){
            hbcorner();
        }
    }
}

module core(){
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
}

// hole and mounts for 150C thermocouple and heating element wires
module thermohole(){
    r = 8;
    l = 34;
    w = 6;
    cylinder(wallz, r, r, true);
    cube([w, l, wallz], true);
    // mounting holes are orthogonal to where we drop in terminals
    translate([-12.5, 0, wallz / 2]){
        screw("M4", l = wallz);
    }
    translate([12.5, 0, wallz / 2]){
        screw("M4", l = wallz);
    }
}

module hotbox(){
    difference(){
        union(){
            topbottom(wallz);
            core();
            translate([0, 0, totalz - topz]){
                core2(totalxy / 2 - 5);
            }
            hbcorners();
            cheight = totalz - wallz + 3;
            translate([0, 0, cheight / 2 + wallz]){
                difference(){
                    cylinder(cheight, totalxy / 2, totalxy / 2, true);
                    cylinder(cheight, totalxy / 2 - 5, totalxy / 2 - 5, true);
                }
            }
        }
        // now remove all other interacting pieces
        // 80x80mm worth of air passage cut into the floor
        floorcuts();
        mirror([1, 0, 0]){
            floorcuts();
        }
        // exhaust fan hole
        translate([0, -(totalxy - wallxy) / 2, 80 / 2 + wallz]){
            fanhole(wallxy + 16);
        }
        // central column
        cylinder(10, 30 / 2, 30 / 2, true);
        // screw holes for the ceramic heating element. we want it entirely
        // underneath the spool, but further towards the perimeter
        // than the center.
        translate([0, totalxy / 4 + 10, wallz / 2]){
            ceramheat230(wallz);
        }
        // hole and mounts for 150C thermocouple and heating element wires
        translate([40, -10, wallz / 2]){
            thermohole();
        }
    }
}

// testing
hotbox();

/*
multicolor("green"){
  spool();
}

multicolor("orange"){
    top();
}
*/