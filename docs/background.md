# Background: why not WLED or MoonLight

Findings from 2026-11-16, testing both on this WT32-ETH01 with the ceiling in Beam.

## WLED 16.0.1

- Art-Net output packs pixels contiguously from universe 0 with no start universe or gaps. To match the
  pixel controller (bar n at universe 2n) each bar needed 160 padding LEDs: 4940 LEDs in total.
- A ledmap (180 x 15) hid the padding. Bug in 16.0.1: when a ledmap has fewer entries than the LED count,
  the unmapped tail is painted black over physical LEDs >= 2700, so bars 9-15 went dark. Fix: list all
  4940 physical LEDs in the map (padding as trailing entries).
- WLED assumes square pixels; with bars ~19 LED pitches apart every 2D effect is stretched 19x across the
  bars. Most 2D effects were designed for square panels and looked poor.
- Segments live only in RAM unless saved in a preset that is the boot preset.

## MoonLight 7.0.0

- Layouts give lights real positions (Tubes, SingleRow with Y position), but the effect layer is a dense
  box: true spacing needs 267 x 180 and does not fit without PSRAM. 180 x 57 (spacing 4) left too little
  memory for the web UI; spacing 3 (180 x 43) did.
- Each NetworkSend driver costs ~16 KB; 15 drivers (one per bar) crashed the board. One driver plus black
  GridBlacks spacers (160 slots) between rows gives the controller's universe map.
- Heavy effects (Aurora, PolarNoise, Truchet, Tunnel) render at 3-16 fps at that size.

## Conclusion

Both render a rectangle that is mostly empty space between the bars, then pick the LEDs out of it. A
renderer that evaluates effects only at the 2700 real LED positions (non-square 180 x 15 buffer in metre
coordinates) needs ~8 KB and gets the proportions right.
