include <core.scad>
use <croom.scad>
//include <croom.scad>
// bottom is the bearing
// lower spacer
// geat
// upper spacer
// platform at the top

loadcellmount(4);

multicolor("blue"){
    mirror([1, 0, 0]){
        lowercoupling();
    }
}

/*
multicolor("white"){
    translate([0, 0, uloadcellmounth + shafth / 2]){
        shaft();
    }
}

multicolor("pink"){
    dropmotor();
}

multicolor("silver"){
    translate([0, 60, flr + 30 / 2]){
        acadapter();
    }
}

multicolor("black"){
    translate([0, 0, flr + loadcellmounth]){
        loadcell();
    }
}

dropgear();
*/