include <core.scad>
use <croom.scad>
//include <croom.scad>
// bottom is the bearing
// lower spacer
// geat
// upper spacer
// platform at the top
shafth = bearingh + croomz - loadcellmounth - 30;
module shaft(){
    cylinder(shafth, bearingh / 2, bearingh / 2, true);
}

multicolor("blue"){
    translate([0, 0, -60]){
        mirror([1, 0, 0]){
            lowercoupling();
        }
    }
    /*
    translate([0, 0, -60 + 19 + shafth / 2]){
        shaft();
    }
    */
}

/*
multicolor("pink"){
    translate([45, 45, flr + motormounth + motorboxh / 2]){
        rotate([0, 270, motortheta]){
            motor();
        }
    }
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

translate([teeth / 2, 0, -34]){
    gear();
}
*/