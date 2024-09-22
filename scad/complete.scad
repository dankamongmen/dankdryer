use <hotbox.scad>
use <croom.scad>
use <lowercoupling.scad>
include <holder.scad>

multicolor("purple"){
    rotate([0, 0, 180]){
        hotbox();
    }
}

multicolor("green"){
  spool();
}

module drop(){
    translate([0, 0, -croomz]){
        children();
    }
}

multicolor("grey"){
    drop(){
        croom();
    }
}

/*translate([0, 0, totalz - 10]){
    holder();
}*/

multicolor("pink"){
    drop(){
        dropmotor();
    }
}

multicolor("silver"){
    translate([0, 60, flr + 30 / 2]){
        acadapter();
    }
}
multicolor("black"){
    translate([0, 0, loadcellmounth]){
        drop(){
            loadcell();
        }
    }
}

multicolor("blue"){
    translate([0, 0, -60]){
        drop(){
            mirror([1, 0, 0]){
                lowercoupling();
            }
        }
    }
    translate([0, 0, -60 + 19 + shafth / 2]){
        shaft();
    }
}

dogear();