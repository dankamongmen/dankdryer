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

multicolor("orange"){
    translate([0, 0, totalz]){
        mirror([0, 0, 1]){
            top();
        }
    }
}

multicolor("red"){
    translate([0, 0, totalz]){
        holder();
    }
}

multicolor("grey"){
    drop(){
        croom();
    }
}

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
    translate([0, 0, loadcellmounth + wallz + loadcellh / 2]){
        drop(){
            loadcell();
        }
    }
}

multicolor("blue"){
    drop(){
        translate([0, 0, loadcellmounth + wallz + loadcellh]){
            mirror([1, 0, 0]){
                lowercoupling();
            }
        }
    }
    translate([0, 0, -60 + 19 + shafth / 2]){
        shaft();
    }
}

multicolor("white"){
    drop(){
        dropgear();
    }
}

multicolor("green"){
    dropworm();
}