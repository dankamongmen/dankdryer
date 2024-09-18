include <core.scad>

module wormy(){
    difference(){
        union(){
            worm_gear(modul=1, tooth_number=teeth, thread_starts=2, width=8, length=wormlen, worm_bore=5.5, gear_bore=4, pressure_angle=20, lead_angle=10, optimized=1, together_built=1, show_spur=0, show_worm=1);
    
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


translate([5, 0, 0]){
    rotate([90, 0, 0]){
        wormy();
    }
}
gear();