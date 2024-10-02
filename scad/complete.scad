include <core.scad>
use <hotbox.scad>
use <croom.scad>
use <coupling.scad>
use <top.scad>

multicolor("purple"){
    rotate([0, 0, 180]){
        hotbox();
    }
}

multicolor("green"){
  spool();
}

multicolor("orange"){
    translate([0, 0, totalz + 4]){
        mirror([0, 0, 1]){
            top();
        }
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
    drop(){
        translate([0, 60, wallz + acadapterh / 2]){
            acadapter();
        }
    }
}

multicolor("black"){
    drop(){
        assembly();
    }
}

multicolor("green"){
    drop(){
        dropworm();
    }
}