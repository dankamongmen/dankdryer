include <gears.scad>

module wormy(){
    difference(){
        union(){
            worm_gear(modul=1, tooth_number=60, thread_starts=2, width=8, length=20, worm_bore=4, gear_bore=4, pressure_angle=20, lead_angle=10, optimized=1, together_built=1, show_spur=0, show_worm=1);
    
            // fill in the central worm gear hole, save for our rotor cutout
            translate([6, 0, 0]){
                cube([6, 16, 6], true);
            }
        }
        translate([6, 12, 0]){
            rotate([90, 0, 0]){
                difference(){
                    cylinder(14, 6 / 2, 6 / 2, true);
                    // effect the D-shape of the rotor (5.5 vs 6)
                    translate([2.5, -2.5, 0]){
                        cube([0.5, 10, 14]);
                    }
                }
            }
        }
    }
}

module gear(){
    worm_gear(modul=1, tooth_number=60, thread_starts=2, width=8, length=20, worm_bore=4, gear_bore=4, pressure_angle=20, lead_angle=10, optimized=1, together_built=1, show_spur=1, show_worm=0);
}

translate([5, 0, 0]){
    wormy();
}
gear();