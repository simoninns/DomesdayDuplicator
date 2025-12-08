(NOTE:  these are notes I'm taking before hopefully the last rewrite of this code, and will hopefully cover the design of said rewrite before this is done :) )

## Framing Overview

Common background for both NTSC and PAL:

The VSYNC period consists of these three components:

- A set of equalizing pulses, which are basically half-lines.  (2.3-2.35usec of sync pulse, then 0IRE for the rest of the .5H)

- A set of vertical sync pulses, which contain "broad pulses" that are .5H - 4.7usec long, followed by 4.7usec at 0IRE.

- One more set of equalizing pulses.

After the last  equalizing pulses, there are then several vertical interval lines that are not displayed, which have vertical interval test signals and laserdisc metadata.

Interlacing/field order is determined by where the VSYNC block is relative to the field's actual display lines.

### Terms

- [number]H - a multiple of the regular horizontal frequency.

## Standard-specific details

Field ordering and line number counting gets very confusing, especially since PAL and NTSC handle it completely differently.

### NTSC

- Each set of equalization and vsync pulses are 6 cycles (3H) long.
- Line numbering begins with the equalization pulse 1H after the last display HSYNC.
- The first field has it's final EQ pulse at 9.5H, second at 9.0H.
- Vertical blank interval is officially 20H.
  
For color NTSC, there is a 4 field sequence:  (table goes here) 

- Field 1: First field, positive chroma phase at on even lines
- Field 2: Second field, negative chroma phase at on even lines
- Field 3: First field, negative chroma phase at on even lines
- Field 4: Second field, positive chroma phase at on even lines

### PAL

- Each set of equalization and vsync pulses are 5 cycles (2.5H) long.
- Line counting begins with the first vertical sync pulse.
  - The first field has it's final EQ pulse at 5.5H (ld-decode: 7.5H)
  - And the second at line 318 (ld-decode: 8H)

There is an 8 field chroma burst sequence, but detecting that in ld-decode is not implemented yet.  The PAL colour burst system is much more sophisticated than NTSC's.  Page 22 of [SPEC:CCIR] has details.

## ld-decode rev5 implementation details

The framer code takes (at least some) advantage of the fact that unlike an LD player, a field can be processed as a whole.

- Internally the NTSC line numbering system is used, with line 0 set to the last line in the previous field.
  - This is corrected on output for PAL.
  
- The framing code expects the last 2-3 lines of the previous field through the VSYNC of the next field.  If this is not found, field decoding is aborted and the next seek address given will read the appropriate data.
  
### Frame detection/framing overview

First a list of sync pulses is built up using numpy magic, which consists of areas between -20 to -50IRE (NTSC, PAL is similar).  This filters out spikes and dropouts.  Then a search within those is made for core VSYNC blocks.

Once the (assumed) beginning and ending areas of the two VSYNCs are found, distance comparisons are made between them.  (This should allow for stable decoding as long as any two of the four edges are correct.)  Refinement is then made based on the actual line length between the two.

(If it turns out more anchor points are needed, I can add the beginning and ending of the equalization cycles, but I don't think it'll come to that... yet.)

Then the list of sync pulses can be processed.  Any valid pulses are added to a dictionary, which is then passed to further refinement stages - -20IRE zero crossing, then color burst (NTSC) or pilot (PAL) fine alignment.

## References

- [SPEC:CCIR] CCIR 624-4: https://www.itu.int/dms_pub/itu-r/opb/rep/R-REP-BT.624-4-1990-PDF-E.pdf
(includes data for both NTSC and PAL framing)

- [DOC:TEK] Tektronix "NTSC Systems/Television Measurements" - contains lots of good info, including the official NTSC specs as an appendix.  A similar one exists for PAL but is less comprehensive and the diagrams are all pixelated.

- [BOOK:VDEM] Video Demystified (1st edition is best for analog, but later ones are easier to get digitally...)

