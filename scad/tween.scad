// morph the hotbox shape into a circle into which the top
// piece can latch

use <tweening/tween_loft.scad>
include <tweening/tween_shapes.scad>
include <core.scad>
module core2(){
    shape1= tween_dank;
    shape1Size = totalxy / 2 - 5;
    shape1Rotation = 0;
    shape1Extension = wallz;
    shape1Centroid  = [0,0];

    shape2= tween_circle;
    shape2Size = totalxy / 2 - 5;
    shape2Rotation = -20;
    shape2Extension = 3;
    shape2Centroid= [0,0];
    shape2ExtensionAdjustment= 0;

    wallThickness= 5;

    isHollow = 1;

    extrusionSlices = topz / 0.3; // assume 0.3 layer height
    sliceAdjustment= 0;

    sliceHeight = topz * 1.0 / extrusionSlices;

    tweenLoft(shape1, shape1Size, shape1Rotation, shape1Centroid, shape1Extension,
              shape2, shape2Size, shape2Rotation, shape2Centroid, shape2Extension,
              shape2ExtensionAdjustment, extrusionSlices, sliceHeight,
              sliceAdjustment, wallThickness/2, isHollow);
    // latches for the top. we want to drop it in and rotate it slightly,
    // locking it into place. if the apparatus is knocked over, the top
    // should remain where it is.
    
}
      
core2();


multicolor("red"){
    // latching mechanism for the top
    translate([0, 0, 4]){
        difference(){
            linear_extrude(3){
                difference(){
                    circle(totalxy / 2 - 4);
                    circle(totalxy / 2 - 10);
                }
            }
            translate([0, 0, 1.5]){
                linear_extrude(1.5){
                    difference(){
                        circle(totalxy / 2 - 5.5);
                        circle(totalxy / 2 - 8.5);
                    }
                }
            }
        }
    }
}

/*
multicolor("purple"){
    translate([0, 0, topz + 14]){
        mirror([0, 0, 1]){
            top();
        }
    }
}
*/