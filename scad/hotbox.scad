// chamber for a high-temperature (140C) filament dryer
// intended to be printed in polycarbonate + PAHT-CF
// the top is lifted up and out to insert a spool
// the spool sits on corner walls
include <core.scad>
use <top.scad>

// ceramic heater 230C  77x62
// holes: 28.3/48.6 3.5
ceramheat230w = 77;
module ceramheat230(height){
	r = 3.5;
    holegapw = 32;
	holegapl = 52;
    translate([-holegapw / 2, -holegapl / 2, 0]){
        screw("M4", length = height);
        translate([holegapw, 0, 0]){
            screw("M4", length = height);
            translate([0, holegapl, 0]){
                screw("M4", length = height);
            }
        }
        translate([0, holegapl, 0]){
            screw("M4", length = height);
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
            // a series of arcs
            intersection(){
                translate([-totalxy / 2 - 42, 0, 0]){
                    cube([totalxy / 2, totalxy / 2, wallz]);
                }
                for(i = [0:1:8]){
                    linear_extrude(wallz){
                        difference(){
                            circle(56 + i * 6);
                            circle(56 + i * 6 - 4);
                        }
                    }
                }
            }
            translate([-31, 105, d / 2]){
                cube([62, 14, d], true);
            }
        }
    }
}

module hbcorner(){
    side = 50;
    t = totalxy / 2;
    difference(){
        translate([t - 18, t - 18, side / 4]){
            rotate([0, 0, 45]){
                cylinder(side / 2, side / 2, side / 4, true, $fn = 4);
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

module upcorner(){
    side = 40;
    t = totalxy / 2;
    translate([t - 15, t - 15, totalz - side / 2]){
        rotate([0, 0, 45]){
            cylinder(3 * side / 2, side / 4, side / 2, true, $fn = 4);
        }
    }
}

module upcorners(){
    // four corners for mating to croom
    upcorner();
    mirror([0, 1, 0]){
        upcorner();
    }
    mirror([1, 0, 0]){
        upcorner();
    }
    mirror([0, 1, 0]){
        mirror([1, 0, 0]){
            upcorner();
        }
    }
}

module core(){
    translate([0, 0, wallz]){
        linear_extrude(totalz - wallz - topz){
            polygon(opoints);
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

module rc522side(l){
    holer = 3.1 / 2;
    translate([-33.5 / 2 - holer, -22.25 / 2 - holer, l / 2]){
        screw("M4", length = l);
    }
    translate([34 / 2 + holer, -22.25 / 2 - holer, l / 2]){
        screw("M4", length = l);
    }
}

module rc522holes(l){
    rc522side(l);
    mirror([0, 1, 0]){
        rc522side(l);
    }
}

module hotbox(){
    difference(){
        union(){
            topbottom(wallz);
            hbcorners();
            difference(){
                core();
                // cut away top corners to reduce material costs
                upcorners();

            }
        }
        translate([-40, -40, 0]){
            rc522holes(wallz);
        }
        // we want to clear everything in the central
        // cylinder from the floor up.
        cheight = totalz - wallz;
        translate([0, 0, cheight / 2 + wallz]){
            cylinder(cheight, innerr, innerr, true);
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
        translate([0, 0, wallz / 2]){
            cylinder(wallz, columnr, columnr, true);
        }
        // screw holes for the ceramic heating element.
        // we want it entirely underneath the spool, but
        // further towards the perimeter than the center.
        translate([0, totalxy / 4 + 8, wallz / 2]){
            ceramheat230(wallz);
        }
        // hole and mounts for 150C thermocouple and heating element wires
        translate([50, -15, wallz / 2]){
            rotate([0, 0, 45]){
                thermohole();
            }
        }
    }
    translate([0, 0, totalz - topz]){
        linear_extrude(topz){
            difference(){
                circle(totalxy / 2 - 3);
                circle(innerr);
            }
        }
    }
}

hotbox();

// testing
/*
multicolor("green"){
  spool();
}

multicolor("orange"){
    top();
}
*/
