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
#pv{display:block;margin:0 auto;border-radius:6px;background:#000}
.card.pv-card{background:#000}
.card.pv-card.pinned{position:sticky;top:4px;z-index:5;box-shadow:0 6px 14px #000d}
.pv-bar{display:flex;align-items:center;gap:10px}
.pv-bar .row{flex:1}
.pv-bar .row label{flex:0 0 auto}
.pv-pin{background:none;border:1px solid transparent;font-size:18px;line-height:1;padding:4px 7px;filter:grayscale(1);opacity:.45}
.pv-pin.on{filter:none;opacity:1;border-color:var(--acc);background:#2a2a2a}
.pal .mid{position:absolute;top:19px;width:10px;height:10px;margin-left:-5px;background:#bbb;border:1px solid #000;transform:rotate(45deg);cursor:ew-resize;touch-action:none}
.pal .mid.set{background:var(--acc)}
.pal .mid:hover,.pal .mid.drag{background:#fff}
.pal .mid:focus-visible{outline:2px solid var(--acc);outline-offset:2px}
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
.seq{display:grid;column-gap:2px;row-gap:4px;margin:6px 0 4px;user-select:none;touch-action:none}
.seq .ln{font-size:12px;color:var(--dim);align-self:center}
.seq .bt{font-size:10px;color:var(--dim);text-align:left}
.seq .st{height:22px;border-radius:3px;background:#2f2f2f;cursor:pointer}
.seq .st.q{background:#3a3a3a}
.seq .st.on{background:var(--acc)}
.na .st,.seq .st.na{pointer-events:none}

#elsewhere{position:fixed;inset:0;z-index:20;display:none;align-items:center;justify-content:center;background:#121212e8;padding:16px}
#elsewhere .card{max-width:360px;text-align:center}
#elsewhere p{margin:6px 0 14px}
#tip{position:fixed;z-index:9;display:none;bottom:0;box-sizing:border-box;max-height:40vh;overflow:auto;background:#2b2b2b;color:#eee;border-top:2px solid var(--acc);border-radius:10px 10px 0 0;box-shadow:0 -4px 18px #000a;padding:10px 16px calc(10px + env(safe-area-inset-bottom));font-size:14px;line-height:1.45;pointer-events:none}
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
  el.dataset.tip = (el.dataset.tip0 ? el.dataset.tip0 + ' ' : '') + 'Unavailable: ' + why;
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
// The hint shows at the bottom of the screen, as wide as the page's column, so it never covers
// the control being used
function showTip(el) {
  tipFor = el; tipBox.textContent = el.dataset.tip; tipBox.style.display = 'block';
  // as wide as the panels: the page column without its side padding
  const app = document.getElementById('app'), m = app.getBoundingClientRect(), cs = getComputedStyle(app);
  const left = Math.max(0, m.left + parseFloat(cs.paddingLeft)), right = Math.min(innerWidth, m.right - parseFloat(cs.paddingRight));
  tipBox.style.left = left + 'px';
  tipBox.style.width = Math.max(0, right - left) + 'px';
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
  on: 'Lights on or off, with a 1 s fade. Same as a long press of the button.',
  brightness: 'Master brightness, after the gamma curve (50% looks half as bright). Saved.',
  backup: 'Everything in one file: presets, palettes, Auto, Link, output, layout, network. Load it to set up this or another board.',
  rowOrder: 'Colour order of the row’s LEDs: RGB for most, GRB for many strips. Then Apply.',
  restart: 'Restart: tap twice. Lights out for a few seconds. Hold the button at power-up for config mode (hotspot and this page only).',
  status: 'Frames per second (target 40) and draw time (budget 25 ms). Temperature: uncalibrated, 60–70 °C is normal. Running: uptime and start reason (brown-out, watchdog or crash mean trouble). Network, WiFi, Link, free memory.',
  auto: 'Presets change by themselves, every so many seconds or bars. Only presets with In Auto take part. Picking a preset restarts the count; waits while the lights are off. Button: double press on, short press off.',
  autoEvery: 'How often Auto changes preset: seconds, or bars on the beat grid.',
  autoOrder: 'In order, or Shuffle (random, never the same twice in a row).',
  inAuto: 'Auto plays this preset. Saved at once.',
  presets: 'Click to play. The button steps through them in this order (short: next, double: Auto on, long: on/off). Up to 32. • = unsaved changes.',
  play: 'Play this preset (short fade).',
  add: 'New preset at the end: a copy of the one playing.',
  name: 'Preset name, up to 19 characters.',
  earlier: 'Move one place earlier.',
  later: 'Move one place later.',
  del: 'Delete: click twice within 3 s. The last preset stays.',
  effect: 'Changing it resets the sliders to the new effect’s defaults.',
  palette: 'Colours from the Palettes list. Editing a palette changes every preset using it.',
  timing: 'Time: speeds in their own units. Follow tempo: the speed is set for 120 BPM and scales with the tempo. Beats: one cycle takes a whole number of beats, on the bar (effects with a cycle only).',
  beats: 'Beats per cycle; 4 = one bar. A change eases onto the new grid.',
  reverse: 'Runs the cycle backwards (Beats timing).',
  sparkle: 'Twinkles over every preset, kept across preset changes. 0 = none, 1 = about 8% of the LEDs lit.',
  sparkleBeat: 'When twinkles start: Free (any moment) or on the beat grid, every 4 beats down to 1/8 beat.',
  mirror: 'Mirrors top to bottom.',
  mirrorX: 'Mirrors left to right.',
  sparkleBurst: 'Off: single twinkles. On: a batch flashes on each tick and fades before the next.',
  paletteLock: 'Every preset plays this palette instead of its own. Not saved.',
  preview: 'Live view of the LEDs, up to 15 frames/s, bars at their real spacing, first row on the left. No gamma. Sent only while open and on screen.',
  previewPin: 'Pin (📌): keeps the preview at the top while you scroll. Kept in this browser.',
  previewLook: 'On: bars as diffused strips. Off: bare LEDs with glare. Kept in this browser.',
  steps: 'A set step flashes that third of every bar: Top, Center, Bottom. Beats: 32 steps, a 1/16 note each (8 beats per pattern). Frequency: only the first column, which thirds flash. Click or drag to set. Lighter boxes are beats.',
  stepsClear: 'Clear all steps (Revert brings them back).',
  effectBrightness: 'Dims the effect, not the Sparkle overlay. On Strobe: only the Background. Saved.',
  sparkleBrightness: 'How strongly the twinkles cover the effect: 0 = not at all, 100% = fully. Saved.',
  sparkleTime: 'Seconds a twinkle lasts (with Through the palette: one run through it). Same amount lit at any time.',
  sparkleThrough: 'Each twinkle runs through the palette, first colour to last (black, black, red: dark, pops red, fades).',
  sparkleStep: '0 = random colours. Above 0: each twinkle this far along the palette from the last. ~0.0005 drifts slowly, 0.01 already looks mixed, 0.5 alternates opposites. Fine on the left, coarse on the right.',
  sparkleColour: 'White, the playing preset’s palette, or any palette. Twinkles replace what is under them: black gives dark twinkles.',
  pulse: 'Brightness accent: full on each beat, dipping between, strongest on the bar. 0 = steady.',
  reset: 'Sliders back to the effect’s defaults (not saved).',
  save: 'Store this preset as it plays now. Unsaved changes play but are lost on a power cut.',
  revert: 'Back to the saved preset.',
  motion: 'Moves one slider between From and To by itself. The slider shows ↔. Up to three per preset.',
  shape: 'Sine: smooth. Linear: steady, sharp turns. Ease in-out: slow at the ends. Hold at ends: rests at each end. Overshoot: a little past each end. Bounce. Random slide: glides to random values. Min max: From, then To, no transition. Random jump: jumps to random values. Sawtooth: From to To, then straight back (Angle −180 to 180: endless turn).',
  periodIn: 'Seconds, or beats (on the beat grid, at From on the bar).',
  periodBeats: 'Beats for one there-and-back (Sawtooth: one run); 4 = one bar.',
  periodSec: 'Seconds for one there-and-back (Sawtooth: one run).',
  from: 'One end, in the slider’s units.',
  to: 'The other end. Below From runs the other way.',
  phase: 'Where in its cycle: 0 = at From, 0.5 = at To. Offsets one motion from another.',
  palettes: 'Shared palettes: each preset picks one. Looping effects wrap from the last colour to the first.',
  palName: 'Palette name, up to 15 characters.',
  palColour: 'Tap to change, drag along to move, drag off (or Delete) to remove.',
  palMid: 'Where the blend is half-way. Double-click (or Home): straight blend. Arrow keys nudge.',
  palStops: 'Click the bar to add a colour (up to 7). Drag markers to move them, off the bar to remove (2 stay).',
  palAddPalette: 'New palette at the end: a copy of the last one. Up to 16.',
  palDelete: 'Delete: click twice within 3 s. Its presets switch to the first palette.',
  saveAsNew: 'Renamed: Save makes a new preset with this name; this one goes back to its saved state.',
  target: 'Art-Net destination: the pixel controller, a visualiser PC, or a broadcast (x.x.x.255 or 255.255.255.255). Cable only.',
  gamma: 'Brightness curve. 2.2 suits most LEDs; lower = brighter darks, higher = deeper darks.',
  link: 'Enabled: tempo and beat shared with Ableton Live and other Link apps on the network. Disabled: own tempo.',
  bpm: 'Tempo. With Link peers it changes everyone’s tempo.',
  latency: 'Frames are drawn this far ahead so beats land on time. Raise it if the lights trail the music.',
  osc: 'OSCQuery at beamled.local:80, OSC in on port 8000. The live controls plus slider1…slider8 (the playing preset’s sliders, 0–1).',
  nx: 'LEDs per row.',
  ny: 'Rows.',
  len: 'Row length in metres.',
  rowSpacing: 'Distance between rows in metres.',
  measured: 'Use the uneven row positions built into the firmware (when the row count matches).',
  rowUni: 'Start universe of each row. Over 170 LEDs a row continues into the next universe.',
  fill: 'Number the rows one after another from this universe.',
  apply: 'Apply and save. A new LED count restarts the board (click twice within 4 s).',
  netName: 'Board name: <name>.local, hotspot and update name. After a restart.',
  fallback: 'Address used when no DHCP answers within 10 s. Empty: automatic 169.254.x.x.',
  apPass: 'Config-mode hotspot password, 8+ characters. Empty: open. - removes a saved one.',
  netSave: 'Save and restart (lights out for a few seconds).',
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
const hexRgb = h => [1, 3, 5].map(i => parseInt(h.slice(i, i + 2), 16));
const palGrad = q => {
  const st = q.stops && q.stops.length == q.colors.length ? q.stops : evenStops(q.colors.length);
  const mids = q.mids && q.mids.length == q.colors.length - 1 ? q.mids : [];
  const parts = [];
  q.colors.forEach((c, j) => {
    parts.push(c + ' ' + (st[j] * 100).toFixed(1) + '%');
    // A midpoint: the board's curve. Above the middle that is a CSS colour hint's (f^k); below it
    // its mirror image (1 - (1-f)^k), which CSS can't draw: there, colour stops along the curve
    const m = mids[j];
    if (m != null && Math.abs(m - 0.5) > 1e-3 && j < q.colors.length - 1) {
      if (m > 0.5) parts.push(((st[j] + m * (st[j + 1] - st[j])) * 100).toFixed(1) + '%');
      else {
        const A = hexRgb(c), B = hexRgb(q.colors[j + 1]), k = Math.log(0.5) / Math.log(1 - m);
        for (let s = 1; s < 12; s++) {
          const f = s / 12, b = 1 - Math.pow(1 - f, k);
          const rgb = A.map((v, n) => Math.round(v + (B[n] - v) * b));
          parts.push('rgb(' + rgb.join(',') + ') ' + ((st[j] + f * (st[j + 1] - st[j])) * 100).toFixed(2) + '%');
        }
      }
    }
  });
  return 'linear-gradient(90deg,' + parts.join(',') + ')';
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
  const sk = S.sparkle || {amount: 0, beat: 0, burst: false, colour: 0, step: 0, through: false, time: 1.2};
  S.sparkle = sk;
  const send = o => post('/api/sparkle', o);
  const rows = [slider('Sparkle overlay', 0, 1, 0.01, sk.amount, v => v.toFixed(2),
    v => { sk.amount = v; liveSend('/beamled/sparkle', v, () => send({amount: v}), true); },
    H.sparkle + (ef.sparkle ? '' : ' (The playing effect is Sparkle itself: no overlay on it.)'), 'sparkle')];
  rows.push(slider('Sparkle brightness', 0, 255, 1, S.sparkleBrightness == null ? 255 : S.sparkleBrightness, v => Math.round(v / 2.55) + '%',
    v => { S.sparkleBrightness = v; liveSend('/beamled/sparkle_brightness', v / 255, () => post('/api/set', {sparkleBrightness: v}), true); }, H.sparkleBrightness, 'sparkleBrightness'));
  const timeRow = slider('Twinkle time (s)', 0.2, 5, 0.1, sk.time || 1.2, v => v.toFixed(1),
    v => { sk.time = v; liveSend('/beamled/sparkle_time', v, () => send({time: v}), true); }, H.sparkleTime, 'sparkleTime');
  if (sk.beat && sk.burst) off(timeRow, 'bursts last until the next tick.');
  rows.push(timeRow);
  // Listed slow to fast; the codes are the board's (1..4 = 1, 1/2, 1/4, 1/8 beat; 5 = 2, 6 = 4 beats)
  const beatCodes = [0, 6, 5, 1, 2, 3, 4];
  rows.push(select('Sparkle timing', ['Free', '4 beats (1 bar)', '2 beats', '1 beat', '1/2 beat', '1/4 beat', '1/8 beat'],
    Math.max(0, beatCodes.indexOf(sk.beat || 0)), i => {
      sk.beat = beatCodes[i]; send({beat: sk.beat}); draw(); }, H.sparkleBeat));
  const burst = h('input', {type: 'checkbox', checked: !!sk.burst});
  burst.onchange = () => { sk.burst = burst.checked; send({burst: burst.checked}); draw(); };
  const bRow = field('Bursts', burst, H.sparkleBurst);
  if (!sk.beat) off(bRow, 'needs a Sparkle timing other than Free.');
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
  const thr = h('input', {type: 'checkbox', checked: !!sk.through});
  thr.onchange = () => { sk.through = thr.checked; send({through: thr.checked}); draw(); };
  const thrRow = field('Through the palette', thr, H.sparkleThrough);
  if (!sk.colour) off(thrRow, 'Sparkle colour is White.');
  rows.push(thrRow);
  if (!sk.colour) off(stepRow, 'Sparkle colour is White.');
  else if (sk.through) off(stepRow, 'Through the palette is on.');
  rows.push(stepRow);
  return rows;
}
function draw() {
  hideTip();
  sliders = {};
  const app = document.getElementById('app');
  app.replaceChildren();
  const p = S.presets[S.current], ef = p && S.effects[p.effect];
  if (!ef) { reload(); return; }  // the lists changed under this page: fetch them again

  const onb = button(S.on ? 'On' : 'Off', H.on, {className: S.on ? 'on' : ''});
  onb.onclick = () => { S.on = !S.on; post('/api/set', {on: S.on}); draw(); };
  app.append(h('div', {className: 'top'}, h('h1', {textContent: 'beamLED'}), onb));
  app.append(h('div', {className: 'conn', id: 'status'}));  // only when the board can't be reached
  if (S.configMode) app.append(h('div', {className: 'conn', textContent: 'Config mode: hotspot and this page only, the lights get no data. Restart (at the bottom) to run normally.'}));

  app.append(heading('Preview', H.preview));
  const diffuse = h('input', {type: 'checkbox', checked: !pvBare});
  diffuse.onchange = () => {
    pvBare = !diffuse.checked;
    try { localStorage.setItem('beamled.pvlook', pvBare ? 'bare' : 'diffused'); } catch (e) {}
  };
  const look = field('Diffuse', diffuse, H.previewLook);
  const pin = button('📌', H.previewPin, {className: 'pv-pin' + (pvPinned ? ' on' : '')});
  pin.setAttribute('aria-label', pvPinned ? 'Unpin the preview' : 'Pin the preview');
  pin.setAttribute('aria-pressed', pvPinned ? 'true' : 'false');
  pin.onclick = () => {
    pvPinned = !pvPinned;
    try { localStorage.setItem('beamled.pvpin', pvPinned ? '1' : ''); } catch (e) {}
    draw();
  };
  app.append(h('div', {className: 'card pv-card' + (pvPinned ? ' pinned' : '')}, h('canvas', {id: 'pv'}),
    h('div', {className: 'pv-bar'}, look, pin)));

  app.append(h('div', {className: 'card'},
    slider('Brightness', 0, 255, 1, S.brightness, v => Math.round(v / 2.55) + '%',
      v => { S.brightness = v; liveSend('/beamled/brightness', v / 255, () => post('/api/set', {brightness: v}), true); }, H.brightness, 'brightness'),
    slider('Effect brightness', 0, 255, 1, S.effectBrightness == null ? 255 : S.effectBrightness, v => Math.round(v / 2.55) + '%',
      v => { S.effectBrightness = v; liveSend('/beamled/effect_brightness', v / 255, () => post('/api/set', {effectBrightness: v}), true); }, H.effectBrightness, 'effectBrightness'),
    lockRow(), ...sparkleRows(p, ef)));
  app.lastChild.dataset.nofold = '';  // the controls under the preview: not part of its section


  app.append(heading('Presets', H.presets));
  const g = h('div', {className: 'presets'});
  S.presets.forEach((q, i) => {
    const b = button((i + 1) + '  ' + q.name + (q.changed ? ' •' : ''), q.name + ': ' + S.effects[q.effect].name + (q.changed ? ', unsaved changes' : '') + '.',
      {className: (i == S.current ? 'on' : '') + (q.auto === false ? ' out' : '')});
    b.onclick = () => { S.current = i; S.on = true; post('/api/set', {current: i}); draw(); };
    g.append(b);
  });
  const add = button('+ Add', H.add, {className: 'add'});
  add.onclick = () => presetList({add: S.current});
  if (S.presets.length >= S.maxPresets) off(add, 'all ' + S.maxPresets + ' presets used.');
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
  if (S.current == 0) off(earlier, 'already first.');
  const later = button('Later ▶', H.later);
  later.onclick = () => presetList({move: S.current, to: S.current + 1});
  if (S.current == S.presets.length - 1) off(later, 'already last.');
  const del = button('Delete', H.del);
  if (S.presets.length <= 1) off(del, 'the only preset.');
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
    ef.name + ': ' + ef.help + ' ' + H.effect));
  const locked = S.paletteLock >= 0 && S.palettes[S.paletteLock];
  card.append(paletteSelect('Palette', p.palette, v => {
    p.palette = v; post('/api/preset', {index: S.current, palette: v}); }, H.palette +
    (locked ? ' Now the Palette lock (top) plays ' + locked.name + ' instead; this choice is kept and plays again when the lock is off.' : '')));
  const mirX = h('input', {type: 'checkbox', checked: !!p.mirrorX});
  mirX.onchange = () => { p.mirrorX = mirX.checked; markChanged(S.current); post('/api/preset', {index: S.current, mirrorX: mirX.checked}); };
  card.append(field('Mirror X', mirX, H.mirrorX));
  const mir = h('input', {type: 'checkbox', checked: !!p.mirror});
  mir.onchange = () => { p.mirror = mir.checked; markChanged(S.current); post('/api/preset', {index: S.current, mirror: mir.checked}); };
  card.append(field('Mirror Y', mir, H.mirror));
  // Timing: tempo and beat come from Ableton Link (or the board's own clock when it is disabled).
  // In beats, the effect's cycle (a band, a revolution, ...) takes 1, 2, 4 ... 64 beats and the
  // speed slider is replaced by that choice.
  // Beats is always in the list; for an effect with no cycle it can't be picked, and says why
  const modes = ef.steps ? ['Frequency', 'Frequency, follow tempo', 'Beats'] : ['Time', 'Time, follow tempo', 'Beats'];
  const timing = ef.cycle ? p.timing : Math.min(p.timing, 1);
  const inBeats = timing == 2;
  const noCycle = ef.name + ' has no cycle to fit to the beat.';
  const tRow = select('Timing', modes, timing, v => {
    p.timing = v; post('/api/preset', {index: S.current, timing: v}); draw(); }, H.timing + (ef.cycle ? '' : ' No Beats: ' + noCycle));
  if (!ef.cycle) tRow.querySelector('option[value="2"]').disabled = true;
  card.append(tRow);
  const beats = [1, 2, 4, 8, 16, 32, 64].map(b => b + ' beat' + (b > 1 ? 's' : '') + (b >= 4 ? ' (' + b / 4 + ' bar' + (b > 4 ? 's' : '') + ')' : ''));
  const bRow = select('Beats per ' + (ef.cycle || 'cycle'), beats, p.beatsExp, v => {
    p.beatsExp = v; post('/api/preset', {index: S.current, beatsExp: v}); }, H.beats + (ef.cycle ? ' Here a cycle is one ' + ef.cycle + '.' : ''));
  const rev = h('input', {type: 'checkbox', checked: p.reverse});
  rev.onchange = () => { p.reverse = rev.checked; post('/api/preset', {index: S.current, reverse: rev.checked}); };
  const rRow = field('Reverse', rev, H.reverse);
  if (!inBeats) for (const r of [bRow, rRow]) off(r, ef.cycle ? 'Timing is not Beats.' : noCycle);
  card.append(bRow, rRow);
  if (ef.steps) card.appendChild(stepGrid(p, !inBeats));
  const motions = p.motions || [];
  const rows = [], moved = j => motions.some(mo => mo.target == j + 1);
  ef.params.forEach((d, j) => {
    const by = motions.findIndex(mo => mo.target == j + 1);
    const row = slider(d.name, d.min, d.max, d.step, p.params[j],
      v => v.toFixed(decimals(d.step)),
      v => { p.params[j] = v; paramDeps(); markChanged(S.current); liveSend('/beamled/slider' + (j + 1), (v - d.min) / (d.max - d.min), () => post('/api/preset', {index: S.current, params: p.params})); },
      d.help + ' (' + d.min + ' to ' + d.max + ', default ' + d.def + ')' +
        (by >= 0 ? ' Animation ' + (by + 1) + ' moves it.' : ''), 'p' + j);
    if (by >= 0) { row.classList.add('moving'); off(row, 'Animation ' + (by + 1) + ' moves it.'); }
    else if (inBeats && j == ef.speedParam) off(row, 'Timing is Beats: Beats per ' + ef.cycle + ' sets the speed.');
    rows[j] = row;
    card.append(row);
  });
  // Sliders that only count for one setting of a switch slider (Radial): greyed while it is the
  // other way. A switch moved by a motion can be either, so nothing is greyed then.
  const rad = ef.params.findIndex(d => d.name == 'Radial'), arm = ef.params.findIndex(d => d.name == 'Arms');
  paramDeps = () => {
    ef.params.forEach((d, j) => {
      if (moved(j)) return;  // greyed for its motion already
      let why = null, rule = false;
      if (rad >= 0 && (/^Centre [XY]$/.test(d.name) || /^Angle/.test(d.name))) {  // Gradient
        rule = true;
        const radial = p.params[rad] >= 0.5;
        if (!moved(rad) && !radial && d.name.startsWith('Centre')) why = 'Radial is off.';
        if (!moved(rad) && radial && d.name.startsWith('Angle')) why = 'Radial is on.';
      }
      if (arm >= 0 && (d.name == 'Weave' || d.name.startsWith('Turn'))) {  // Fibres
        rule = true;
        if (!moved(arm) && p.params[arm] < 0.5) why = 'Arms is 0.';
      }
      if (!rule) return;
      if (why) off(rows[j], why); else on(rows[j]);
    });
  };
  paramDeps();
  card.append(slider('Beat pulse', 0, 1, 0.01, p.pulse, v => v.toFixed(2),
    v => { p.pulse = v; markChanged(S.current); throttle('pulse', () => post('/api/preset', {index: S.current, pulse: v})); }, H.pulse, 'pulse'));
  motions.forEach((mo, m) => card.append(motionEditor(p, ef, mo, m)));
  const reset = button('Reset parameters', H.reset);
  reset.onclick = () => { p.params = ef.params.map(d => d.def); p.changed = true; post('/api/preset', {index: S.current, params: p.params}); draw(); };
  const asNew = p.savedName != null;
  const save = button(asNew ? 'Save as new •' : p.changed ? 'Save •' : 'Saved', asNew ? H.saveAsNew : H.save,
    {className: p.changed || asNew ? 'on' : ''});
  if (!p.changed && !asNew) off(save, 'no changes.');
  save.onclick = () => post('/api/preset', {index: S.current, save: true}).then(np => {
    if (np.savedAs != null) { S.current = np.savedAs; load(); return; }  // a new preset: the list changed
    S.presets[S.current] = np; draw();
  });
  saveBtn = save;
  const revert = button('Revert', H.revert);
  if (!p.changed) off(revert, 'no changes.');
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
    if (S.palettes.length >= (S.maxPalettes || 16)) off(add, 'all ' + (S.maxPalettes || 16) + ' palettes used.');
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
  previewSync();
}

// Each section folds under its heading; this browser remembers which (a convenience: the page
// works the same without storage). The key is the heading without numbers ("Preset 3" -> "Preset").
const FOLD_KEY = 'beamled.folded';
let folded = {};
try { folded = JSON.parse(localStorage.getItem(FOLD_KEY) || '{}') || {}; } catch (e) { folded = {}; }
function applyFolds(app) {
  let key = null, head = null;
  for (const el of [...app.children]) {
    if (el.dataset.nofold != null) { key = null; head = null; continue; }  // ends the section above it
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
        previewSync();
      };
      el.onclick = toggle;
      el.onkeydown = e => { if (e.key == 'Enter' || e.key == ' ') { e.preventDefault(); toggle(); } };
    } else if (key) {
      el.hidden = !!folded[key];
    }
  }
}

// ---- Motions: the same shapes as the board (store.cpp, motionShape)
const SHAPES = ['Sine', 'Linear', 'Ease in-out', 'Hold at ends', 'Overshoot', 'Bounce', 'Random slide', 'Min max', 'Random jump', 'Sawtooth'];
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
  if (s == 9) return ph;  // Sawtooth: From to To, then straight back
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
  // Random slide and Random jump: three cycles, to show them wander; Sawtooth: two, to show the jump back
  const span = mo.shape == 6 || mo.shape == 8 ? 3 : (mo.shape == 9 ? 2 : 1);
  for (let k = 0; k <= 200; k++) { const x = k / 200 * span + mo.phase; const y = motionShape(mo.shape, x, Math.floor(x), 0); d += (k ? 'L' : 'M') + k + ' ' + (36 - y * 28).toFixed(1); }
  for (const y of [8, 36]) { const l = document.createElementNS(ns, 'line'); l.setAttribute('x1', 0); l.setAttribute('x2', 200); l.setAttribute('y1', y); l.setAttribute('y2', y); svg.append(l); }
  const path = document.createElementNS(ns, 'path'); path.setAttribute('d', d); svg.append(path);
  return svg;
}
function motionEditor(p, ef, mo, m) {
  const send = o => post('/api/preset', Object.assign({index: S.current, motion: m}, o));
  const box = h('div', {className: 'motion'});
  box.append(select('Animation ' + (m + 1), ['Off'].concat(ef.params.map(d => d.name)), mo.target, v => {
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
  off(mo.inBeats ? secRow : beatRow, 'Period in is ' + (mo.inBeats ? 'Beats.' : 'Seconds.'));
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
  off(A.inBars ? secsBox : barsBox, 'Auto counts in ' + (A.inBars ? 'bars.' : 'seconds.'));
  const amount = h('span', {}, secsBox, ' ', barsBox);
  const order = tip(h('select'), H.autoOrder);
  ['In order', 'Shuffle'].forEach((l, k) => order.append(h('option', {value: k, textContent: l, selected: (A.shuffle ? 1 : 0) == k})));
  order.onchange = () => send({shuffle: order.value == 1});
  return h('div', {className: 'auto'}, onb, mode, amount, order, h('span', {className: 'hint', id: 'autoLeft'}));
}

function paletteEditor(q, i) {
  if (!q.stops || q.stops.length != q.colors.length) q.stops = evenStops(q.colors.length);
  if (!q.mids || q.mids.length != q.colors.length - 1) q.mids = Array(q.colors.length - 1).fill(0.5);
  const box = h('div', {className: 'pal'});
  const sw = tip(h('div', {className: 'sw'}), H.palStops);
  const paint = () => sw.style.background = palGrad(q);
  const send = () => throttle('pal' + i, () => post('/api/palette', {index: i, name: q.name, colors: q.colors, stops: q.stops, mids: q.mids}));
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
    if (S.palettes.length <= 1) off(del, 'the only palette.');
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
    let f = span > 0 ? (t - q.stops[k]) / span : 0;
    const m = q.mids[k];
    if (m != null && Math.abs(m - 0.5) > 1e-3 && f > 0 && f < 1) f = Math.pow(f, Math.log(0.5) / Math.log(m));
    return '#' + a.map((v, n) => Math.round(v + (b[n] - v) * f).toString(16).padStart(2, '0')).join('');
  };
  sw.onpointerdown = e => {
    if (e.target != sw || q.colors.length >= 7) return;
    const r = sw.getBoundingClientRect();
    const t = Math.round(Math.min(1, Math.max(0, (e.clientX - r.left) / r.width)) * 1000) / 1000;
    let j = 0;
    while (j < q.stops.length && q.stops[j] <= t) j++;
    const n0 = q.colors.length;
    q.colors.splice(j, 0, colourAt(t));
    q.stops.splice(j, 0, t);
    // the stretch it lands in splits in two, each blended straight
    if (j == 0) q.mids.unshift(0.5); else if (j >= n0) q.mids.push(0.5); else q.mids.splice(j - 1, 1, 0.5, 0.5);
    send(); markers();
  };
  // A colour goes when its marker is dragged off the bar (or with Delete); at least 2 stay
  const remove = j => {
    if (q.colors.length <= 2) return;
    const n0 = q.colors.length;
    q.colors.splice(j, 1); q.stops.splice(j, 1);
    // its two stretches become one, blended straight
    if (j == 0) q.mids.shift(); else if (j == n0 - 1) q.mids.pop(); else q.mids.splice(j - 1, 2, 0.5);
    send(); paint(); markers();
  };
  // The colour markers on the bar: tap one for its colour picker, drag it to move its colour
  // along the palette (kept between its neighbours)
  // The midpoint diamonds under the bar, one per stretch between two colours
  const midHandle = j => {
    const at = () => (q.stops[j] + q.mids[j] * (q.stops[j + 1] - q.stops[j])) * 100 + '%';
    const d = tip(h('div', {className: 'mid' + (Math.abs(q.mids[j] - 0.5) > 1e-3 ? ' set' : ''), tabIndex: 0, role: 'slider'}),
      H.palMid + ' (between colours ' + (j + 1) + ' and ' + (j + 2) + ': ' + q.mids[j].toFixed(2) + ')');
    d.setAttribute('aria-label', 'Midpoint between colours ' + (j + 1) + ' and ' + (j + 2));
    d.style.left = at();
    const set = m => {
      q.mids[j] = Math.round(Math.min(0.98, Math.max(0.02, m)) * 100) / 100;
      d.style.left = at(); d.classList.toggle('set', Math.abs(q.mids[j] - 0.5) > 1e-3); paint(); send();
    };
    d.ondblclick = () => set(0.5);
    d.onkeydown = e => {
      if (e.key == 'ArrowLeft' || e.key == 'ArrowRight') { e.preventDefault(); set(q.mids[j] + (e.key == 'ArrowRight' ? 0.02 : -0.02)); }
      else if (e.key == 'Home') { e.preventDefault(); set(0.5); }
    };
    d.onpointerdown = e => {
      e.preventDefault(); e.stopPropagation(); d.setPointerCapture(e.pointerId); d.classList.add('drag');
      d.onpointermove = ev => {
        const r = sw.getBoundingClientRect(), span = q.stops[j + 1] - q.stops[j];
        if (span <= 0) return;
        set(((ev.clientX - r.left) / r.width - q.stops[j]) / span);
      };
      d.onpointerup = () => { d.onpointermove = null; d.classList.remove('drag'); };
    };
    return d;
  };
  const markers = () => {
    // The diamonds, kept so a dragged colour takes its two neighbours along (each stays the same
    // share of its stretch, between its two colours)
    const mh = q.colors.slice(0, -1).map((c, j) => midHandle(j));
    const follow = k => { if (mh[k]) mh[k].style.left = (q.stops[k] + q.mids[k] * (q.stops[k + 1] - q.stops[k])) * 100 + '%'; };
    sw.replaceChildren(...mh, ...q.colors.map((c, j) => {
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
          mk.style.left = (q.stops[j] * 100) + '%'; follow(j - 1); follow(j); paint(); send();
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
  const nx = num(L.nx, 1), ny = num(L.ny, 1), len = num(L.length, 0.01);
  // Row spacing in metres; the board keeps it as row spacing ÷ LED spacing (aspect)
  const sp = num(+(L.aspect * L.length / Math.max(1, L.nx)).toFixed(3), 0.001);
  const aspect = () => +sp.value / (+len.value / Math.max(1, +nx.value));
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
      (m ? 'measured' : (+sp.value).toFixed(3) + ' m') + ' · ' + leds + ' LEDs' +
      (leds > L.maxLeds ? ' (max ' + L.maxLeds + ')' : '') + ' · ' + upr() + ' universe' + (upr() > 1 ? 's' : '') +
      ' per row' + (dup.size ? ' · overlapping universes: rows ' + [...dup].join(', ') : '');
  };
  nx.oninput = () => { table(); upd(); };
  ny.oninput = () => { table(); upd(); };
  [len, sp, meas].forEach(e => e.oninput = upd);
  const apply = button('Apply', H.apply, {className: 'on'});
  const msg = h('div', {className: 'hint'});
  // A new LED count restarts the board: the first click warns, a second within 4 s applies
  let apArmed = 0;
  const disarm = () => { apArmed = 0; apply.textContent = 'Apply'; apply.className = 'on'; msg.textContent = ''; };
  apply.onclick = () => {
    const resize = +nx.value != L.nx || +ny.value != L.ny;
    if (resize && Date.now() - apArmed > 4000) {
      apArmed = Date.now();
      apply.textContent = 'Restart and apply?'; apply.className = 'warn';
      msg.textContent = 'A new number of LEDs (' + L.nx * L.ny + ' → ' + (+nx.value * +ny.value) +
        ') restarts the board: the lights go dark for a few seconds. Click again to go ahead.';
      setTimeout(() => { if (apArmed && Date.now() - apArmed >= 4000) disarm(); }, 4100);
      return;
    }
    apArmed = 0;
    post('/api/layout', {nx: +nx.value, ny: +ny.value, length: +len.value, aspect: aspect(),
      measured: meas.checked, rowUni: uni.slice(0, +ny.value), rowOrder: ord.slice(0, +ny.value)})
    .then(r => {
      if (!r || !r.restarting) return useState(r);
      apply.disabled = true;  // another LED count: the board restarts with it (this page reconnects)
      apply.textContent = 'Restarting…';
      msg.textContent = 'Restarting with ' + (+nx.value * +ny.value) + ' LEDs… the page reconnects by itself.';
    }).catch(e => { msg.textContent = e.message; });
  };
  table();
  upd();
  return h('div', {className: 'card'},
    field('LEDs per row', nx, H.nx), field('Rows', ny, H.ny), field('Row length (m)', len, H.len),
    field('Row spacing (m)', sp, H.rowSpacing),
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
  const noFile = 'no file chosen.';
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
    } catch (e) { msg.textContent = 'Can’t use this file: ' + e.message; off(load, 'unusable file.'); }
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

// The step pattern of a preset whose effect has steps (Strobe): 4 lanes x 32 steps, each lane a
// number with bit s for step s. Click to toggle a box; dragging paints the same way across boxes.
const LANES = ['Top', 'Center', 'Bottom'];
let seqPaint = null;  // while dragging over a step grid: true = setting boxes, false = clearing
addEventListener('pointerup', () => { seqPaint = null; });
addEventListener('pointercancel', () => { seqPaint = null; });
// firstOnly (Frequency timing): only the first column counts, the rest is greyed
function stepGrid(p, firstOnly) {
  if (!Array.isArray(p.steps)) p.steps = [0, 0, 0, 0];
  const send = () => throttle('steps', () => post('/api/preset', {index: S.current, steps: p.steps}));
  const grid = h('div', {className: 'seq'});
  const set = (k, b, box, v) => {
    const bit = (1 << b) >>> 0;
    p.steps[k] = (v ? (p.steps[k] | bit) : (p.steps[k] & ~bit)) >>> 0;
    box.classList.toggle('on', v);
    send();
  };
  // Every step the same width; the gaps between beats and between the bars are columns of their own
  const cols = ['3.6em'];
  for (let b = 0; b < 32; b++) { if (b && b % 4 == 0) cols.push(b == 16 ? '6px' : '2px'); cols.push('1fr'); }
  grid.style.gridTemplateColumns = cols.join(' ');
  const at = (el, row, b) => { el.style.gridRow = row; el.style.gridColumn = b < 0 ? 1 : 2 + b + (b >> 2); return el; };
  for (let b = 0; b < 32; b += 4)
    grid.append(at(h('span', {className: 'bt', textContent: (b / 4) % 4 + 1}), 1, b));
  LANES.forEach((name, k) => {
    grid.append(at(h('span', {className: 'ln', textContent: name}), k + 2, -1));
    for (let b = 0; b < 32; b++) {
      const box = at(h('span', {className: 'st' + (b % 4 == 0 ? ' q' : '') + ((p.steps[k] >>> b) & 1 ? ' on' : '') + (firstOnly && b ? ' na' : '')}), k + 2, b);
      if (firstOnly && b) box.dataset.tip = 'Unavailable: Timing is Frequency: only the first column (which thirds flash) counts.';
      box.onpointerdown = e => {
        e.preventDefault();
        seqPaint = !box.classList.contains('on');
        set(k, b, box, seqPaint);
        try { box.releasePointerCapture(e.pointerId); } catch (x) {}  // touch: let the drag reach other boxes
      };
      box.onpointerenter = () => { if (seqPaint != null && box.classList.contains('on') != seqPaint) set(k, b, box, seqPaint); };
      grid.append(box);
    }
  });
  const clear = button('Clear pattern', H.stepsClear);
  clear.onclick = () => { p.steps = [0, 0, 0, 0]; post('/api/preset', {index: S.current, steps: p.steps}); draw(); };
  return h('div', {}, tip(grid, H.steps), h('div', {className: 'row'}, h('label'), clear));
}
let paramDeps = () => {};  // the playing preset's slider dependencies (greying), set by its card
function oscValue(addr, v) {
  const a = addr.split('/');
  if (a[1] != 'beamled' || !S) return;
  if (a.length == 3) {
    if (a[2] == 'brightness') { S.brightness = Math.round(v * 255); moveSlider('brightness', S.brightness); }
    else if (a[2] == 'effect_brightness') { S.effectBrightness = Math.round(v * 255); moveSlider('effectBrightness', S.effectBrightness); }
    else if (a[2] == 'sparkle_brightness') { S.sparkleBrightness = Math.round(v * 255); moveSlider('sparkleBrightness', S.sparkleBrightness); }
    else if (a[2] == 'on') { if (S.on != v) { S.on = v; redraw(); } }
    else if (a[2] == 'preset') {
      // a preset this page doesn't have yet (one was just added): fetch the list first
      if (v - 1 >= S.presets.length) reload();
      else if (S.current != v - 1) { S.current = v - 1; redraw(); }
    }
    else if (a[2] == 'link') { if (S.link.enabled != v) { S.link.enabled = v; redraw(); } }
    else if (a[2] == 'auto') { if (S.auto && S.auto.enabled != v) { S.auto.enabled = v; redraw(); } }
    else if (S.sparkle && a[2] == 'sparkle') { S.sparkle.amount = v; moveSlider('sparkle', v); }
    else if (S.sparkle && a[2] == 'sparkle_timing') { if (S.sparkle.beat != v) { S.sparkle.beat = v; redraw(); } }
    else if (S.sparkle && a[2] == 'sparkle_burst') { if (S.sparkle.burst != !!v) { S.sparkle.burst = !!v; redraw(); } }
    else if (S.sparkle && a[2] == 'sparkle_colour') { if (S.sparkle.colour != v) { S.sparkle.colour = v; redraw(); } }
    else if (a[2] == 'palette') { if (S.paletteLock != v - 1) { S.paletteLock = v - 1; redraw(); } }
    else if (S.sparkle && a[2] == 'sparkle_time') { S.sparkle.time = v; moveSlider('sparkleTime', v); }
    else if (S.sparkle && a[2] == 'sparkle_through') { if (S.sparkle.through != !!v) { S.sparkle.through = !!v; redraw(); } }
    else if (S.sparkle && a[2] == 'sparkle_colour_step') { S.sparkle.step = v; moveSlider('sparkleStep', Math.cbrt(v)); }
    else if (/^slider\d$/.test(a[2])) {  // the playing preset's slider, 0..1 over its range
      const p = S.presets[S.current], j = +a[2].slice(6) - 1, d = p && S.effects[p.effect].params[j];
      if (!d) return;
      let x = d.min + v * (d.max - d.min);
      if (d.step >= 1) x = Math.round(x);
      if (Math.abs(x - p.params[j]) < Math.max(d.step, 1e-6) * 0.5) return;  // our own echo
      p.params[j] = x;
      moveSlider('p' + j, x);
      paramDeps();
      markChanged(S.current);
    }
    return;
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

// ---- Live preview: frames from the board while the Preview section is open and the tab shows
let pvOn = false, pvOff = null, pvDots = null, pvDotImg = null;
let pvMaxSent = 0;  // the points per row last asked for
let pvBare = false;  // Look: bare LEDs with bloom (else diffused bars); kept in this browser
let pvPinned = false;  // the preview stays at the top of the screen while scrolling; kept in this browser
try { pvPinned = localStorage.getItem('beamled.pvpin') == '1'; } catch (e) {}
try { pvBare = localStorage.getItem('beamled.pvlook') == 'bare'; } catch (e) {}
let pvSeen = true, pvWatch = null;  // the preview on screen (not scrolled away)
function previewSync() {
  const cv = document.getElementById('pv');
  if (cv && (!pvWatch || pvWatch.cv != cv) && 'IntersectionObserver' in window) {
    if (pvWatch) pvWatch.disconnect();
    pvWatch = new IntersectionObserver(es => { pvSeen = es[es.length - 1].isIntersecting; previewSync(); });
    pvWatch.cv = cv;
    pvWatch.observe(cv);
  }
  const want = !!cv && !cv.closest('[hidden]') && document.visibilityState == 'visible' && pvSeen;
  // Points per row: a preview this size can't show more (neighbouring LEDs merge on the board)
  const max = 200;
  if ((want == pvOn && (!want || max == pvMaxSent)) || !sock || sock.readyState != 1 || !sock.accepted) return;
  sock.send(JSON.stringify({COMMAND: 'PREVIEW', DATA: want, MAX: max}));
  pvOn = want;
  pvMaxSent = max;
}
// "BLP5": points per row, rows, then each point in 16-bit colour (RGB565): to the 8-bit RGB
// frame the drawing takes, one entry per LED. With more LEDs than points (long rows: the board
// merges neighbours), each point lights all the LEDs it stands for, so no LED is left out.
function unpack565(buf) {
  const s = new Uint8Array(buf), ns = s[4] | s[5] << 8, ny = s[6] | s[7] << 8, n = ns * ny;
  if (s.length < 8 + n * 2 || !ns) return null;
  const v = new Uint16Array(buf.slice(8, 8 + n * 2));
  const L = S && S.layout, nx = L && L.ny == ny && L.nx >= ns ? L.nx : ns, g = Math.ceil(nx / ns);
  const o = new Uint8Array(8 + nx * ny * 3);
  o.set([66, 76, 80, 86, nx & 255, nx >> 8, ny & 255, ny >> 8]);
  for (let b = 0, t = 8; b < ny; b++)
    for (let i = 0; i < nx; i++, t += 3) {
      const p = v[b * ns + Math.min(ns - 1, (i / g) | 0)], r = p >> 11, gr = p >> 5 & 63, bl = p & 31;
      o[t] = r << 3 | r >> 2; o[t + 1] = gr << 2 | gr >> 4; o[t + 2] = bl << 3 | bl >> 2;
    }
  return o.buffer;
}
document.addEventListener('visibilitychange', previewSync);
// The layout from below, the first row on the left: each row at its place, its LEDs up the
// bar. The frame goes into a small image (a column per row) and each column is stretched to scale.
function drawPreview(buf) {
  const cv = document.getElementById('pv');
  if (!cv || !S) return;
  const u8 = new Uint8Array(buf), nx = u8[4] | u8[5] << 8, ny = u8[6] | u8[7] << 8;
  if (u8.length < 8 + nx * ny * 3 || !nx || !ny) return;
  const L = S.layout || {};
  const ry = L.rowY && L.rowY.length == ny ? L.rowY : Array.from({length: ny}, (_, b) => b * 0.5);
  const len = L.length || 5, y0 = ry[0], depth = Math.max(ry[ny - 1] - y0, len * 0.05);
  // Sized in the screen's own pixels so that a bar is a whole number of pixels per LED: every LED
  // on its own pixel, evenly spaced (no 2-then-3 pattern), the layout in its true proportions
  const dpr = window.devicePixelRatio || 1, box = cv.parentElement, cs = getComputedStyle(box);
  const avail = (box.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight)) * dpr;
  const M = Math.round(12 * dpr);  // a small margin round the bars, in screen pixels (room for the glow)
  const maxH = innerHeight * 0.8 * dpr;  // a layout of few rows can't make it taller than the window
  const per = Math.max(1, Math.min(Math.floor((avail - 2 * M) / depth * len / nx), Math.floor((maxH - 2 * M) / nx)));  // pixels per LED
  const scale = per * nx / len;  // screen pixels per metre
  const W = Math.round(depth * scale) + 2 * M, H = per * nx + 2 * M;
  if (cv.width != W || cv.height != H) {
    cv.width = W; cv.height = H; cv.style.width = W / dpr + 'px'; cv.style.height = H / dpr + 'px';
  }
  if (pvBare) return drawBare(u8, nx, ny, cv, ry, y0, M, scale, W, H, dpr, per);
  if (!pvOff || pvOff.width != ny || pvOff.height != nx) { pvOff = document.createElement('canvas'); pvOff.width = ny; pvOff.height = nx; }
  const oc = pvOff.getContext('2d'), img = oc.createImageData(ny, nx), d = img.data;
  for (let b = 0; b < ny; b++)
    for (let i = 0; i < nx; i++) {
      const s = 8 + (b * nx + i) * 3, t = (i * ny + b) * 4;
      d[t] = u8[s]; d[t + 1] = u8[s + 1]; d[t + 2] = u8[s + 2]; d[t + 3] = 255;
    }
  oc.putImageData(img, 0, 0);
  const c = cv.getContext('2d');
  c.setTransform(1, 0, 0, 1, 0, 0);
  c.imageSmoothingEnabled = true;
  c.fillStyle = '#000'; c.fillRect(0, 0, W, H);
  const top = M, tall = per * nx;
  const glow = 'filter' in c;
  for (const pass of glow ? [0, 1] : [1]) {
    c.filter = pass == 0 ? 'blur(' + 6 * dpr + 'px)' : 'none';
    c.globalAlpha = pass == 0 ? 0.9 : 1;
    const w = (pass == 0 ? 14 : 5) * dpr;
    for (let b = 0; b < ny; b++) c.drawImage(pvOff, b, 0, 1, nx, M + (ry[b] - y0) * scale - w / 2, top, w, tall);
  }
  c.filter = 'none'; c.globalAlpha = 1;
}
// Bare LEDs: every LED one point of light at its place (written straight into an image, no call
// per LED). The glare comes from a coarse image (a cell per 4 x 4 pixels) where neighbouring LEDs
// add up, as their glare does in a camera: blurred and laid on top it is the bloom, and bright
// LEDs go towards white in the middle with their colour in the glare.
let pvGlow = null, pvGlowImg = null, pvAcc = null;
function drawBare(u8, nx, ny, cv, ry, y0, M, scale, W, H, dpr, per) {
  const w = Math.max(1, Math.round(W)), h = Math.max(1, Math.round(H));
  const G = 4, gw = Math.ceil(w / G), gh = Math.ceil(h / G);
  if (!pvDots || pvDots.width != w || pvDots.height != h) {
    pvDots = document.createElement('canvas'); pvDots.width = w; pvDots.height = h;
    pvDotImg = pvDots.getContext('2d').createImageData(w, h);
    pvGlow = document.createElement('canvas'); pvGlow.width = gw; pvGlow.height = gh;
    pvGlowImg = pvGlow.getContext('2d').createImageData(gw, gh);
    pvAcc = new Float32Array(gw * gh * 3);
  }
  const d = pvDotImg.data, acc = pvAcc;
  d.fill(0); acc.fill(0);
  // Each LED a square dot that grows with the spacing: about a third of it, at least 2 pixels wide
  const dot = Math.max(2, Math.round(per * 0.35)), dh = Math.max(1, Math.min(dot, per - 1));
  const round = dot >= 3 && dh >= 3;  // big enough to round: leave out the corner pixels
  const top = M, mid = (per - dh + 1) >> 1;
  for (let b = 0; b < ny; b++) {
    const x = M + Math.round((ry[b] - y0) * scale) - (dot >> 1);  // the dot: x .. x + dot - 1
    if (x < 0 || x + dot > w) continue;
    const gx = (x / G) | 0;
    for (let i = 0; i < nx; i++) {
      const y = top + i * per + mid;  // exactly per pixels apart
      if (y < 0 || y + dh > h) continue;
      const s = 8 + (b * nx + i) * 3, g = (((y / G) | 0) * gw + gx) * 3;
      const r = u8[s], gr = u8[s + 1], bl = u8[s + 2];
      for (let yy = y; yy < y + dh; yy++) {
        const edge = round && (yy == y || yy == y + dh - 1);  // top and bottom rows lose their corners
        for (let t = (yy * w + x) * 4 + (edge ? 4 : 0), e = (yy * w + x + dot - (edge ? 1 : 0)) * 4; t < e; t += 4) {
          d[t] = r; d[t + 1] = gr; d[t + 2] = bl; d[t + 3] = 255;
        }
      }
      acc[g] += r; acc[g + 1] += gr; acc[g + 2] += bl;
    }
  }
  pvDots.getContext('2d').putImageData(pvDotImg, 0, 0);
  const gd = pvGlowImg.data;
  for (let k = 0, n = gw * gh; k < n; k++) {
    gd[4 * k] = acc[3 * k] * 0.55; gd[4 * k + 1] = acc[3 * k + 1] * 0.55; gd[4 * k + 2] = acc[3 * k + 2] * 0.55; gd[4 * k + 3] = 255;
  }
  pvGlow.getContext('2d').putImageData(pvGlowImg, 0, 0);
  const c = cv.getContext('2d');
  c.setTransform(1, 0, 0, 1, 0, 0);
  c.filter = 'none'; c.globalAlpha = 1; c.globalCompositeOperation = 'source-over';
  c.fillStyle = '#000'; c.fillRect(0, 0, W, H);
  c.globalCompositeOperation = 'lighter';
  // the bloom: the coarse glare, smoothly enlarged and softened
  c.imageSmoothingEnabled = true;
  if ('filter' in c) c.filter = 'blur(' + 5 * dpr + 'px)';
  c.drawImage(pvGlow, 0, 0, W, H);
  c.filter = 'none';
  c.globalAlpha = 0.6;
  c.drawImage(pvGlow, 0, 0, W, H);  // a tighter, brighter core of glare
  c.globalAlpha = 1;
  // the points themselves, sharp, added twice so bright ones go white
  c.imageSmoothingEnabled = false;
  c.drawImage(pvDots, 0, 0, W, H);
  c.drawImage(pvDots, 0, 0, W, H);
  c.globalCompositeOperation = 'source-over';
}

function live(take = false) {
  const ws = sock = new WebSocket('ws://' + location.host + '/');
  ws.binaryType = 'arraybuffer';
  ws.onopen = () => ws.send(JSON.stringify({COMMAND: 'PAGE', ID: pageId, TAKE: take}));
  ws.onmessage = e => {
    if (typeof e.data != 'string') {
      const u8 = new Uint8Array(e.data);
      if (u8[0] == 66 && u8[1] == 76 && u8[2] == 80 && u8[3] == 53) { const f = unpack565(e.data); return f && drawPreview(f); }  // "BLP5"
      for (const [addr, v] of oscMessages(e.data)) oscValue(addr, v);
      return;
    }
    const m = JSON.parse(e.data);
    if (m.COMMAND == 'BUSY') {
      elsewhere('beamLED is open in another window or on another device. Continue to use it here?');
      ws.close();
    } else if (m.COMMAND == 'TAKEN') {
      elsewhere('beamLED is now open in another window or on another device. Continue to use it here?');
      ws.close();
    } else if (m.COMMAND == 'STATUS') {
      if (!ws.accepted) { ws.accepted = true; ws.send(JSON.stringify({COMMAND: 'LISTEN', DATA: '/'})); pvOn = false; previewSync(); }
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
