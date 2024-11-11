include <core.scad>
// friction fit into esp32s3 breakout
// hole inner gap: 37.35mm
// hole outer gap: 44mm
// hole d: 3.32

// we have a small ElectroCookie perfboard we
// want to loft above our ESP32S3 breakout.
// friction fit it between the pin receptors,
// and provide a mounting surface for the two holes.

// thick enough to put an M4 screw through. these
// are printed distinctly from the main structure,
// to simplify printing. screw them in at mounttime.
liftermounth = 4;
module liftermount(){
	difference(){
		cube([liftermounth + 1,
			liftermounth + 1,
			liftermounth], true);
		translate([0, 0, 0]){
			screw("M4", length=liftermounth);
		}
	}
}

innergap = 28.2;
outergap = 33.8;
liftersidew = (outergap - innergap) / 2;
lifterl = 50; // length of board
module boardlifterside(h){
  cube([liftersidew, lifterl, h], true);
}

boardliftertoph = 2;
boardlifterholegap = 40.67;
module boardliftertop(){
	difference(){
		cube([outergap, lifterl, boardliftertoph], true);
		translate([0, boardlifterholegap / 2, 0]){
			screw("M4", length=boardliftertoph);
		}
		translate([0, -boardlifterholegap / 2, 0]){
			screw("M4", length=boardliftertoph);
		}
	}
}

lifterh = 20;
module boardlifter(){
	out = innergap + liftersidew;
	translate([out / 2, 0, lifterh / 2]){
		boardlifterside(lifterh);
	}
	translate([-out / 2, 0, lifterh / 2]){
		boardlifterside(lifterh);
	}
	translate([0, 0, lifterh]){
		boardliftertop();
	}
}

translate([0, 0, lifterh + boardliftertoph / 2]){
	mirror([0, 0, 1]){
		boardlifter();
	}
}
translate([10, lifterl / 2 + 10, liftermounth / 2]){
	liftermount();
}
translate([-10, lifterl / 2 + 10, liftermounth / 2]){
	liftermount();
}