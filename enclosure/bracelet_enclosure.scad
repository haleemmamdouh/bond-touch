// ====================================================================
// BOND TOUCH PROTOTYPE BRACELET ENCLOSURE (OpenSCAD)
// غلاف السوار الذكي للطباعة ثلاثية الأبعاد
// ====================================================================
// Outer dimensions: 54mm x 28mm x 20mm
// Inner clearance: 50mm x 24mm x 16mm (Fits ESP32/Nano 33 + Motor + Battery)
// ====================================================================

$fn = 60; // Smoothness

// PARAMETERS (in mm)
box_length = 54;
box_width  = 28;
box_height = 18;
wall_thick = 2;

usb_width  = 12;
usb_height = 8;

strap_slot_w = 20;
strap_slot_h = 3;

module bracelet_base() {
    difference() {
        // Outer Box Base with rounded corners
        hull() {
            translate([2, 2, 0]) cylinder(r=2, h=box_height);
            translate([box_length-2, 2, 0]) cylinder(r=2, h=box_height);
            translate([2, box_width-2, 0]) cylinder(r=2, h=box_height);
            translate([box_length-2, box_width-2, 0]) cylinder(r=2, h=box_height);
        }

        // Inner Cavity Hollow Out
        translate([wall_thick, wall_thick, wall_thick])
            cube([box_length - (2 * wall_thick), box_width - (2 * wall_thick), box_height]);

        // USB Cable Cutout (Side Wall)
        translate([box_length - wall_thick - 1, (box_width - usb_width)/2, wall_thick + 2])
            cube([wall_thick + 2, usb_width, usb_height]);

        // Wrist Strap Slot Left
        translate([-1, (box_width - strap_slot_w)/2, 2])
            cube([wall_thick + 2, strap_slot_w, strap_slot_h]);

        // Wrist Strap Slot Right
        translate([box_length - wall_thick - 1, (box_width - strap_slot_w)/2, 2])
            cube([wall_thick + 2, strap_slot_w, strap_slot_h]);
            
        // Ventilation holes (Bottom)
        for (i = [15 : 8 : box_length - 15]) {
            translate([i, box_width/2, -1])
                cylinder(r=1.5, h=wall_thick + 2);
        }
    }
}

module bracelet_lid() {
    lid_length = box_length - 0.4;
    lid_width  = box_width - 0.4;
    
    translate([0, box_width + 10, 0]) {
        union() {
            // Main Top Plate
            hull() {
                translate([2, 2, 0]) cylinder(r=2, h=2);
                translate([lid_length-2, 2, 0]) cylinder(r=2, h=2);
                translate([2, lid_width-2, 0]) cylinder(r=2, h=2);
                translate([lid_length-2, lid_width-2, 0]) cylinder(r=2, h=2);
            }
            // Friction-fit lip insertion
            translate([wall_thick + 0.2, wall_thick + 0.2, 2])
                cube([box_length - (2 * wall_thick) - 0.4, box_width - (2 * wall_thick) - 0.4, 2.5]);
        }
    }
}

// Render Base and Lid side-by-side
bracelet_base();
bracelet_lid();
