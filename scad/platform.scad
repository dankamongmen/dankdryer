include <core.scad>

// platform for the top of the shaft. it expands into the
// hotbox and supports the spool.

// shaft radius is bearingh / 2
// central column radius is 15

platformh = elevation;
columnr = 15;
platformd = 40;

module platform(){
    difference(){
        cylinder(platformh, platformd / 2, columnr, true);
        cylinder(platformh, gearbore / 2, gearbore / 2, true);
    }
}

module dropplatform(){
    translate([0, 0, platformh / 2]){
        platform();
    }
}

dropplatform();