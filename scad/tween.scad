// morph the hotbox shape into a circle into which the top
// piece can latch

use <tweening/tween_loft.scad>
include <tweening/tween_shapes.scad>
include <core.scad>
module core2(){
    shape1= tween_dank;
    shape1Size = totalxy / 2 - 5;
    shape1Rotation = 0;
    shape1Extension = 0;
    shape1Centroid  = [0,0];

    shape2= tween_circle;
    shape2Size = totalxy / 2 - 5;
    shape2Rotation = -20;
    shape2Extension = 0;
    shape2Centroid= [0,0];
    shape2ExtensionAdjustment= 0;

    wallThickness= 5;

    isHollow = 1;

    extrusionHeight= 10;
    extrusionSlices = 33; // assume 0.3 layer height
    sliceAdjustment= 0;

    sliceHeight = extrusionHeight * 1.0 / extrusionSlices;

    tweenLoft(shape1, shape1Size, shape1Rotation, shape1Centroid, shape1Extension,
              shape2, shape2Size, shape2Rotation, shape2Centroid, shape2Extension,
              shape2ExtensionAdjustment, extrusionSlices, sliceHeight,
              sliceAdjustment, wallThickness/2, isHollow);
    translate([0, 0, extrusionHeight]){
        difference(){
            cylinder(3, totalxy / 2 - 5, totalxy / 2 - 5, true);
            translate([0, 0, 0]){
                cylinder(3, (totalxy - 3) / 2, (totalxy - 3) / 2, true);
            }
        }
    }
}
      
core2();