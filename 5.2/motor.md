

What is this, hopefully give part number on all parts

This is a yellow TT gearmotor with a DIY optical speed encoder: a slotted disc on the output shaft, read by a RobotLinking sensor board that’s hot-glued to the gearbox. There’s no part number for the whole assembly. I can’t read the markings on the IC or slot sensor, so those two are best matches, not confirmed.

    Motor: generic dual-shaft TT gearmotor, typically 3–6 V and 1:48, about 200 RPM at 6 V unloaded. The closest named part is the Dagu DG01D-A130. The ratio isn’t marked, and 1:90 and 1:120 versions exist.
    www.makerstore.com.au +2
    Orange capacitor: ceramic disc for motor noise suppression, usually 100 nF (marked 104). I can’t read the value here.
    Encoder disc: looks like the common 20-slot type, but count the slots to confirm. The closest match is the HC-020K disc, which is 24 mm with 20 lines and a 4 mm bore. A similar part is the Adafruit 3782 snap-on disc.
    ja-bots
    core-electronics
    Green sensor board: RobotLinking, with no visible model number, and I found no listing for it. The closest generic is the HC-020K module. It’s not a standard FC-03, which has four pins (VCC, GND, D0, A0); yours has three.
    abra-electronics
    8-pin IC: almost certainly an LM393 dual comparator in a SOIC-8 package, as on these modules generally.
    Black slot sensor: IR optical interrupter. Similar modules use the ITR9608, ITR9606 or H2010.
    FC-03 RPM sensor module with slot optocoupler +2
    Resistors: SMD 1 kΩ (code 102), 10 kΩ (code 103) and 330 Ω.
    Header: 3-pin, likely VCC, GND and signal. Check the silkscreen before wiring, because the pin order varies.

With a 20-slot disc on the output shaft, you get 20 pulses per wheel revolution. A single sensor like this measures speed but not direction.
littlebirdelectronics

A sharp close-up of the IC’s top and the side of the black sensor could confirm those two part numbers.

Sources:

    Dagu DG01D-A130 datasheet
    HC-020K specs
    FC-03 module
    Adafruit TT Motor Encoder

