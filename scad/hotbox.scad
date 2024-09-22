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

// 40x80mm worth of air passage through one quadrant of floor
module floorcuts(){
    d = wallz;
    // 11 x 40 x 4
    for(i=[0:1:10]){
        translate([-80, -10 + i * 7, d / 2]){
            rotate([0, 0, 45]){
                cube([40, 4, d], true);
            }
        }
    }
    // 2 x 60 x 4
    translate([-30, 100, d / 2]){
        cube([60, 4, d], true);
    }
    translate([-30, 22, d / 2]){
        cube([60, 4, d], true);
    }
    // 12 x 20 x 4
    for(i=[0:1:11]){
        translate([-52, 28 + i * 6, d / 2]){
            cube([20, 4, d], true);
        }
    }
}

module hbcorner(){
    side = 50;
    t = totalxy / 2;
    difference(){
        translate([t - 18, t - 18, side / 2]){
            rotate([0, 0, 45]){
                cylinder(side, side / 2, 1, true, $fn = 4);
            }
        }
        translate([totalxy / 2 - 12, totalxy / 2 - 12, side / 2]){
            screw("M5", length = side);
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
    l = 32;
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
                core2();
            }
            hbcorners();
        }
        // now remove all other interacting pieces
        // 80x80mm worth of air passage cut into the floor
        floorcuts();
        mirror([1, 0, 0]){
            floorcuts();
        }
        // exhaust fan hole
        translate([0, -(totalxy - wallxy) / 2, 80 / 2 + wallz]){
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