// The web page: presets, brightness, effect parameters, palettes, network settings. Every control
// has a tooltip: hover with a mouse; on a touch screen tap its label (or long-press a button).
#pragma once
#include <pgmspace.h>

static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>beamLED</title>
<style>
:root{--bg:#121212;--card:#1c1c1c;--line:#333;--fg:#ddd;--dim:#888;--acc:#e0a020;--warn:#d04040}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,sans-serif}
main{max-width:640px;margin:auto;padding:12px 16px 40px}
h1{font-size:20px;margin:0;flex:1}
h2{font-size:14px;text-transform:uppercase;letter-spacing:.06em;color:var(--dim);margin:22px 0 8px;cursor:pointer;user-select:none}
h2::before{content:'▾';display:inline-block;width:1.2em;transition:transform .15s}
h2.folded::before{transform:rotate(-90deg)}
[hidden]{display:none!important}
@media (prefers-reduced-motion:reduce){h2::before{transition:none}}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:10px 12px}
button{background:#2a2a2a;color:var(--fg);border:1px solid #444;border-radius:7px;padding:7px 12px;font:inherit;cursor:pointer}
button.on{background:var(--acc);border-color:var(--acc);color:#000}
button.warn{background:var(--warn);border-color:var(--warn);color:#fff}
button:disabled{opacity:.4;cursor:default}
.na{opacity:.4;cursor:not-allowed}
.na input,.na select,.na .pdd{pointer-events:none}
.top{display:flex;align-items:center;gap:10px}
.top button{min-width:72px}
.presets{display:grid;grid-template-columns:repeat(auto-fill,minmax(110px,1fr));gap:6px}
.presets button{text-align:left;padding:9px 10px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.presets button.add{text-align:center;color:var(--dim)}
.presets button.out{opacity:.5}
.auto{display:flex;flex-wrap:wrap;align-items:center;gap:8px;margin-top:10px}
.auto select{flex:0 1 auto}
.auto input[type=number]{flex:0 0 80px;width:80px}
.auto .hint{margin-left:auto}
.row{display:flex;align-items:center;gap:10px;margin:7px 0}
.row label{flex:0 0 130px;color:#bbb}
.row input[type=range]{flex:1;min-width:0;accent-color:var(--acc)}
.row output{flex:0 0 46px;text-align:right;font-variant-numeric:tabular-nums;color:var(--dim)}
.btns{display:flex;flex-wrap:wrap;gap:6px;margin:7px 0}
select,input[type=text],input[type=password],input[type=number]{flex:1;min-width:0;background:#222;color:var(--fg);border:1px solid #444;border-radius:6px;padding:6px 8px;font:inherit}
.pal{display:flex;flex-wrap:nowrap;align-items:center;gap:10px;padding:10px 0;border-bottom:1px solid var(--line)}
.pal:last-child{border-bottom:0}
.pal input[type=text]{flex:0 0 110px;width:110px;order:0}
.pal input[type=color]{width:38px;height:32px;border:1px solid #444;border-radius:6px;background:none;padding:2px}
.pal .sw{flex:1 1 auto;min-width:80px;height:16px;border-radius:4px;position:relative;margin:0 12px;touch-action:none;order:1}
.pal .stop{position:absolute;top:-6px;width:20px;height:28px;margin-left:-10px;border-radius:5px;border:2px solid #fff;box-shadow:0 0 0 1px #000a;cursor:pointer;touch-action:none}
.pal .stop:focus-visible{outline:2px solid var(--acc);outline-offset:2px}
.pal .stop.off{opacity:.25;border-style:dashed}
.pal .sw{cursor:copy}
.pal .stop input{position:absolute;left:0;top:100%;width:1px;height:1px;opacity:0;pointer-events:none;border:0;padding:0}
.pal .stop:hover,.pal .stop.drag{border-color:var(--acc)}
.pal .del{flex:0 0 auto;order:2}
.pal-add{margin-top:8px}
.pal button{padding:4px 10px}
.conn{color:var(--warn);font-size:13px;margin:4px 0}
.conn:empty{display:none}
.stats{display:grid;grid-template-columns:auto 1fr;gap:4px 14px;font-size:14px}
.stats dt{color:var(--dim)}
.stats dd{margin:0;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}
.hint{color:var(--dim);font-size:13px}
.unis{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:6px 12px;margin:6px 0 10px}
.unis select.ord{flex:0 0 72px;width:72px}
.row select.ord{flex:0 0 72px}
.unis label{display:flex;align-items:center;gap:6px;color:#bbb;font-size:13px}
.unis label span:first-child{flex:0 0 46px}
.unis input{flex:0 0 72px;width:72px}
.row input[type=number]{flex:0 0 90px}
[data-tip] label,label[data-tip],h2[data-tip]{cursor:help;text-decoration:underline dotted #555;text-underline-offset:3px}
.motion{border-top:1px solid var(--line);margin-top:10px;padding-top:4px}
.pdd{flex:1;min-width:0;position:relative}
.pdd>button{width:100%;padding:0;border-radius:6px;overflow:hidden;display:flex}
.pdd .g{flex:1;display:block;padding:7px 10px;color:#fff;font-weight:600;text-align:left;text-shadow:0 0 3px #000,0 0 2px #000,0 0 1px #000;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.pdd>button .g::after{content:'▾';float:right;margin-left:8px}
.pdd ul{position:absolute;z-index:20;left:0;right:0;top:100%;margin:4px 0 0;padding:4px;list-style:none;background:#1b1b1b;border:1px solid #444;border-radius:8px;max-height:60vh;overflow:auto;box-shadow:0 6px 18px #000a}
.pdd li{margin:4px 0;border-radius:5px;overflow:hidden;cursor:pointer;outline:none}
.pdd li.sel{box-shadow:0 0 0 2px var(--acc)}
.pdd li:hover,.pdd li:focus-visible{box-shadow:0 0 0 2px #fffa}
.curve{display:block;width:100%;height:44px}
.curve path{fill:none;stroke:var(--acc);stroke-width:2}
.curve line{stroke:#444;stroke-width:1}
.moving label::after{content:" ↔";color:var(--acc)}
#elsewhere{position:fixed;inset:0;z-index:20;display:none;align-items:center;justify-content:center;background:#121212e8;padding:16px}
#elsewhere .card{max-width:360px;text-align:center}
#elsewhere p{margin:6px 0 14px}
#tip{position:fixed;z-index:9;display:none;max-width:min(380px,calc(100vw - 24px));background:#2b2b2b;color:#eee;
  border:1px solid #555;border-radius:8px;padding:8px 10px;font-size:13px;line-height:1.45;box-shadow:0 4px 16px #000a;pointer-events:none}
</style></head><body><main id="app">Loading...</main><div id="tip"></div>
<div id="elsewhere"><div class="card"><p id="elsewhere-text"></p><button class="on" id="elsewhere-go">Continue here</button></div></div>
<script>
let S, N;
const h = (tag, props = {}, ...kids) => { const e = Object.assign(document.createElement(tag), props); e.append(...kids); return e; };
let saveBtn = null, revertBtn = null;  // the playing preset's Save and Revert: lit by any edit
function markChanged(i) {
  if (!S || !S.presets[i]) return;
  S.presets[i].changed = true;
  if (saveBtn && i == S.current) { on(saveBtn); saveBtn.className = 'on'; saveBtn.textContent = 'Save •'; }
  if (revertBtn && i == S.current) on(revertBtn);
}
const post = (u, o) => {
  if (u == '/api/preset' && !o.save && !o.revert && o.auto == null) markChanged(o.index);  // In Auto isn't a preset edit
  return fetch(u, {method: 'POST', body: JSON.stringify(o)}).then(r => r.ok ? r.json() : r.text().then(t => { throw new Error(t); }));
};

// Sliders go to the board as OSC over the live connection: the latest value of each address
// once per screen refresh, applied on the board's next frame. No HTTP request per move. Without
// the live connection: the old way (an HTTP request at most every 120 ms). `persist`: also one
// HTTP request half a second after the last move, so the setting is saved (brightness).
const oscOut = {}, persistTimer = {};
let oscQueued = false;
function oscMessage(addr, v) {
  const pad = n => (n + 4) & ~3, a = pad(addr.length), buf = new ArrayBuffer(a + 4 + 4), u8 = new Uint8Array(buf);
  for (let k = 0; k < addr.length; k++) u8[k] = addr.charCodeAt(k);
  u8[a] = 44; u8[a + 1] = 102;  // ",f"
  new DataView(buf).setFloat32(a + 4, v);
  return buf;
}
function flushOsc() {
  oscQueued = false;
  if (!sock || sock.readyState != 1) return;
  for (const a in oscOut) { sock.send(oscMessage(a, oscOut[a])); delete oscOut[a]; }
}
function liveSend(addr, v, fallback, persist) {
  if (!sock || sock.readyState != 1 || !sock.accepted) return throttle(addr, fallback);
  oscOut[addr] = v;
  if (!oscQueued) { oscQueued = true; requestAnimationFrame(flushOsc); }
  if (persist) { clearTimeout(persistTimer[addr]); persistTimer[addr] = setTimeout(fallback, 500); }
}
const presetAddr = key => '/beamled/presets/' + (S.current + 1) + '/' + key;
// At most one request per key every 120 ms while a slider moves; the last value always goes out
const pend = {};
function throttle(k, fn) {
  if (pend[k]) { pend[k] = fn; return; }
  fn(); pend[k] = true;
  setTimeout(() => { const f = pend[k]; delete pend[k]; if (typeof f == 'function') throttle(k, f); }, 120);
}

// ---- tooltips: data-tip on a control or its row. Mouse: hover. Touch: tap the label, or
// long-press a button.
const tipBox = document.getElementById('tip');
let tipFor = null, touch = false;
const tip = (el, text) => { if (text) el.dataset.tip = text; return el; };
// A control that can't be used now stays on the page, greyed, and its hint says why. Inside a
// row the inputs are disabled and let the pointer through to the row (whose hint shows); a lone
// button keeps its hint and has its clicks stopped (on touch, a tap shows the hint instead).
function off(el, why) {
  if (el.dataset.tip0 == null) el.dataset.tip0 = el.dataset.tip || '';
  el.dataset.tip = (el.dataset.tip0 ? el.dataset.tip0 + ' ' : '') + 'Not available now: ' + why;
  el.classList.add('na');
  el.setAttribute('aria-disabled', 'true');
  el.querySelectorAll('input,select,button').forEach(c => { c.disabled = true; });
  if (el.matches('button')) el.tabIndex = -1;
  return el;
}
function on(el) {
  if (el.dataset.tip0 != null) { el.dataset.tip = el.dataset.tip0; delete el.dataset.tip0; }
  el.classList.remove('na');
  el.removeAttribute('aria-disabled');
  el.querySelectorAll('input,select,button').forEach(c => { c.disabled = false; });
  if (el.matches('button')) el.removeAttribute('tabindex');
  return el;
}
document.addEventListener('click', e => {
  const o = e.target.closest && e.target.closest('.na');
  if (!o) return;
  e.preventDefault(); e.stopImmediatePropagation();
  if (touch) showTip(o);
}, true);
function showTip(el) {
  tipFor = el; tipBox.textContent = el.dataset.tip; tipBox.style.display = 'block';
  const r = el.getBoundingClientRect(), b = tipBox.getBoundingClientRect();
  let top = r.bottom + 6;
  if (top + b.height > innerHeight - 8) top = r.top - b.height - 6;
  tipBox.style.top = Math.max(8, top) + 'px';
  tipBox.style.left = Math.max(8, Math.min(r.left, innerWidth - b.width - 8)) + 'px';
}
function hideTip() { tipFor = null; tipBox.style.display = 'none'; }
addEventListener('pointerdown', e => { touch = e.pointerType != 'mouse'; }, true);
document.addEventListener('mouseover', e => {
  if (touch) return;
  const t = e.target.closest('[data-tip]');
  if (!t) hideTip(); else if (t != tipFor) showTip(t);
});
document.addEventListener('click', e => {
  const t = e.target.closest('[data-tip]');
  if (touch && t && e.target.tagName == 'LABEL') showTip(t);  // headings fold; their help: long press
  else if (touch) hideTip();
}, true);
document.addEventListener('contextmenu', e => {
  const t = e.target.closest('[data-tip]');
  if (t && touch) { e.preventDefault(); showTip(t); }
});
addEventListener('scroll', () => { if (!touch) hideTip(); }, true);

const H = {
  on: 'Switches the ceiling on or off with a one-second fade, the same as a long press of the button on the board. Off keeps the board running, so it comes back where it was.',
  brightness: 'Master brightness for every preset. It works after the gamma curve, so 50% looks about half as bright. Saved, and kept after a power cut.',
  backup: 'Everything this board keeps in one file: presets (as they play now), palettes, Auto, Ableton Link, output, layout and network, the hotspot password too. Load it on another beamLED, or on this one later, to set it up in one go.',
  rowOrder: 'The colour order the LEDs of a row expect: RGB for most, GRB for many strips (red shows as green otherwise). Set it per row, or for all rows at once, then Apply.',
  restart: 'Restarts the board, after a second tap to confirm. The lights go out for a few seconds. Hold the button on the board while it powers up for config mode (hotspot and this page only).',
  status: 'Frames per second (the target is 40) and the time one frame takes to draw (25 ms is the budget). Temperature: the chip’s own sensor, uncalibrated, so watch it change rather than trust the exact number; 60-70 °C is normal. Running: time since the board started, and why it started: power-on and restart (an update or a settings change) are normal; brown-out (the power supply dipped), watchdog or crash mean something went wrong. Then the network connection, WiFi, Ableton Link and the free memory.',
  auto: 'Auto: the presets change by themselves, every so many seconds, or every so many bars with Ableton Link (always on the bar line). Only the presets with In Auto ticked take part; the others are dimmed. Picking a preset by hand, here, with the button or over OSC, starts the count again from there. While the ceiling is off Auto waits. A double press of the button on the board turns Auto on, a short press turns it off.',
  autoEvery: 'How often Auto changes preset: a number of seconds, or a number of bars (4 beats each) on the beat grid of Ableton Link or the board\u2019s own tempo.',
  autoOrder: 'In order: the next preset in the list that is in Auto, round and round. Shuffle: a random one each time, never the same twice in a row.',
  inAuto: 'Whether Auto plays this preset. Leave out presets you only want to pick by hand. Saved at once.',
  presets: 'Click a preset to play it. The button on the board steps through them in this order (short press: next and Auto off, double press: Auto on, long press: on/off), and the one playing is kept after a power cut. Up to 32 presets. A • after a name: changes not saved yet (Save or Revert in its settings).',
  play: 'Plays this preset with a short fade.',
  add: 'Adds a new preset at the end, a copy of the one playing, and plays it. Up to 32 presets.',
  name: 'The preset’s name, shown on its button. Up to 19 characters.',
  earlier: 'Moves this preset one place earlier in the list, and in the order the board’s button steps through them.',
  later: 'Moves this preset one place later in the list, and in the order the board’s button steps through them.',
  del: 'Deletes this preset: click once, then again within 3 seconds to confirm. The last remaining preset can’t be deleted.',
  effect: 'The effect this preset shows. Changing it resets the parameters below to that effect’s defaults.',
  palette: 'The colours this preset uses, from the Palettes list below. Editing a palette changes every preset that uses it.',
  timing: 'Time: speeds are in their own units (m/s, rpm, …). Time, follow tempo: the set speed is the speed at 120 BPM and scales with the tempo, so at 240 BPM it runs twice as fast. Beats (only for effects with a cycle): one cycle (a band, a revolution, …) takes a whole number of beats and lines up with the bar; the speed slider then gives way to Beats per cycle.',
  beats: 'How many beats one cycle takes; 4 beats = one bar. After a change the motion eases onto the new beat grid over a few seconds instead of jumping.',
  reverse: 'Runs the cycle backwards: the lighthouse turns the other way, bands roll back, rings flow in.',
  sparkle: 'Twinkles on top of whatever preset plays: one setting for all presets, kept when the preset changes (not drawn over the Sparkle effect, which is twinkles itself). 0 = none, 1 = about 8% of the LEDs lit at any moment. Their colours: Sparkle colour.',
  sparkleBeat: 'When twinkles start. Free: at any moment. Otherwise only on the beat grid of Ableton Link or the board’s own tempo: every 4 beats (once a bar), 2 beats, 1 beat, 1/2, 1/4 or 1/8 beat. The same amount of sparkle as Free, in time with the music.',
  mirror: 'The right half of the ceiling becomes the mirror image of the left, along the rows: anything moving right on the left half moves left on the right half. Only the left half is rendered, so the effect takes about half the time. The Sparkle overlay is not mirrored.',
  sparkleBurst: 'Off: single twinkles, each with the same soft rise and fade as in Free. On: a batch flashes at once on each tick and fades out before the next; Sparkle overlay sets how many (1 = 6% of the LEDs).',
  paletteLock: 'Lock every preset to one palette, for a show in one set of colours: whichever preset plays (picked here, by the button, by Auto or OSC) uses this palette instead of its own. The presets’ own palettes are kept, so Each preset’s own brings them back. Not saved: after a restart each preset has its own palette again.',
  sparkleStep: 'How the twinkles pick their colours from the palette. 0 = each one a random colour. Above 0 = each new twinkle this far along the palette from the one before. A hundred or more twinkles start every second, so tiny steps matter: about 0.0005 = the colours drift slowly through the palette (a burst in neighbouring colours), 0.01 and up already look mixed, 0.5 = opposite colours in turn. The left part of the slider is fine, the right part coarse.',
  sparkleColour: 'The colours of the Sparkle overlay’s twinkles: white, random colours from the playing preset’s palette (they change with the preset), or from any other palette. The twinkle’s colour replaces what is under it, so a palette of black gives dark twinkles.',
  pulse: 'A brightness accent on the beat: full on each beat and dipping in between, strongest on the bar’s first beat. 0 = steady. Follows Ableton Link, or the board’s own tempo.',
  reset: 'Sets this preset’s effect parameters back to the effect’s defaults (not saved until you click Save). Name, palette, timing and pulse stay as they are.',
  save: 'Stores this preset as it plays now: its name, effect, palette, sliders, motions, timing and pulse. Until you save, changes (here or from OSC) play on the ceiling but the preset keeps its saved settings, so you can tinker freely. Unsaved changes are kept while you switch presets, and lost on a power cut.',
  revert: 'Goes back to this preset’s saved settings, dropping the changes since the last Save.',
  motion: 'A Motion moves one of this effect’s sliders back and forth by itself, between From and To: a beam swinging (Lighthouse Angle), bands scrolling to and fro (Offset), a width breathing. Off: no motion. The slider it moves shows ↔ and follows it. Up to three motions can move three sliders (Drops: Centre X and Centre Y, say), or run out of step with Phase.',
  shape: 'How it travels between the ends. Sine: smooth, like a pendulum. Linear: a steady speed, turning sharply at the ends. Ease in-out: slow at the ends, quick through the middle. Hold at ends: rests at each end before turning back. Overshoot: goes a little past each end and settles. Bounce: bounces as it arrives at each end. Random slide: glides to a new random value between From and To every half period (the same values every time, on the beat grid in beats). Min max: From for the first half of the period, To for the second, switching at once with no transition. Random jump: the same random values as Random slide, but it jumps to each one at once and holds it for half a period. The drawing below shows one there-and-back (Random slide and Random jump: three).',
  periodIn: 'Seconds: one there-and-back takes a set time (scaled with the tempo when the preset follows it). Beats: it takes a number of beats and keeps to the beat grid with Ableton Link, at From on the bar.',
  periodBeats: 'How many beats one full there-and-back takes; 4 beats = one bar. With 8 beats it is at From on one bar, at To on the next bar, and back at From on the bar after that.',
  periodSec: 'How many seconds one full there-and-back takes.',
  from: 'One end of the motion, in the slider’s own units.',
  to: 'The other end of the motion, in the slider’s own units. To below From is fine: it simply runs the other way.',
  phase: 'Where in its cycle the motion is: 0 = at From at the start (or on the bar), 0.5 = at To. Give a second motion 0.25 or 0.5 to run it out of step with the first.',
  palettes: 'Palettes are shared: each preset picks one. Effects blend smoothly between the colours in order; looping effects wrap from the last colour back to the first.',
  palName: 'The palette’s name, as listed in each preset’s Palette choice. Up to 15 characters.',
  palColour: 'A colour of this palette: tap it to change it (the ceiling follows as you pick), drag it along the bar to move it, drag it off the bar (or press Delete) to remove it.',
  palStops: 'The palette as effects use it, first colour on the left. Click the bar to add a colour there (up to 7). Each marker is one colour: tap it to change the colour, drag it along the bar to move it (bunch colours together for a quick change, spread them for a long blend), drag it off the bar to remove it (at least 2 stay).',
  palAddPalette: 'Adds a new palette at the end, a copy of the last one, to change as you like. Up to 16 palettes.',
  palDelete: 'Deletes this palette: click once, then again within 3 seconds to confirm. Presets that use it switch to the first palette. The last palette can\u2019t be deleted.',
  saveAsNew: 'You renamed this preset, so Save makes a new preset with the new name and your changes, and plays it. This preset goes back to how it was saved. To keep this preset\u2019s name, change the name back.',
  target: 'Where the Art-Net goes: the pixel controller\u2019s address, or a PC running a visualiser such as Beam for testing. Or a broadcast to everything on the cable: the network\u2019s .255 address (192.168.1.255, for example), or 255.255.255.255. With a broadcast every device on the cable gets all the universes. Art-Net only goes out over the cable, never over WiFi.',
  gamma: 'The brightness curve, so fades look even to the eye. 2.2 suits most LEDs; lower makes dark parts brighter and flatter, higher gives deeper darks.',
  link: 'Ableton Link. Enabled: tempo and beat are shared with Ableton Live and other Link apps on the same network (the Live laptop joins the board’s network or its hotspot). Disabled: the board keeps its own tempo.',
  bpm: 'The tempo in beats per minute. With Ableton Link enabled and peers connected this changes everyone’s tempo; without peers it sets the board’s own.',
  latency: 'How much later the LEDs show a frame than the board draws it (network and pixel controller). Frames are drawn this far ahead so beats land on time. Raise it if the lights trail the music.',
  osc: 'OSC control, for MIMa and other OSC or OSCQuery apps. In MIMa, add beamLED as an OSCQuery application at beamled.local:80 (or this board’s IP address, port 80), and send OSC to port 8000: it lists every preset, parameter, motion and palette colour with its range and help, and its knobs and lights follow changes made here or with the button. Preset changes from OSC play at once and are kept with Save, like changes made here.',
  nx: 'How many LEDs each row (bar) has.',
  ny: 'How many rows (bars) there are.',
  len: 'The length of a row in metres; the LED spacing is this divided by LEDs per row.',
  aspect: 'Row spacing divided by LED spacing, for evenly spaced rows. Used when Measured rows is off or the row count differs from the ceiling’s.',
  measured: 'Uses measured, uneven row positions built into the firmware, when the number of rows matches them. Off: rows evenly spaced by Aspect.',
  rowUni: 'The Art-Net universe each row starts at. A row of more than 170 LEDs continues into the next universe(s). Set them to match how the pixel controller is patched.',
  fill: 'Numbers the rows one after another, starting at the number given, leaving each row the universes it needs.',
  apply: 'Applies the layout at once and saves it. Presets and palettes are kept.',
  netName: 'The board’s name: http://<name>.local, the hotspot’s name and the firmware update name. Takes effect after the restart.',
  fallback: 'The address the board takes when no DHCP server answers within 10 seconds, for example when it is connected directly to a pixel controller. Keep it in the same subnet as the devices it talks to. Empty: an automatic 169.254.x.x address.',
  apPass: 'A password for the config-mode hotspot (button held at power-on), 8 characters or more. Empty: an open hotspot. Type - to remove a saved password.',
  netSave: 'Saves the network settings and restarts the board; the lights go out for a few seconds.',
};

const decimals = s => s >= 1 ? 0 : s >= 0.1 ? 1 : 2;
// Sliders by key, so a change pushed by the board moves them in place (no redraw)
let sliders = {};
function slider(label, min, max, step, val, fmt, onin, help, key) {
  const o = h('output', {textContent: fmt(val)});
  const r = h('input', {type: 'range', min, max, step, value: val});
  r.oninput = () => { o.textContent = fmt(+r.value); onin(+r.value); };
  if (key) sliders[key] = {r, o, fmt};
  return tip(h('div', {className: 'row'}, h('label', {textContent: label}), r, o), help);
}
function select(label, items, val, onch, help) {
  const s = h('select');
  items.forEach((t, i) => s.append(h('option', {value: i, textContent: t, selected: i == val})));
  s.onchange = () => onch(+s.value);
  return tip(h('div', {className: 'row'}, h('label', {textContent: label}), s), help);
}
// The palette picker: each palette drawn as its gradient with its name on top
const palGrad = q => {
  const st = q.stops && q.stops.length == q.colors.length ? q.stops : evenStops(q.colors.length);
  return 'linear-gradient(90deg,' + q.colors.map((c, j) => c + ' ' + (st[j] * 100).toFixed(1) + '%').join(',') + ')';
};
function paletteSelect(label, val, onch, help) {
  return gradSelect(label, S.palettes.map(q => ({name: q.name, bg: palGrad(q)})), val, onch, help);
}
// A picker of gradient strips with a name on each: items [{name, bg}]
function gradSelect(label, items, val, onch, help) {
  const wrap = h('div', {className: 'pdd'});
  const face = h('span', {className: 'g'});
  const btn = h('button', {type: 'button'}, face);
  btn.setAttribute('aria-haspopup', 'listbox');
  btn.setAttribute('aria-expanded', 'false');
  const show = v => { const q = items[v]; face.textContent = q ? q.name : '?'; face.style.background = q ? q.bg : ''; };
  show(val);
  let list = null;
  const outside = e => { if (!wrap.contains(e.target)) close(); };
  const close = () => {
    if (!list) return;
    list.remove(); list = null;
    btn.setAttribute('aria-expanded', 'false');
    document.removeEventListener('pointerdown', outside, true);
  };
  const pick = v => { close(); btn.focus(); if (v != val) { val = v; show(v); onch(v); } };
  btn.onclick = () => {
    if (list) { close(); return; }
    list = h('ul', {role: 'listbox'});
    items.forEach((q, i) => {
      const g = h('span', {className: 'g', textContent: q.name});
      g.style.background = q.bg;
      const li = h('li', {role: 'option', tabIndex: -1, className: i == val ? 'sel' : ''}, g);
      li.setAttribute('aria-selected', i == val);
      li.onclick = () => pick(i);
      li.onkeydown = e => {
        if (e.key == 'Enter' || e.key == ' ') { e.preventDefault(); pick(i); }
        else if (e.key == 'ArrowDown') { e.preventDefault(); (li.nextSibling || li).focus(); }
        else if (e.key == 'ArrowUp') { e.preventDefault(); (li.previousSibling || li).focus(); }
        else if (e.key == 'Escape' || e.key == 'Tab') { close(); btn.focus(); }
      };
      list.append(li);
    });
    wrap.append(list);
    btn.setAttribute('aria-expanded', 'true');
    document.addEventListener('pointerdown', outside, true);
    const cur = list.children[val] || list.firstChild;
    if (cur) { cur.focus(); cur.scrollIntoView({block: 'nearest'}); }
  };
  btn.onkeydown = e => { if ((e.key == 'ArrowDown' || e.key == 'ArrowUp') && !list) { e.preventDefault(); btn.click(); } };
  wrap.append(btn);
  return tip(h('div', {className: 'row'}, h('label', {textContent: label}), wrap), help);
}
function field(label, input, help) { return tip(h('div', {className: 'row'}, h('label', {textContent: label}), input), help); }
const button = (text, help, props = {}) => tip(h('button', Object.assign({textContent: text}, props)), help);
const heading = (text, help) => tip(h('h2', {textContent: text}), help);

// The settings from the board. When it can't be reached (restarting, network) this says so and
// tries again every 2 s, instead of leaving the page stuck.
const getJson = async u => { const r = await fetch(u); if (!r.ok) throw new Error(u + ': ' + r.status); return r.json(); };
let loading = null;  // one load at a time: a second ask waits for the one under way
function load() { return loading || (loading = loadOnce().finally(() => { loading = null; })); }
async function loadOnce() {
  for (;;) {
    try {
      const st = await getJson('/api/state');
      // The effects (names, ranges, help): cached by the browser under the table's fingerprint
      st.effects = await getJson('/api/effects?v=' + encodeURIComponent(st.effectsVersion || st.build));
      N = await getJson('/api/net');
      S = st;
      draw();
      return;
    } catch (e) {
      const el = S ? document.getElementById('status') : document.getElementById('app');
      if (el) el.textContent = 'Can’t reach beamLED (' + e.message + '), trying again…';
      await new Promise(res => setTimeout(res, 2000));
    }
  }
}

// The preset list changed on the board: take its state
// A reply with the whole state: keep the effects (fetched once per build, not in the reply)
const useState = st => { S = Object.assign(st, {effects: S.effects}); draw(); };
const presetList = o => post('/api/presets', o).then(useState);

// Palette lock: every preset plays one palette (live only, not saved)
function lockRow() {
  const lock = S.paletteLock == null ? -1 : S.paletteLock;
  const items = [{name: 'Each preset’s own', bg: 'linear-gradient(90deg,#3a3a3a,#5a5a5a)'}]
    .concat(S.palettes.map(q => ({name: q.name, bg: palGrad(q)})));
  return gradSelect('Palette lock', items, lock + 1, v => {
    S.paletteLock = v - 1; post('/api/set', {paletteLock: v - 1}); draw(); }, H.paletteLock);
}
// The Sparkle overlay: one for all presets
function sparkleRows(p, ef) {
  const sk = S.sparkle || {amount: 0, beat: 0, burst: false, colour: 0, step: 0};
  S.sparkle = sk;
  const send = o => post('/api/sparkle', o);
  const rows = [slider('Sparkle overlay', 0, 1, 0.01, sk.amount, v => v.toFixed(2),
    v => { sk.amount = v; liveSend('/beamled/sparkle', v, () => send({amount: v}), true); },
    H.sparkle + (ef.sparkle ? '' : ' (The playing effect is Sparkle itself: no overlay on it.)'), 'sparkle')];
  // Listed slow to fast; the codes are the board's (1..4 = 1, 1/2, 1/4, 1/8 beat; 5 = 2, 6 = 4 beats)
  const beatCodes = [0, 6, 5, 1, 2, 3, 4];
  rows.push(select('Sparkle timing', ['Free', '4 beats (1 bar)', '2 beats', '1 beat', '1/2 beat', '1/4 beat', '1/8 beat'],
    Math.max(0, beatCodes.indexOf(sk.beat || 0)), i => {
      sk.beat = beatCodes[i]; send({beat: sk.beat}); draw(); }, H.sparkleBeat));
  const burst = h('input', {type: 'checkbox', checked: !!sk.burst});
  burst.onchange = () => { sk.burst = burst.checked; send({burst: burst.checked}); };
  const bRow = field('Bursts', burst, H.sparkleBurst);
  if (!sk.beat) off(bRow, 'Bursts need a Sparkle timing other than Free: with Free, twinkles start at any moment, so there is no tick to flash on.');
  rows.push(bRow);
  const own = S.palettes[S.paletteLock >= 0 ? S.paletteLock : p.palette] || S.palettes[0];
  const items = [{name: 'White', bg: 'linear-gradient(90deg,#8a8a8a,#e0e0e0)'},
                 {name: 'Preset’s palette (now ' + own.name + ')', bg: palGrad(own)}]
    .concat(S.palettes.map(q => ({name: q.name, bg: palGrad(q)})));
  rows.push(gradSelect('Sparkle colour', items, sk.colour || 0, v => { sk.colour = v; send({colour: v}); draw(); }, H.sparkleColour));
  // On a cube curve: the left half gives the fine steps that matter (a hundred or more twinkles
  // start every second), the right end still reaches 0.5 and 1. The value itself is the step.
  const stepOf = q => { const v = q * q * q; return v < 0.01 ? Math.round(v / 0.00005) * 0.00005 : +v.toFixed(2); };
  const showStep = v => !v ? 'random' : v < 0.01 ? v.toFixed(5).replace(/0+$/, '') : v.toFixed(2);
  const stepRow = slider('Colour step', 0, 1, 0.005, Math.cbrt(sk.step || 0),
    q => showStep(stepOf(q)),
    q => { const v = stepOf(q); sk.step = v; liveSend('/beamled/sparkle_colour_step', v, () => send({step: v}), true); }, H.sparkleStep, 'sparkleStep');
  if (!sk.colour) off(stepRow, 'Sparkle colour is White: there is no palette to step along. Pick a palette above.');
  rows.push(stepRow);
  return rows;
}
function draw() {
  hideTip();
  sliders = {};
  const app = document.getElementById('app');
  app.replaceChildren();
  const p = S.presets[S.current], ef = S.effects[p.effect];

  const onb = button(S.on ? 'On' : 'Off', H.on, {className: S.on ? 'on' : ''});
  onb.onclick = () => { S.on = !S.on; post('/api/set', {on: S.on}); draw(); };
  app.append(h('div', {className: 'top'}, h('h1', {textContent: 'beamLED'}), onb));
  app.append(h('div', {className: 'conn', id: 'status'}));  // only when the board can't be reached
  if (S.configMode) app.append(h('div', {className: 'conn', textContent: 'Config mode: hotspot and this page only, the lights get no data. Restart (at the bottom) to run normally.'}));
  app.append(h('div', {className: 'card'},
    slider('Brightness', 0, 255, 1, S.brightness, v => Math.round(v / 2.55) + '%',
      v => { S.brightness = v; liveSend('/beamled/brightness', v / 255, () => post('/api/set', {brightness: v}), true); }, H.brightness, 'brightness'),
    lockRow(), ...sparkleRows(p, ef)));

  app.append(heading('Presets', H.presets));
  const g = h('div', {className: 'presets'});
  S.presets.forEach((q, i) => {
    const b = button((i + 1) + '  ' + q.name + (q.changed ? ' •' : ''), H.play + ' ' + (i + 1) + ': ' + q.name + ' (' + S.effects[q.effect].name + ').' + (q.changed ? ' It has changes that aren’t saved yet.' : ''),
      {className: (i == S.current ? 'on' : '') + (q.auto === false ? ' out' : '')});
    b.onclick = () => { S.current = i; S.on = true; post('/api/set', {current: i}); draw(); };
    g.append(b);
  });
  const add = button('+ Add', H.add, {className: 'add'});
  add.onclick = () => presetList({add: S.current});
  if (S.presets.length >= S.maxPresets) off(add, 'all ' + S.maxPresets + ' preset places are used; delete one first.');
  g.append(add);
  app.append(g);
  app.append(autoRow());

  app.append(heading('Preset ' + (S.current + 1), H.presets));
  const card = h('div', {className: 'card'});
  const nm = h('input', {type: 'text', value: p.name, maxLength: 19});
  nm.onchange = () => {
    if (p.savedName == null && nm.value != p.name && !p.fresh) p.savedName = p.name;
    if (nm.value == p.savedName) delete p.savedName;
    p.name = nm.value; post('/api/preset', {index: S.current, name: nm.value}); draw();
  };
  card.append(field('Name', nm, H.name));
  const earlier = button('◀ Earlier', H.earlier);
  earlier.onclick = () => presetList({move: S.current, to: S.current - 1});
  if (S.current == 0) off(earlier, 'this is already the first preset.');
  const later = button('Later ▶', H.later);
  later.onclick = () => presetList({move: S.current, to: S.current + 1});
  if (S.current == S.presets.length - 1) off(later, 'this is already the last preset.');
  const del = button('Delete', H.del);
  if (S.presets.length <= 1) off(del, 'it is the only preset, and the board always needs one.');
  let armed = null;
  del.onclick = () => {
    if (armed) { clearTimeout(armed); presetList({delete: S.current}); return; }
    del.textContent = 'Really delete?'; del.className = 'warn';
    armed = setTimeout(() => { armed = null; del.textContent = 'Delete'; del.className = ''; }, 3000);
  };
  card.append(h('div', {className: 'btns'}, earlier, later, del));
  const inAuto = h('input', {type: 'checkbox', checked: p.auto !== false});
  inAuto.onchange = () => { p.auto = inAuto.checked; post('/api/preset', {index: S.current, auto: inAuto.checked}); draw(); };
  card.append(field('In Auto', inAuto, H.inAuto));
  card.append(select('Effect', S.effects.map(e => e.name), p.effect, v =>
    post('/api/preset', {index: S.current, effect: v}).then(np => { S.presets[S.current] = np; draw(); }),
    H.effect + ' ' + ef.name + ': ' + ef.help));
  const locked = S.paletteLock >= 0 && S.palettes[S.paletteLock];
  card.append(paletteSelect('Palette', p.palette, v => {
    p.palette = v; post('/api/preset', {index: S.current, palette: v}); }, H.palette +
    (locked ? ' Now the Palette lock (top) plays ' + locked.name + ' instead; this choice is kept and plays again when the lock is off.' : '')));
  const mir = h('input', {type: 'checkbox', checked: !!p.mirror});
  mir.onchange = () => { p.mirror = mir.checked; markChanged(S.current); post('/api/preset', {index: S.current, mirror: mir.checked}); };
  card.append(field('Mirror', mir, H.mirror));
  // Timing: tempo and beat come from Ableton Link (or the board's own clock when it is disabled).
  // In beats, the effect's cycle (a band, a revolution, ...) takes 1, 2, 4 ... 64 beats and the
  // speed slider is replaced by that choice.
  // Beats is always in the list; for an effect with no cycle it can't be picked, and says why
  const modes = ['Time', 'Time, follow tempo', 'Beats'];
  const timing = ef.cycle ? p.timing : Math.min(p.timing, 1);
  const inBeats = timing == 2;
  const noCycle = ef.name + ' has no repeating cycle (a band, a revolution, …) to fit to the beat, so it can’t run in Beats.';
  const tRow = select('Timing', modes, timing, v => {
    p.timing = v; post('/api/preset', {index: S.current, timing: v}); draw(); }, H.timing + (ef.cycle ? '' : ' Beats is not available here: ' + noCycle));
  if (!ef.cycle) tRow.querySelector('option[value="2"]').disabled = true;
  card.append(tRow);
  const beats = [1, 2, 4, 8, 16, 32, 64].map(b => b + ' beat' + (b > 1 ? 's' : '') + (b >= 4 ? ' (' + b / 4 + ' bar' + (b > 4 ? 's' : '') + ')' : ''));
  const bRow = select('Beats per ' + (ef.cycle || 'cycle'), beats, p.beatsExp, v => {
    p.beatsExp = v; post('/api/preset', {index: S.current, beatsExp: v}); }, H.beats + (ef.cycle ? ' One cycle here: one ' + ef.cycle + '.' : ''));
  const rev = h('input', {type: 'checkbox', checked: p.reverse});
  rev.onchange = () => { p.reverse = rev.checked; post('/api/preset', {index: S.current, reverse: rev.checked}); };
  const rRow = field('Reverse', rev, H.reverse);
  if (!inBeats) for (const r of [bRow, rRow]) off(r, ef.cycle ? 'only with Timing set to Beats.' : noCycle);
  card.append(bRow, rRow);
  const motions = p.motions || [];
  ef.params.forEach((d, j) => {
    const by = motions.findIndex(mo => mo.target == j + 1);
    const row = slider(d.name, d.min, d.max, d.step, p.params[j],
      v => v.toFixed(decimals(d.step)),
      v => { p.params[j] = v; markChanged(S.current); liveSend(presetAddr(slug(d.name)), v, () => post('/api/preset', {index: S.current, params: p.params})); },
      d.help + ' (' + d.min + ' to ' + d.max + ', default ' + d.def + ')' +
        (by >= 0 ? ' Now moved by Motion ' + (by + 1) + ' (below): set its From and To there.' : ''), 'p' + j);
    if (by >= 0) { row.classList.add('moving'); off(row, 'Motion ' + (by + 1) + ' moves it: set its From and To there.'); }
    else if (inBeats && j == ef.speedParam) off(row, 'Timing is Beats: the speed comes from Beats per ' + ef.cycle + ' instead.');
    card.append(row);
  });
  card.append(slider('Beat pulse', 0, 1, 0.01, p.pulse, v => v.toFixed(2),
    v => { p.pulse = v; markChanged(S.current); liveSend(presetAddr('pulse'), v, () => post('/api/preset', {index: S.current, pulse: v})); }, H.pulse, 'pulse'));
  motions.forEach((mo, m) => card.append(motionEditor(p, ef, mo, m)));
  const reset = button('Reset parameters', H.reset);
  reset.onclick = () => { p.params = ef.params.map(d => d.def); p.changed = true; post('/api/preset', {index: S.current, params: p.params}); draw(); };
  const asNew = p.savedName != null;
  const save = button(asNew ? 'Save as new •' : p.changed ? 'Save •' : 'Saved', asNew ? H.saveAsNew : H.save,
    {className: p.changed || asNew ? 'on' : ''});
  if (!p.changed && !asNew) off(save, 'this preset has no unsaved changes.');
  save.onclick = () => post('/api/preset', {index: S.current, save: true}).then(np => {
    if (np.savedAs != null) { S.current = np.savedAs; load(); return; }  // a new preset: the list changed
    S.presets[S.current] = np; draw();
  });
  saveBtn = save;
  const revert = button('Revert', H.revert);
  if (!p.changed) off(revert, 'there are no unsaved changes to drop.');
  revertBtn = revert;
  revert.onclick = () => post('/api/preset', {index: S.current, revert: true}).then(np => { S.presets[S.current] = np; draw(); });
  card.append(h('div', {className: 'btns'}, save, revert, reset));
  app.append(card);

  app.append(heading('Palettes', H.palettes));
  const pc = h('div', {className: 'card'});
  S.palettes.forEach((q, i) => pc.append(paletteEditor(q, i)));
  {
    const add = button('+ Add palette', H.palAddPalette, {className: 'pal-add'});
    add.onclick = () => post('/api/palettes', {add: S.palettes.length - 1}).then(st => { S = Object.assign(st, {effects: S.effects}); draw(); });
    if (S.palettes.length >= (S.maxPalettes || 16)) off(add, 'all ' + (S.maxPalettes || 16) + ' palette places are used; delete one first.');
    pc.append(add);
  }
  app.append(pc);

  app.append(h('h2', {textContent: 'Output'}));
  const tgt = h('input', {type: 'text', value: S.target});
  tgt.onchange = () => post('/api/set', {target: tgt.value.trim()}).then(() => { S.target = tgt.value.trim(); });
  app.append(h('div', {className: 'card'},
    field('Art-Net to', tgt, H.target),
    slider('Gamma', 1, 3, 0.1, S.gamma, v => v.toFixed(1), v => { S.gamma = v; throttle('g', () => post('/api/set', {gamma: v})); }, H.gamma)));

  app.append(h('h2', {textContent: 'Tempo'}));
  app.append(linkCard());

  app.append(heading('OSC', H.osc));
  app.append(tip(h('div', {className: 'card'}, h('div', {className: 'hint', id: 'osc'})), H.osc));

  app.append(h('h2', {textContent: 'Layout'}));
  app.append(layoutCard());

  app.append(h('h2', {textContent: 'Network'}));
  app.append(networkCard());

  app.append(heading('Settings file', H.backup));
  app.append(backupCard());

  app.append(heading('Status', H.status));
  const stats = h('dl', {className: 'stats'});
  for (const [k, label] of [['frames', 'Frames'], ['temp', 'Temperature'], ['up', 'Running'], ['net', 'Network'], ['wifi', 'WiFi'], ['link', 'Ableton Link'], ['heap', 'Free memory']])
    stats.append(h('dt', {textContent: label}), h('dd', {id: 'st-' + k}));
  const rs = button('Restart', H.restart);
  let rsArmed = 0;
  rs.onclick = () => {
    if (Date.now() - rsArmed > 3000) {  // first tap asks, a second within 3 s restarts
      rsArmed = Date.now(); rs.textContent = 'Restart now?'; rs.className = 'warn';
      setTimeout(() => { if (Date.now() - rsArmed >= 3000) { rs.textContent = 'Restart'; rs.className = ''; } }, 3100);
      return;
    }
    rs.disabled = true; rs.textContent = 'Restarting…';
    post('/api/restart', {});
  };
  app.append(h('div', {className: 'card'}, stats, h('div', {className: 'btns'}, rs)));
  applyFolds(app);
  showStatus();
}

// Each section folds under its heading; this browser remembers which (a convenience: the page
// works the same without storage). The key is the heading without numbers ("Preset 3" -> "Preset").
const FOLD_KEY = 'beamled.folded';
let folded = {};
try { folded = JSON.parse(localStorage.getItem(FOLD_KEY) || '{}') || {}; } catch (e) { folded = {}; }
function applyFolds(app) {
  let key = null, head = null;
  for (const el of [...app.children]) {
    if (el.tagName == 'H2') {
      head = el;
      key = el.textContent.replace(/[0-9]+/g, '').trim();
      el.classList.toggle('folded', !!folded[key]);
      el.setAttribute('role', 'button');
      el.tabIndex = 0;
      el.setAttribute('aria-expanded', folded[key] ? 'false' : 'true');
      const k = key;
      const toggle = () => {
        folded[k] = !folded[k];
        if (!folded[k]) delete folded[k];
        try { localStorage.setItem(FOLD_KEY, JSON.stringify(folded)); } catch (e) {}
        applyFolds(app);
      };
      el.onclick = toggle;
      el.onkeydown = e => { if (e.key == 'Enter' || e.key == ' ') { e.preventDefault(); toggle(); } };
    } else if (key) {
      el.hidden = !!folded[key];
    }
  }
}

// ---- Motions: the same shapes as the board (store.cpp, motionShape)
const SHAPES = ['Sine', 'Linear', 'Ease in-out', 'Hold at ends', 'Overshoot', 'Bounce', 'Random slide', 'Min max', 'Random jump'];
// Random's value for half-cycle n of motion m: the board's hash (store.cpp, randomAt)
function randomAt(n, m) {
  let x = (Math.imul(n >>> 0, 2654435761) + Math.imul(m, 40503) + 12345) >>> 0;
  x = Math.imul(x ^ (x >>> 16), 0x7feb352d) >>> 0;
  x = Math.imul(x ^ (x >>> 15), 0x846ca68b) >>> 0;
  x = (x ^ (x >>> 16)) >>> 0;
  return (x >>> 8) / 16777216;
}
function motionShape(s, ph, cycle = 0, m = 0) {
  ph -= Math.floor(ph);
  const easeIO = x => x < 0.5 ? 4 * x * x * x : 1 - Math.pow(-2 * x + 2, 3) / 2;
  if (s == 7) return ph < 0.5 ? 0 : 1;
  if (s == 8) return randomAt((cycle * 2 + (ph < 0.5 ? 0 : 1)) >>> 0, m);
  if (s == 6) {
    const n = (cycle * 2 + (ph < 0.5 ? 0 : 1)) >>> 0, u = ph < 0.5 ? ph * 2 : ph * 2 - 1;
    const a = randomAt((n - 1) >>> 0, m), b = randomAt(n, m);
    return a + (b - a) * easeIO(u);
  }
  const bounceOut = t => { const n = 7.5625, d = 2.75;
    if (t < 1 / d) return n * t * t;
    if (t < 2 / d) { t -= 1.5 / d; return n * t * t + 0.75; }
    if (t < 2.5 / d) { t -= 2.25 / d; return n * t * t + 0.9375; }
    t -= 2.625 / d; return n * t * t + 0.984375; };
  const ease = x => x < 0.5 ? 4 * x * x * x : 1 - Math.pow(-2 * x + 2, 3) / 2;
  if (s == 0) return 0.5 - 0.5 * Math.cos(2 * Math.PI * ph);
  if (s == 5) return ph < 0.5 ? bounceOut(2 * ph) : 1 - bounceOut(2 * ph - 1);
  const x = ph < 0.5 ? 2 * ph : 2 - 2 * ph;
  if (s == 1) return x;
  if (s == 2) return ease(x);
  if (s == 3) return ease(Math.min(1, Math.max(0, (x - 0.15) / 0.7)));
  const c = 1.70158 * 1.525;
  return x < 0.5 ? (4 * x * x * ((c + 1) * 2 * x - c)) / 2 : ((2 * x - 2) * (2 * x - 2) * ((c + 1) * (2 * x - 2) + c) + 2) / 2;
}
// One there-and-back as a drawing: left = start, top = To, bottom = From
function curve(mo) {
  const ns = 'http://www.w3.org/2000/svg', svg = document.createElementNS(ns, 'svg');
  svg.setAttribute('viewBox', '0 0 200 44'); svg.setAttribute('preserveAspectRatio', 'none'); svg.classList.add('curve');
  let d = '';
  const span = mo.shape == 6 || mo.shape == 8 ? 3 : 1;  // Random slide and Random jump: three cycles, to show them wander
  for (let k = 0; k <= 200; k++) { const x = k / 200 * span + mo.phase; const y = motionShape(mo.shape, x, Math.floor(x), 0); d += (k ? 'L' : 'M') + k + ' ' + (36 - y * 28).toFixed(1); }
  for (const y of [8, 36]) { const l = document.createElementNS(ns, 'line'); l.setAttribute('x1', 0); l.setAttribute('x2', 200); l.setAttribute('y1', y); l.setAttribute('y2', y); svg.append(l); }
  const path = document.createElementNS(ns, 'path'); path.setAttribute('d', d); svg.append(path);
  return svg;
}
function motionEditor(p, ef, mo, m) {
  const send = o => post('/api/preset', Object.assign({index: S.current, motion: m}, o));
  const box = h('div', {className: 'motion'});
  box.append(select('Motion ' + (m + 1), ['Off'].concat(ef.params.map(d => d.name)), mo.target, v => {
    mo.target = v;
    if (v) { const d = ef.params[v - 1]; mo.from = d.min; mo.to = d.max; }
    send({target: v, from: mo.from, to: mo.to}); draw();
  }, H.motion));
  // A motion that is Off shows only its selector: picking a slider is what brings its settings
  if (!mo.target || mo.target > ef.params.length) return box;
  const d = ef.params[mo.target - 1], fmt = v => v.toFixed(decimals(d.step));
  const pic = h('div');
  const repaint = () => pic.replaceChildren(curve(mo));
  box.append(select('Shape', SHAPES, mo.shape, v => { mo.shape = v; send({shape: v}); repaint(); }, H.shape));
  box.append(select('Period in', ['Seconds', 'Beats'], mo.inBeats ? 1 : 0, v => { mo.inBeats = v == 1; send({inBeats: mo.inBeats}); draw(); }, H.periodIn));
  const secRow = slider('Period (s)', 0.5, 120, 0.5, mo.seconds, v => v.toFixed(1), v => { mo.seconds = v; throttle('ms' + m, () => send({seconds: v})); }, H.periodSec);
  const beats = [1, 2, 4, 8, 16, 32, 64].map(b => b + ' beat' + (b > 1 ? 's' : '') + (b >= 4 ? ' (' + b / 4 + ' bar' + (b > 4 ? 's' : '') + ')' : ''));
  const beatRow = select('Period (beats)', beats, mo.beatsExp, v => { mo.beatsExp = v; send({beatsExp: v}); }, H.periodBeats);
  off(mo.inBeats ? secRow : beatRow, 'Period in is ' + (mo.inBeats ? 'Beats: the period is set in beats below.' : 'Seconds: the period is set in seconds above.'));
  box.append(secRow, beatRow);
  box.append(slider('From', d.min, d.max, d.step, mo.from, fmt, v => { mo.from = v; throttle('mf' + m, () => send({from: v})); }, H.from));
  box.append(slider('To', d.min, d.max, d.step, mo.to, fmt, v => { mo.to = v; throttle('mt' + m, () => send({to: v})); }, H.to));
  box.append(slider('Phase', 0, 1, 0.01, mo.phase, v => v.toFixed(2), v => { mo.phase = v; repaint(); throttle('mp' + m, () => send({phase: v})); }, H.phase));
  repaint();
  box.append(tip(pic, H.shape));
  return box;
}
// The moved sliders follow their motions on screen: the board pushes each motion's phase and rate
// once a second with the status, and the page carries it on between pushes
let motionClock = {t: 0, m: []};
function animateMotions() {
  requestAnimationFrame(animateMotions);
  const p = S && S.presets[S.current];
  if (!p || !p.motions) return;
  const ef = S.effects[p.effect];
  p.motions.forEach((mo, m) => {
    if (!mo.target || mo.target > ef.params.length) return;
    const s = sliders['p' + (mo.target - 1)], clock = motionClock.m[m];
    if (!s || !clock) return;
    const d = ef.params[mo.target - 1];
    const t = clock[0] + (performance.now() - motionClock.t) / 1000 * clock[1];  // phase, carried on
    const cycle = (clock[2] || 0) + Math.floor(t);
    let v = Math.min(d.max, Math.max(d.min, mo.from + (mo.to - mo.from) * motionShape(mo.shape, t, cycle, m)));
    if (d.step >= 1) v = Math.round(v);
    s.r.value = v; s.o.textContent = s.fmt(v);
  });
}
requestAnimationFrame(animateMotions);

// Where each colour sits, 0..1: the palette's own, or evenly spaced
const evenStops = n => Array.from({length: n}, (_, j) => n > 1 ? j / (n - 1) : 0);
const isEven = s => s.every((v, j) => Math.abs(v - (s.length > 1 ? j / (s.length - 1) : 0)) < 1e-3);
function autoRow() {
  const A = S.auto || {};
  const send = o => { Object.assign(A, o); post('/api/auto', o); };
  const onb = button(A.enabled ? 'Auto on' : 'Auto off', H.auto, {className: A.enabled ? 'on' : ''});
  onb.onclick = () => { send({enabled: !A.enabled}); draw(); };
  const mode = tip(h('select'), H.autoEvery);
  ['Every … seconds', 'Every … bars'].forEach((l, k) => mode.append(h('option', {value: k, textContent: l, selected: (A.inBars ? 1 : 0) == k})));
  mode.onchange = () => { send({inBars: mode.value == 1}); draw(); };
  // Seconds and bars both stay on the page; the one not in use is greyed
  const bars = h('select');
  [1, 2, 4, 8, 16, 32, 64].forEach((b, k) => bars.append(h('option', {value: k, textContent: b + ' bar' + (b > 1 ? 's' : ''), selected: A.barsExp == k})));
  bars.onchange = () => send({barsExp: +bars.value});
  const secs = h('input', {type: 'number', min: 2, max: 3600, step: 1, value: A.seconds});
  secs.onchange = () => send({seconds: Math.max(2, Math.min(3600, +secs.value || 60))});
  const secsBox = tip(h('span', {}, secs, ' s'), H.autoEvery), barsBox = tip(h('span', {}, bars), H.autoEvery);
  off(A.inBars ? secsBox : barsBox, 'Auto counts in ' + (A.inBars ? 'bars: pick “Every … seconds” to use seconds.' : 'seconds: pick “Every … bars” to use bars.'));
  const amount = h('span', {}, secsBox, ' ', barsBox);
  const order = tip(h('select'), H.autoOrder);
  ['In order', 'Shuffle'].forEach((l, k) => order.append(h('option', {value: k, textContent: l, selected: (A.shuffle ? 1 : 0) == k})));
  order.onchange = () => send({shuffle: order.value == 1});
  return h('div', {className: 'auto'}, onb, mode, amount, order, h('span', {className: 'hint', id: 'autoLeft'}));
}

function paletteEditor(q, i) {
  if (!q.stops || q.stops.length != q.colors.length) q.stops = evenStops(q.colors.length);
  const box = h('div', {className: 'pal'});
  const sw = tip(h('div', {className: 'sw'}), H.palStops);
  const paint = () => sw.style.background = palGrad(q);
  const send = () => throttle('pal' + i, () => post('/api/palette', {index: i, name: q.name, colors: q.colors, stops: q.stops}));
  const nm = tip(h('input', {type: 'text', value: q.name, maxLength: 15}), H.palName);
  nm.onchange = () => { q.name = nm.value; send(); draw(); };
  box.append(nm);
  {
    const del = button('✕', H.palDelete, {className: 'del'});
    del.setAttribute('aria-label', 'Delete palette ' + q.name);
    let armed = null;
    del.onclick = () => {
      if (armed) { clearTimeout(armed); post('/api/palettes', {delete: i}).then(st => { S = Object.assign(st, {effects: S.effects}); draw(); }); return; }
      del.textContent = 'Delete?'; del.className = 'del warn';
      armed = setTimeout(() => { armed = null; del.textContent = '✕'; del.className = 'del'; }, 3000);
    };
    if (S.palettes.length <= 1) off(del, 'it is the only palette, and every preset needs one.');
    box.append(del);
  }
  // Click the bar to add a colour there, in the colour the palette already has at that point
  // (so nothing changes until it is picked). Up to 5 colours.
  const hex = c => [1, 3, 5].map(k => parseInt(c.substr(k, 2), 16));
  const colourAt = t => {
    let k = 0;
    while (k < q.stops.length - 1 && q.stops[k + 1] < t) k++;
    if (t <= q.stops[0]) return q.colors[0];
    if (k >= q.stops.length - 1) return q.colors[q.colors.length - 1];
    const a = hex(q.colors[k]), b = hex(q.colors[k + 1]), span = q.stops[k + 1] - q.stops[k];
    const f = span > 0 ? (t - q.stops[k]) / span : 0;
    return '#' + a.map((v, n) => Math.round(v + (b[n] - v) * f).toString(16).padStart(2, '0')).join('');
  };
  sw.onpointerdown = e => {
    if (e.target != sw || q.colors.length >= 7) return;
    const r = sw.getBoundingClientRect();
    const t = Math.round(Math.min(1, Math.max(0, (e.clientX - r.left) / r.width)) * 1000) / 1000;
    let j = 0;
    while (j < q.stops.length && q.stops[j] <= t) j++;
    q.colors.splice(j, 0, colourAt(t));
    q.stops.splice(j, 0, t);
    send(); markers();
  };
  // A colour goes when its marker is dragged off the bar (or with Delete); at least 2 stay
  const remove = j => {
    if (q.colors.length <= 2) return;
    q.colors.splice(j, 1); q.stops.splice(j, 1);
    send(); paint(); markers();
  };
  // The colour markers on the bar: tap one for its colour picker, drag it to move its colour
  // along the palette (kept between its neighbours)
  const markers = () => {
    sw.replaceChildren(...q.colors.map((c, j) => {
      const mk = tip(h('div', {className: 'stop', tabIndex: 0, role: 'button'}), H.palColour + ' (colour ' + (j + 1) + ' of ' + q.colors.length + ')');
      mk.setAttribute('aria-label', 'Colour ' + (j + 1) + ' of ' + q.colors.length);
      mk.style.left = (q.stops[j] * 100) + '%';
      mk.style.background = c;
      // The picker belongs to the marker and opens next to it; picking repaints in place, so
      // the open picker stays open
      const ci = h('input', {type: 'color', value: c, tabIndex: -1});
      ci.oninput = () => { q.colors[j] = ci.value; mk.style.background = ci.value; paint(); send(); };
      mk.append(ci);
      // Focused while open, so the page holds its redraws until the picker closes
      const pick = () => { ci.focus({preventScroll: true}); try { if (ci.showPicker) ci.showPicker(); else ci.click(); } catch (e) { ci.click(); } };
      mk.onkeydown = e => {
        if (e.key == 'Enter' || e.key == ' ') { e.preventDefault(); pick(); }
        else if (e.key == 'Delete' || e.key == 'Backspace') { e.preventDefault(); remove(j); }
      };
      mk.onpointerdown = e => {
        if (e.target == ci) return;
        e.preventDefault(); mk.setPointerCapture(e.pointerId);
        const x0 = e.clientX, y0 = e.clientY;
        let moved = false, outside = false;
        mk.onpointermove = ev => {
          if (!moved && Math.abs(ev.clientX - x0) < 4 && Math.abs(ev.clientY - y0) < 4) return;  // a tap so far
          moved = true; mk.classList.add('drag');
          const r = sw.getBoundingClientRect();
          // Pulled well away from the bar: let go there to remove this colour
          outside = q.colors.length > 2 && (ev.clientY > r.bottom + 36 || ev.clientY < r.top - 36);
          mk.classList.toggle('off', outside);
          const lo = j > 0 ? q.stops[j - 1] : 0, hi = j < q.stops.length - 1 ? q.stops[j + 1] : 1;
          q.stops[j] = Math.round(Math.min(hi, Math.max(lo, (ev.clientX - r.left) / r.width)) * 1000) / 1000;
          mk.style.left = (q.stops[j] * 100) + '%'; paint(); send();
        };
        mk.onpointerup = () => {
          mk.onpointermove = null; mk.classList.remove('drag');
          if (outside) remove(j);
          else if (!moved) pick();
        };
      };
      return mk;
    }));
  };
  paint();
  markers();
  box.append(sw);
  return box;
}

function layoutCard() {
  const L = S.layout;
  const num = (v, step) => h('input', {type: 'number', value: v, step, min: 0});
  const nx = num(L.nx, 1), ny = num(L.ny, 1), len = num(L.length, 0.01), asp = num(L.aspect, 0.1);
  const meas = h('input', {type: 'checkbox', checked: L.measured});
  const info = h('div', {className: 'hint'});
  // Start universe per row; a row uses ceil(LEDs per row / 170) universes from there
  const uni = L.rowUni.slice();
  const ORDERS = ['RGB', 'RBG', 'GRB', 'GBR', 'BRG', 'BGR'];
  const ord = (L.rowOrder || []).slice();
  const orderSel = (v, onch) => {
    const s = tip(h('select', {className: 'ord'}), H.rowOrder);
    ORDERS.forEach((o, k) => s.append(h('option', {value: k, textContent: o, selected: (v || 0) == k})));
    s.onchange = () => onch(+s.value);
    return s;
  };
  const upr = () => Math.max(1, Math.ceil(+nx.value / 170));
  const grid = tip(h('div', {className: 'unis'}), H.rowUni);
  const table = () => {
    const rows = Math.max(1, Math.min(64, +ny.value || 1));
    while (uni.length < rows) uni.push(uni.length ? uni[uni.length - 1] + upr() : 0);
    while (ord.length < rows) ord.push(0);
    grid.replaceChildren();
    for (let b = 0; b < rows; b++) {
      const range = h('span', {className: 'hint'});
      const showRange = () => range.textContent = upr() > 1 ? '–' + (uni[b] + upr() - 1) : '';
      const inp = h('input', {type: 'number', min: 0, max: 32767, step: 1, value: uni[b]});
      inp.oninput = () => { uni[b] = Math.max(0, Math.round(+inp.value)); showRange(); upd(); };
      showRange();
      grid.append(h('label', {}, h('span', {textContent: 'Row ' + (b + 1)}), inp, range, orderSel(ord[b], v => { ord[b] = v; })));
    }
  };
  const fillFrom = num(uni[0] || 0, 1);
  const fill = button('Fill', H.fill);
  fill.onclick = () => { for (let b = 0; b < uni.length; b++) uni[b] = +fillFrom.value + b * upr(); table(); upd(); };
  const allOrd = orderSel(0, v => { for (let b = 0; b < ord.length; b++) ord[b] = v; table(); });
  const upd = () => {
    const pitch = +len.value / Math.max(1, +nx.value), leds = +nx.value * +ny.value;
    const m = meas.checked && +ny.value == L.measuredRows;
    const used = uni.slice(0, +ny.value), dup = new Set();
    used.forEach((u, i) => used.forEach((v, j) => { if (i != j && u < v + upr() && v < u + upr()) dup.add(i + 1); }));
    info.textContent = 'LED spacing ' + (pitch * 1000).toFixed(1) + ' mm, row spacing ' +
      (m ? 'measured (ceiling)' : (pitch * +asp.value).toFixed(3) + ' m') + ' · ' + leds + ' LEDs' +
      (leds > L.maxLeds ? ' (max ' + L.maxLeds + ')' : '') + ' · ' + upr() + ' universe' + (upr() > 1 ? 's' : '') +
      ' per row' + (dup.size ? ' · overlapping universes: rows ' + [...dup].join(', ') : '');
  };
  nx.oninput = () => { table(); upd(); };
  ny.oninput = () => { table(); upd(); };
  [len, asp, meas].forEach(e => e.oninput = upd);
  const apply = button('Apply', H.apply, {className: 'on'});
  const msg = h('div', {className: 'hint'});
  apply.onclick = () => post('/api/layout', {nx: +nx.value, ny: +ny.value, length: +len.value, aspect: +asp.value,
      measured: meas.checked, rowUni: uni.slice(0, +ny.value), rowOrder: ord.slice(0, +ny.value)})
    .then(useState).catch(e => { msg.textContent = e.message; });
  table();
  upd();
  return h('div', {className: 'card'},
    field('LEDs per row', nx, H.nx), field('Rows', ny, H.ny), field('Row length (m)', len, H.len),
    field('Aspect', asp, H.aspect), h('div', {className: 'row'}, h('label'), h('span', {className: 'hint', textContent: 'row spacing ÷ LED spacing'})),
    field('Measured rows', meas, H.measured),
    tip(h('div', {className: 'row'}, h('label', {textContent: 'Row universes'}), h('span', {className: 'hint', textContent: 'consecutive from'}), fillFrom, fill), H.rowUni),
    tip(h('div', {className: 'row'}, h('label', {textContent: 'Colour order'}), h('span', {className: 'hint', textContent: 'all rows'}), allOrd), H.rowOrder),
    grid, info, h('div', {className: 'row'}, h('label'), apply), msg);
}

function networkCard() {
  const name = h('input', {type: 'text', value: N.name, maxLength: 31});
  const fb = h('input', {type: 'text', value: N.fallback, placeholder: 'automatic: ' + N.autoFallback});
  const ap = h('input', {type: 'password', placeholder: N.apPassSet ? 'saved (leave empty to keep, - to remove)' : 'open hotspot'});
  const msg = h('div', {className: 'hint'});
  const save = button('Save and restart', H.netSave, {className: 'on'});
  save.onclick = () => post('/api/net', {name: name.value, fallback: fb.value, apPass: ap.value})
    .then(() => { msg.textContent = 'Restarting... reconnect at http://' + name.value + '.local in a few seconds.'; })
    .catch(e => { msg.textContent = e.message; });
  return h('div', {className: 'card'},
    field('Name', name, H.netName), field('Fallback IP', fb, H.fallback), field('Hotspot password', ap, H.apPass),
    h('div', {className: 'row'}, h('label'), save), msg,
    h('p', {className: 'hint', textContent: 'Ethernet only: DHCP first; with no answer after 10 s the fallback IP is used. No WiFi in normal running. Hold the button on the board while it powers up for config mode: a hotspot named after the board with this page at http://192.168.4.1, and no light output until the next restart.'}));
}

// Settings file: download everything as one file, or load one (it replaces everything, then the
// board restarts). The file holds the hotspot password: keep it like a password.
function backupCard() {
  const msg = h('div', {className: 'hint'});
  const dl = button('Download settings file', H.backup);
  dl.onclick = async () => {
    try {
      const r = await fetch('/api/backup');
      if (!r.ok) throw new Error(await r.text());
      const text = await r.text();
      const day = new Date().toISOString().slice(0, 10);
      const a = h('a', {href: URL.createObjectURL(new Blob([text], {type: 'application/json'})),
                        download: (N.name || 'beamled') + '-settings-' + day + '.json'});
      document.body.append(a); a.click(); a.remove();
      setTimeout(() => URL.revokeObjectURL(a.href), 5000);
      msg.textContent = 'Saved. The file holds the hotspot password.';
    } catch (e) { msg.textContent = 'Could not download: ' + e.message; }
  };
  const file = h('input', {type: 'file', accept: '.json,application/json', id: 'settingsFile'});
  const load = button('Load and restart', 'Replaces everything on this board with the file, saves it and restarts. Tap twice.');
  const noFile = 'choose a beamLED settings file first.';
  off(load, noFile);
  let text = null, armed = 0;
  file.onchange = async () => {
    text = null; load.textContent = 'Load and restart'; load.className = ''; off(load, noFile);
    const f = file.files && file.files[0];
    if (!f) { msg.textContent = ''; return; }
    try {
      const t = await f.text(), d = JSON.parse(t);
      if (d.beamled != 1 || !Array.isArray(d.presets) || !Array.isArray(d.palettes)) throw new Error('not a beamLED settings file');
      const o = d.output || {}, n = d.network || {}, y = d.layout || {};
      msg.textContent = d.presets.length + ' presets, ' + d.palettes.length + ' palettes' +
        (y.ny ? ', ' + y.ny + ' rows of ' + y.nx + ' LEDs' : '') +
        (o.target ? ', Art-Net to ' + o.target : '') +
        (n.fallback != null ? ', fallback IP ' + (n.fallback || 'automatic') : '') +
        '. Loading replaces everything on this board.';
      text = t; on(load);
    } catch (e) { msg.textContent = 'Can’t use this file: ' + e.message; off(load, 'that file can’t be used (the message says why); choose another.'); }
  };
  load.onclick = async () => {
    if (!text) return;
    if (Date.now() - armed > 3000) {  // first tap asks, a second within 3 s loads
      armed = Date.now(); load.textContent = 'Load now?'; load.className = 'warn';
      setTimeout(() => { if (Date.now() - armed >= 3000) { load.textContent = 'Load and restart'; load.className = ''; } }, 3100);
      return;
    }
    load.disabled = true; load.textContent = 'Loading…';
    try {
      const r = await fetch('/api/restore', {method: 'POST', body: text});
      if (!r.ok) throw new Error(await r.text());
      msg.textContent = 'Loaded. The board restarts; this page reconnects by itself.';
    } catch (e) { msg.textContent = 'Not loaded: ' + e.message; load.disabled = false; load.textContent = 'Load and restart'; load.className = ''; }
  };
  return h('div', {className: 'card'},
    h('div', {className: 'btns'}, dl),
    h('div', {className: 'row'}, h('label', {textContent: 'Load a file', htmlFor: 'settingsFile'}), file),
    h('div', {className: 'btns'}, load), msg);
}

// Ableton's guidelines: name it "Ableton Link" in full, its state as Enabled / Disabled
function linkCard() {
  const L = S.link;
  const btn = button(L.enabled ? 'Enabled' : 'Disabled', H.link, {className: L.enabled ? 'on' : ''});
  btn.onclick = () => { L.enabled = !L.enabled; post('/api/link', {enabled: L.enabled}).then(st => { Object.assign(S, st); draw(); }); };
  const bpm = h('input', {type: 'number', min: 20, max: 999, step: 0.1, value: L.bpm, id: 'bpm'});
  bpm.onchange = () => post('/api/link', {bpm: +bpm.value});
  return h('div', {className: 'card'},
    tip(h('div', {className: 'row'}, h('label', {textContent: 'Ableton Link'}), btn, h('span', {className: 'hint', id: 'peers'})), H.link),
    tip(h('div', {className: 'row'}, h('label', {textContent: 'BPM'}), bpm,
      h('span', {className: 'hint', textContent: 'with Ableton Link enabled, this sets everyone’s tempo'})), H.bpm),
    slider('Latency (ms)', 0, 200, 1, S.linkLatency, v => v.toFixed(0),
      v => { S.linkLatency = v; throttle('lat', () => post('/api/link', {latency: v})); }, H.latency),
    h('p', {className: 'hint', textContent: 'Ableton and Ableton Link are trademarks of Ableton AG.'}));
}

function showStatus() {
  const L = S.link || {};
  const st = (k, t) => { const e = document.getElementById('st-' + k); if (e) e.textContent = t; };
  st('frames', S.fps + ' fps · render ' + S.renderMs + ' ms');
  st('temp', S.tempC != null ? Math.round(S.tempC) + ' °C (chip)' : '');
  st('net', S.net || '');
  st('wifi', S.wifi || '');
  st('link', !L.enabled ? 'Disabled' : (L.peers > 0 ? L.peers + ' peer' + (L.peers == 1 ? '' : 's') : 'no peers') + ' · ' + (L.bpm || 120) + ' BPM');
  if (S.uptime != null) {
    const u = S.uptime, d = Math.floor(u / 86400), hr = Math.floor(u / 3600) % 24, mi = Math.floor(u / 60) % 60;
    st('up', (d ? d + ' d ' : '') + (d || hr ? hr + ' h ' : '') + mi + ' min · started by ' + (S.resetReason || '?'));
  }
  st('heap', S.heap != null ? Math.round(S.heap / 1024) + ' KB' : '');
  const pe = document.getElementById('peers');
  if (pe) pe.textContent = L.enabled ? (L.peers > 0 ? L.peers + ' peer' + (L.peers == 1 ? '' : 's') : 'no peers') : '';
  const b = document.getElementById('bpm');
  if (b && document.activeElement != b) b.value = L.bpm;
  const al = document.getElementById('autoLeft');
  if (al) al.textContent = S.autoLeft >= 0 ? 'next in ' + (S.auto && S.auto.inBars ? Math.ceil(S.autoLeft) + ' bar' + (Math.ceil(S.autoLeft) == 1 ? '' : 's') : Math.ceil(S.autoLeft) + ' s') : '';
  const o = document.getElementById('osc');
  if (o) o.textContent = S.osc || '';
}

// ---- live: no polling. The board pushes over a WebSocket on its HTTP server (OSCQuery's too): every
// change of a value as OSC (whoever made it: MIMa, the button, another browser), and as text
// its status once a second, a change counter for what OSC doesn't carry (names, the preset
// list, layout: the page reloads).
let held = false, redrawDue = false, reloadDue = false;
// While a slider is dragged or a text field has the cursor, redraws wait
addEventListener('pointerdown', () => { held = true; }, true);
addEventListener('pointerup', () => { held = false; setTimeout(settle, 0); }, true);
addEventListener('focusout', () => setTimeout(settle, 0), true);
const editing = () => held || (document.activeElement && /^(INPUT|SELECT|TEXTAREA)$/.test(document.activeElement.tagName) &&
  document.activeElement.type != 'range' && document.activeElement.type != 'checkbox');
function settle() {
  if (editing()) return;
  if (reloadDue) { reloadDue = false; redrawDue = false; load(); }
  else if (redrawDue) { redrawDue = false; draw(); }
}
const redraw = () => { redrawDue = true; settle(); };
function moveSlider(key, v) {
  const s = sliders[key];
  if (!s) return redraw();
  if (held && document.activeElement == s.r) return;  // being dragged here
  s.r.value = v; s.o.textContent = s.fmt(v);
}
// A parameter's OSC name, as the board makes it: lower case up to any "(unit)", spaces as _
const slug = n => n.split('(')[0].toLowerCase().replace(/[^a-z0-9 ]/g, '').trim().replace(/ +/g, '_');

function oscMessages(buf, out = []) {
  const u8 = new Uint8Array(buf), dv = new DataView(buf);
  let p = 0;
  const str = () => { let e = p; while (e < u8.length && u8[e]) e++; const s = String.fromCharCode(...u8.subarray(p, e)); p = (e + 4) & ~3; return s; };
  const addr = str();
  if (addr == '#bundle') {
    p = 16;
    while (p + 4 <= u8.length) { const n = dv.getUint32(p); out.push(...oscMessages(buf.slice(p + 4, p + 4 + n))); p += 4 + n; }
    return out;
  }
  const tags = str(), args = [];
  for (const t of tags.slice(1)) {
    if (t == 'f') { args.push(dv.getFloat32(p)); p += 4; }
    else if (t == 'i') { args.push(dv.getInt32(p)); p += 4; }
    else if (t == 'r') { args.push('#' + (dv.getUint32(p) >>> 8).toString(16).padStart(6, '0')); p += 4; }
    else if (t == 'T' || t == 'F') args.push(t == 'T');
  }
  out.push([addr, args[0]]);
  return out;
}

function oscValue(addr, v) {
  const a = addr.split('/');
  if (a[1] != 'beamled' || !S) return;
  if (a.length == 3) {
    if (a[2] == 'brightness') { S.brightness = Math.round(v * 255); moveSlider('brightness', S.brightness); }
    else if (a[2] == 'on') { if (S.on != v) { S.on = v; redraw(); } }
    else if (a[2] == 'preset') { if (S.current != v - 1) { S.current = v - 1; redraw(); } }
    else if (a[2] == 'link') { if (S.link.enabled != v) { S.link.enabled = v; redraw(); } }
    else if (a[2] == 'auto') { if (S.auto && S.auto.enabled != v) { S.auto.enabled = v; redraw(); } }
    else if (S.sparkle && a[2] == 'sparkle') { S.sparkle.amount = v; moveSlider('sparkle', v); }
    else if (S.sparkle && a[2] == 'sparkle_timing') { if (S.sparkle.beat != v) { S.sparkle.beat = v; redraw(); } }
    else if (S.sparkle && a[2] == 'sparkle_burst') { if (S.sparkle.burst != !!v) { S.sparkle.burst = !!v; redraw(); } }
    else if (S.sparkle && a[2] == 'sparkle_colour') { if (S.sparkle.colour != v) { S.sparkle.colour = v; redraw(); } }
    else if (a[2] == 'palette') { if (S.paletteLock != v - 1) { S.paletteLock = v - 1; redraw(); } }
    else if (S.sparkle && a[2] == 'sparkle_colour_step') { S.sparkle.step = v; moveSlider('sparkleStep', Math.cbrt(v)); }
    return;
  }
  const n = +a[3] - 1, key = a[4];
  if (a[2] == 'presets') {
    const p = S.presets[n];
    if (!p || key == 'play') return;
    const mine = n == S.current;
    const mm = /^motion(\d)_(\w+)$/.exec(key);
    if (mm) {  // a motion's setting
      const mo = p.motions && p.motions[+mm[1] - 1];
      if (!mo) return;
      if (mm[2] == 'beats') { mo.inBeats = v > 0; if (v > 0) mo.beatsExp = Math.round(Math.log2(v)); }
      else mo[mm[2]] = v;
      if (mine) redraw();
      return;
    }
    if (key == 'effect') { if (p.effect != v - 1) reload(); return; }
    if (key == 'auto') { if (p.auto !== v) { p.auto = v; redraw(); } return; }
    if (key == 'palette') p.palette = v - 1;
    else if (key == 'pulse') { p.pulse = v; if (mine) return moveSlider('pulse', v); }
    else if (key == 'mirror') { p.mirror = !!v; draw(); }
    else if (key == 'timing') p.timing = v;
    else if (key == 'beats') p.beatsExp = Math.round(Math.log2(v));
    else if (key == 'reverse') p.reverse = v;
    else {
      const j = S.effects[p.effect].params.findIndex(d => slug(d.name) == key);
      if (j < 0) return;
      p.params[j] = v;
      if (mine) return moveSlider('p' + j, v);
    }
    if (mine) redraw();
  } else if (a[2] == 'palettes') {
    const q = S.palettes[n];
    if (!q) return;
    if (key == 'colours') { if (q.colors.length != v) reload(); return; }
    if (key.startsWith('position')) {
      const j = +key.slice(8) - 1;
      if (q.stops && j < q.stops.length && Math.abs(q.stops[j] - v) > 1e-4) { q.stops[j] = v; redraw(); }
      return;
    }
    const j = +key.slice(6) - 1;
    if (j < q.colors.length && q.colors[j].toLowerCase() != String(v).toLowerCase()) { q.colors[j] = v; redraw(); }  // not our own echo
  }
}
const reload = () => { reloadDue = true; settle(); };

// One page at a time. This page's id lasts as long as the page, so reconnecting (after a board
// restart, say) isn't "another page". When another page has beamLED, this one asks before taking
// over; a page that loses it to another shows the same question and lets go of its connection.
const pageId = Math.random().toString(36).slice(2, 12) + Date.now().toString(36);
let sock = null, away = false;
function elsewhere(text) {
  away = true;
  hideTip();
  document.getElementById('elsewhere-text').textContent = text;
  document.getElementById('elsewhere').style.display = 'flex';
}
document.getElementById('elsewhere-go').onclick = () => {
  document.getElementById('elsewhere').style.display = 'none';
  away = false;
  load().then(() => live(true));
};

function live(take = false) {
  const ws = sock = new WebSocket('ws://' + location.host + '/');
  ws.binaryType = 'arraybuffer';
  ws.onopen = () => ws.send(JSON.stringify({COMMAND: 'PAGE', ID: pageId, TAKE: take}));
  ws.onmessage = e => {
    if (typeof e.data != 'string') { for (const [addr, v] of oscMessages(e.data)) oscValue(addr, v); return; }
    const m = JSON.parse(e.data);
    if (m.COMMAND == 'BUSY') {
      elsewhere('beamLED is open in another window or on another device. Continue to use it here?');
      ws.close();
    } else if (m.COMMAND == 'TAKEN') {
      elsewhere('beamLED is now open in another window or on another device. Continue to use it here?');
      ws.close();
    } else if (m.COMMAND == 'STATUS') {
      if (!ws.accepted) { ws.accepted = true; ws.send(JSON.stringify({COMMAND: 'LISTEN', DATA: '/'})); }
      const st = m.DATA;
      if (!S) return;
      if (st.rev != S.rev || st.presetCount != S.presets.length) { S.rev = st.rev; reload(); }
      let changed = st.current != S.current || st.on != S.on;
      // Unsaved edits per preset (one bit each): the • marks and the Save button follow
      S.presets.forEach((q, i) => { const c = !!(st.changed >>> i & 1); if (q.changed != c) { q.changed = c; changed = true; } });
      motionClock = {t: performance.now(), m: st.motion || []};
      Object.assign(S, st);
      if (changed) redraw(); else showStatus();
    } else if (m.COMMAND == 'PATH_ADDED' || m.COMMAND == 'PATH_REMOVED') {
      reload();
    }
  };
  // The board restarted or the network dropped: connect again (and reload, things may differ).
  // Not when another page has it: this one waits for "Continue here".
  ws.onclose = () => {
    if (away || ws != sock) return;
    setTimeout(() => { if (away) return; reloadDue = true; settle(); live(); }, 3000);
  };
}
load().then(live);
</script></body></html>)HTML";
