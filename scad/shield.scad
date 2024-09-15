include <core.scad>

// shield around load cell
    translate([0, 0, -croomz + cbottomz + 17.5 / 2]){
        difference(){
            cube([82, 19.5, 17.5], true);
            cube([78, 14.5, 17.5], true);
        }
    }
    