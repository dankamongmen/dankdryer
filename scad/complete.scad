include <core.scad>
use <hotbox.scad>
use <croom.scad>
use <lowercoupling.scad>
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
    translate([0, 0, totalz + 10]){
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
    translate([0, 60, flr + 30 / 2]){
        acadapter();
    }
}

multicolor("black"){
    assembly();
}

multicolor("white"){
    drop(){
        dropgear();
    }
}

multicolor("green"){
    dropworm();
}