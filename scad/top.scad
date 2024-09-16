include <core.scad>

multicolor("white"){
    top();
}

module tophold(){
    cylinder(4, 30, 20, true, $fn = 9);
    difference(){
        holdholes(4);
    }
}

translate([150, 0, 4 / 2]){
    tophold();
}
