include <core.scad>

module dropplatform(){
    translate([0, 0, platformh / 2]){
        mirror([0, 0, 1]){
            platform();
        }
    }
}

dropplatform();