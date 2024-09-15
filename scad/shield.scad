include <core.scad>

// shield around load cell
translate([0, 0, 17.5 / 2]){
    difference(){
        cube([82, 19.5, 17.5], true);
        cube([78, 14.5, 17.5], true);
    }
}
shieldbinder();
mirror([1, 0, 0]){
    shieldbinder();
}
mirror([0, 1, 0]){
    shieldbinder();
}
mirror([0, 1, 0]){
    mirror([1, 0, 0]){
        shieldbinder();
    }
}