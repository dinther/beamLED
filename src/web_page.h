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
h2{font-size:14px;text-transform:uppercase;letter-spacing:.06em;color:var(--dim);margin:22px 0 8px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:10px 12px}
button{background:#2a2a2a;color:var(--fg);border:1px solid #444;border-radius:7px;padding:7px 12px;font:inherit;cursor:pointer}
button.on{background:var(--acc);border-color:var(--acc);color:#000}
button.warn{background:var(--warn);border-color:var(--warn);color:#fff}
button:disabled{opacity:.4;cursor:default}
.top{display:flex;align-items:center;gap:10px}
.top button{min-width:72px}
.presets{display:grid;grid-template-columns:repeat(auto-fill,minmax(110px,1fr));gap:6px}
.presets button{text-align:left;padding:9px 10px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.presets button.add{text-align:center;color:var(--dim)}
.row{display:flex;align-items:center;gap:10px;margin:7px 0}
.row label{flex:0 0 130px;color:#bbb}
.row input[type=range]{flex:1;min-width:0;accent-color:var(--acc)}
.row output{flex:0 0 46px;text-align:right;font-variant-numeric:tabular-nums;color:var(--dim)}
.btns{display:flex;flex-wrap:wrap;gap:6px;margin:7px 0}
select,input[type=text],input[type=password],input[type=number]{flex:1;min-width:0;background:#222;color:var(--fg);border:1px solid #444;border-radius:6px;padding:6px 8px;font:inherit}
.pal{display:flex;flex-wrap:wrap;align-items:center;gap:6px;padding:8px 0;border-bottom:1px solid var(--line)}
.pal:last-child{border-bottom:0}
.pal input[type=text]{flex:0 1 140px}
.pal input[type=color]{width:38px;height:32px;border:1px solid #444;border-radius:6px;background:none;padding:2px}
.pal .sw{flex:1 0 100%;height:12px;border-radius:4px}
.pal button{padding:4px 10px}
.status{color:var(--dim);font-size:13px;margin-top:6px}
.nets button{display:block;width:100%;text-align:left;margin:3px 0}
.hint{color:var(--dim);font-size:13px}
.unis{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:6px 12px;margin:6px 0 10px}
.unis label{display:flex;align-items:center;gap:6px;color:#bbb;font-size:13px}
.unis label span:first-child{flex:0 0 46px}
.unis input{flex:0 0 72px;width:72px}
.row input[type=number]{flex:0 0 90px}
[data-tip] label,label[data-tip],h2[data-tip]{cursor:help;text-decoration:underline dotted #555;text-underline-offset:3px}
#tip{position:fixed;z-index:9;display:none;max-width:min(380px,calc(100vw - 24px));background:#2b2b2b;color:#eee;
  border:1px solid #555;border-radius:8px;padding:8px 10px;font-size:13px;line-height:1.45;box-shadow:0 4px 16px #000a;pointer-events:none}
</style></head><body><main id="app">Loading...</main><div id="tip"></div>
<script>
let S, N;
const h = (tag, props = {}, ...kids) => { const e = Object.assign(document.createElement(tag), props); e.append(...kids); return e; };
const post = (u, o) => fetch(u, {method: 'POST', body: JSON.stringify(o)}).then(r => r.ok ? r.json() : r.text().then(t => { throw new Error(t); }));
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
  if (touch && t && (e.target.tagName == 'LABEL' || e.target.tagName == 'H2')) showTip(t);
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
  status: 'Frames per second (the target is 40) and the time one frame takes to draw (25 ms is the budget), the network connection, WiFi, Ableton Link and OSC.',
  presets: 'Click a preset to play it. The button on the board steps through them in this order (short press: next, long press: on/off), and the one playing is kept after a power cut. Up to 32 presets.',
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
  sparkle: 'White twinkles on top of this preset’s effect. 0 = none, 1 = about 8% of the LEDs lit at any moment.',
  pulse: 'A brightness accent on the beat: full on each beat and dipping in between, strongest on the bar’s first beat. 0 = steady. Follows Ableton Link, or the board’s own tempo.',
  reset: 'Sets this preset’s effect parameters back to the effect’s defaults. Name, palette, timing, sparkle and pulse stay as they are.',
  palettes: 'Palettes are shared: each preset picks one. Effects blend smoothly between the colours in order; looping effects wrap from the last colour back to the first.',
  palName: 'The palette’s name, as listed in each preset’s Palette choice. Up to 15 characters.',
  palColour: 'A colour of this palette. Effects blend between the colours in this order. Click to change it; the ceiling follows as you pick.',
  palAdd: 'Adds a colour at the end, a copy of the last one. Up to 5 colours.',
  palRemove: 'Removes the last colour. At least 2 colours stay.',
  target: 'Where the Art-Net goes: the PixLite (192.168.0.50) for the ceiling, or a PC running Beam for testing. Use the device’s own address.',
  gamma: 'The brightness curve, so fades look even to the eye. 2.2 suits most LEDs; lower makes dark parts brighter and flatter, higher gives deeper darks.',
  link: 'Ableton Link. Enabled: tempo and beat are shared with Ableton Live and other Link apps on the same network (the Live laptop joins the board’s network or its hotspot). Disabled: the board keeps its own tempo.',
  bpm: 'The tempo in beats per minute. With Ableton Link enabled and peers connected this changes everyone’s tempo; without peers it sets the board’s own.',
  latency: 'How much later the LEDs show a frame than the board draws it (network and PixLite). Frames are drawn this far ahead so beats land on time. Raise it if the lights trail the music.',
  osc: 'OSC control, for MIMa and other OSC or OSCQuery apps. In MIMa, add beamLED as an OSCQuery application at http://beamled.local:8010 (or this board’s IP address, port 8010): it lists every preset, parameter and palette colour with its range and help, and its knobs and lights follow changes made here or with the button. OSC changes are saved with the next save from this page or a preset change.',
  nx: 'How many LEDs each row (bar) has. The ceiling: 180.',
  ny: 'How many rows (bars) there are. The ceiling: 15.',
  len: 'The length of a row in metres; the LED spacing is this divided by LEDs per row. The ceiling: 5 m.',
  aspect: 'Row spacing divided by LED spacing, for evenly spaced rows. Used when Measured rows is off or the row count differs from the ceiling’s.',
  measured: 'Uses the ceiling’s measured, uneven row positions (only with 15 rows). Off: rows evenly spaced by Aspect.',
  rowUni: 'The Art-Net universe each row starts at. A row of more than 170 LEDs continues into the next universe(s). The PixLite expects row n (counting from 0) at universe 2n.',
  fill: 'Numbers the rows one after another, starting at the number given, leaving each row the universes it needs.',
  apply: 'Applies the layout at once and saves it. Presets and palettes are kept.',
  netName: 'The board’s name: http://<name>.local, the hotspot’s name and the firmware update name. Takes effect after the restart.',
  fallback: 'The address the board takes when no DHCP server answers within 10 seconds (the cable goes straight to the PixLite). Keep it in the PixLite’s subnet, 192.168.0.x. Empty: an automatic 169.254.x.x address.',
  ssid: 'A WiFi network the board joins while the Ethernet cable is unplugged. Empty: it opens its own hotspot instead.',
  wifiPass: 'The backup WiFi’s password. Leave it empty to keep the saved one.',
  scan: 'Lists the WiFi networks in range; click one to fill in its name.',
  apPass: 'A password for the board’s hotspot, 8 characters or more. Empty: an open hotspot. Type - to remove a saved password.',
  save: 'Saves the network settings and restarts the board; the lights go out for a few seconds.',
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
function field(label, input, help) { return tip(h('div', {className: 'row'}, h('label', {textContent: label}), input), help); }
const button = (text, help, props = {}) => tip(h('button', Object.assign({textContent: text}, props)), help);
const heading = (text, help) => tip(h('h2', {textContent: text}), help);

async function load() {
  S = await (await fetch('/api/state')).json();
  N = await (await fetch('/api/net')).json();
  draw();
}

// The preset list changed on the board: take its state
const presetList = o => post('/api/presets', o).then(st => { S = st; draw(); });

function draw() {
  hideTip();
  sliders = {};
  const app = document.getElementById('app');
  app.replaceChildren();
  const p = S.presets[S.current], ef = S.effects[p.effect];

  const onb = button(S.on ? 'On' : 'Off', H.on, {className: S.on ? 'on' : ''});
  onb.onclick = () => { S.on = !S.on; post('/api/set', {on: S.on}); draw(); };
  app.append(h('div', {className: 'top'}, h('h1', {textContent: 'beamLED'}), onb));
  app.append(h('div', {className: 'card'},
    slider('Brightness', 0, 255, 1, S.brightness, v => Math.round(v / 2.55) + '%',
      v => { S.brightness = v; throttle('b', () => post('/api/set', {brightness: v})); }, H.brightness, 'brightness'),
    tip(h('div', {className: 'status', id: 'status'}), H.status)));

  app.append(heading('Presets', H.presets));
  const g = h('div', {className: 'presets'});
  S.presets.forEach((q, i) => {
    const b = button((i + 1) + '  ' + q.name, H.play + ' ' + (i + 1) + ': ' + q.name + ' (' + S.effects[q.effect].name + ').',
      {className: i == S.current ? 'on' : ''});
    b.onclick = () => { S.current = i; S.on = true; post('/api/set', {current: i}); draw(); };
    g.append(b);
  });
  if (S.presets.length < S.maxPresets) {
    const add = button('+ Add', H.add, {className: 'add'});
    add.onclick = () => presetList({add: S.current});
    g.append(add);
  }
  app.append(g);

  app.append(heading('Preset ' + (S.current + 1), H.presets));
  const card = h('div', {className: 'card'});
  const nm = h('input', {type: 'text', value: p.name, maxLength: 19});
  nm.onchange = () => { p.name = nm.value; post('/api/preset', {index: S.current, name: nm.value}); draw(); };
  card.append(field('Name', nm, H.name));
  const earlier = button('◀ Earlier', H.earlier, {disabled: S.current == 0});
  earlier.onclick = () => presetList({move: S.current, to: S.current - 1});
  const later = button('Later ▶', H.later, {disabled: S.current == S.presets.length - 1});
  later.onclick = () => presetList({move: S.current, to: S.current + 1});
  const del = button('Delete', H.del, {disabled: S.presets.length <= 1});
  let armed = null;
  del.onclick = () => {
    if (armed) { clearTimeout(armed); presetList({delete: S.current}); return; }
    del.textContent = 'Really delete?'; del.className = 'warn';
    armed = setTimeout(() => { armed = null; del.textContent = 'Delete'; del.className = ''; }, 3000);
  };
  card.append(h('div', {className: 'btns'}, earlier, later, del));
  card.append(select('Effect', S.effects.map(e => e.name), p.effect, v =>
    post('/api/preset', {index: S.current, effect: v}).then(np => { S.presets[S.current] = np; draw(); }),
    H.effect + ' ' + ef.name + ': ' + ef.help));
  card.append(select('Palette', S.palettes.map(q => q.name), p.palette, v => {
    p.palette = v; post('/api/preset', {index: S.current, palette: v}); }, H.palette));
  // Timing: tempo and beat come from Ableton Link (or the board's own clock when it is disabled).
  // In beats, the effect's cycle (a band, a revolution, ...) takes 1, 2, 4 ... 64 beats and the
  // speed slider is replaced by that choice.
  const modes = ['Time', 'Time, follow tempo'].concat(ef.cycle ? ['Beats'] : []);
  const timing = Math.min(p.timing, modes.length - 1);
  const inBeats = timing == 2;
  card.append(select('Timing', modes, timing, v => {
    p.timing = v; post('/api/preset', {index: S.current, timing: v}); draw(); }, H.timing));
  if (inBeats) {
    const beats = [1, 2, 4, 8, 16, 32, 64].map(b => b + ' beat' + (b > 1 ? 's' : '') + (b >= 4 ? ' (' + b / 4 + ' bar' + (b > 4 ? 's' : '') + ')' : ''));
    card.append(select('Beats per ' + ef.cycle, beats, p.beatsExp, v => {
      p.beatsExp = v; post('/api/preset', {index: S.current, beatsExp: v}); }, H.beats + ' One cycle here: one ' + ef.cycle + '.'));
    const rev = h('input', {type: 'checkbox', checked: p.reverse});
    rev.onchange = () => { p.reverse = rev.checked; post('/api/preset', {index: S.current, reverse: rev.checked}); };
    card.append(field('Reverse', rev, H.reverse));
  }
  ef.params.forEach((d, j) => {
    if (inBeats && j == ef.speedParam) return;
    card.append(slider(d.name, d.min, d.max, d.step, p.params[j],
      v => v.toFixed(decimals(d.step)),
      v => { p.params[j] = v; throttle('p', () => post('/api/preset', {index: S.current, params: p.params})); },
      d.help + ' (' + d.min + ' to ' + d.max + ', default ' + d.def + ')', 'p' + j));
  });
  if (ef.sparkle) card.append(slider('Sparkle overlay', 0, 1, 0.01, p.sparkle, v => v.toFixed(2),
    v => { p.sparkle = v; throttle('s', () => post('/api/preset', {index: S.current, sparkle: v})); }, H.sparkle, 'sparkle'));
  card.append(slider('Beat pulse', 0, 1, 0.01, p.pulse, v => v.toFixed(2),
    v => { p.pulse = v; throttle('pu', () => post('/api/preset', {index: S.current, pulse: v})); }, H.pulse, 'pulse'));
  const reset = button('Reset parameters', H.reset);
  reset.onclick = () => { p.params = ef.params.map(d => d.def); post('/api/preset', {index: S.current, params: p.params}); draw(); };
  card.append(h('div', {className: 'btns'}, reset));
  app.append(card);

  app.append(heading('Palettes', H.palettes));
  const pc = h('div', {className: 'card'});
  S.palettes.forEach((q, i) => pc.append(paletteEditor(q, i)));
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
  showStatus();
}

function paletteEditor(q, i) {
  const box = h('div', {className: 'pal'});
  const sw = h('div', {className: 'sw'});
  const paint = () => sw.style.background = 'linear-gradient(90deg,' + q.colors.join(',') + ')';
  const send = () => throttle('pal' + i, () => post('/api/palette', {index: i, name: q.name, colors: q.colors}));
  const nm = tip(h('input', {type: 'text', value: q.name, maxLength: 15}), H.palName);
  nm.onchange = () => { q.name = nm.value; send(); draw(); };
  box.append(nm);
  q.colors.forEach((c, j) => {
    const ci = tip(h('input', {type: 'color', value: c}), H.palColour + ' (colour ' + (j + 1) + ' of ' + q.colors.length + ')');
    ci.oninput = () => { q.colors[j] = ci.value; paint(); send(); };
    box.append(ci);
  });
  if (q.colors.length < 5) {
    const b = button('+', H.palAdd);
    b.onclick = () => { q.colors.push(q.colors[q.colors.length - 1]); send(); draw(); };
    box.append(b);
  }
  if (q.colors.length > 2) {
    const b = button('−', H.palRemove);
    b.onclick = () => { q.colors.pop(); send(); draw(); };
    box.append(b);
  }
  paint();
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
  const upr = () => Math.max(1, Math.ceil(+nx.value / 170));
  const grid = tip(h('div', {className: 'unis'}), H.rowUni);
  const table = () => {
    const rows = Math.max(1, Math.min(64, +ny.value || 1));
    while (uni.length < rows) uni.push(uni.length ? uni[uni.length - 1] + upr() : 0);
    grid.replaceChildren();
    for (let b = 0; b < rows; b++) {
      const range = h('span', {className: 'hint'});
      const showRange = () => range.textContent = upr() > 1 ? '–' + (uni[b] + upr() - 1) : '';
      const inp = h('input', {type: 'number', min: 0, max: 32767, step: 1, value: uni[b]});
      inp.oninput = () => { uni[b] = Math.max(0, Math.round(+inp.value)); showRange(); upd(); };
      showRange();
      grid.append(h('label', {}, h('span', {textContent: 'Row ' + (b + 1)}), inp, range));
    }
  };
  const fillFrom = num(uni[0] || 0, 1);
  const fill = button('Fill', H.fill);
  fill.onclick = () => { for (let b = 0; b < uni.length; b++) uni[b] = +fillFrom.value + b * upr(); table(); upd(); };
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
      measured: meas.checked, rowUni: uni.slice(0, +ny.value)})
    .then(st => { S = st; draw(); }).catch(e => { msg.textContent = e.message; });
  table();
  upd();
  return h('div', {className: 'card'},
    field('LEDs per row', nx, H.nx), field('Rows', ny, H.ny), field('Row length (m)', len, H.len),
    field('Aspect', asp, H.aspect), h('div', {className: 'row'}, h('label'), h('span', {className: 'hint', textContent: 'row spacing ÷ LED spacing'})),
    field('Measured rows', meas, H.measured),
    tip(h('div', {className: 'row'}, h('label', {textContent: 'Row universes'}), h('span', {className: 'hint', textContent: 'consecutive from'}), fillFrom, fill), H.rowUni),
    grid, info, h('div', {className: 'row'}, h('label'), apply), msg);
}

function networkCard() {
  const name = h('input', {type: 'text', value: N.name, maxLength: 31});
  const fb = h('input', {type: 'text', value: N.fallback, placeholder: 'automatic: ' + N.autoFallback});
  const ssid = h('input', {type: 'text', value: N.ssid, placeholder: 'none: hotspot only'});
  const wp = h('input', {type: 'password', placeholder: N.wifiPassSet ? 'saved (leave empty to keep)' : 'none'});
  const ap = h('input', {type: 'password', placeholder: N.apPassSet ? 'saved (leave empty to keep, - to remove)' : 'open hotspot'});
  const msg = h('div', {className: 'hint'});
  const nets = h('div', {className: 'nets'});
  const scan = button('Scan', H.scan);
  scan.onclick = async () => {
    scan.disabled = true; msg.textContent = 'Scanning...';
    // The board pushes the results when the scan ends (live connection), no asking again
    const done = new Promise(res => { scanDone = res; setTimeout(() => res({state: 3, nets: []}), 20000); });
    let r = await post('/api/scan', {});
    if (r.state == 1) r = await done;
    scanDone = null;
    scan.disabled = false;
    msg.textContent = r.state == 2 ? '' : 'Scan failed';
    nets.replaceChildren(...r.nets.map(n => { const b = h('button', {textContent: n.s + '  (' + n.r + ' dBm' + (n.l ? ', locked' : '') + ')'}); b.onclick = () => { ssid.value = n.s; wp.focus(); }; return b; }));
  };
  const save = button('Save and restart', H.save, {className: 'on'});
  save.onclick = () => post('/api/net', {name: name.value, fallback: fb.value, ssid: ssid.value, wifiPass: wp.value, apPass: ap.value})
    .then(() => { msg.textContent = 'Restarting... reconnect at http://' + name.value + '.local in a few seconds.'; })
    .catch(e => { msg.textContent = e.message; });
  return h('div', {className: 'card'},
    field('Name', name, H.netName), field('Fallback IP', fb, H.fallback), field('Backup WiFi', ssid, H.ssid), field('WiFi password', wp, H.wifiPass),
    h('div', {className: 'row'}, h('label'), scan), nets, field('Hotspot password', ap, H.apPass),
    h('div', {className: 'row'}, h('label'), save), msg,
    h('p', {className: 'hint', textContent: 'DHCP first; with no answer after 10 s the fallback IP is used (keep it in the PixLite subnet, 192.168.0.x). On the fallback IP, or with the cable unplugged and no backup WiFi, the board opens a hotspot named after it: http://192.168.4.1.'}));
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
  const el = document.getElementById('status');
  const L = S.link || {};
  const link = !L.enabled ? 'Ableton Link disabled'
    : 'Ableton Link · ' + (L.peers > 0 ? L.peers + ' peer' + (L.peers == 1 ? '' : 's') : 'no peers');
  if (el) el.textContent = S.fps + ' fps, render ' + S.renderMs + ' ms · ' + S.net + ' · WiFi: ' + S.wifi +
    ' · ' + link + ' · ' + (L.bpm || 120) + ' BPM';
  const pe = document.getElementById('peers');
  if (pe) pe.textContent = L.enabled ? (L.peers > 0 ? L.peers + ' peer' + (L.peers == 1 ? '' : 's') : 'no peers') : '';
  const b = document.getElementById('bpm');
  if (b && document.activeElement != b) b.value = L.bpm;
  const o = document.getElementById('osc');
  if (o) o.textContent = S.osc || '';
}

// ---- live: no polling. The board pushes over a WebSocket on its OSCQuery port (8010): every
// change of a value as OSC (whoever made it: MIMa, the button, another browser), and as text
// its status once a second, a change counter for what OSC doesn't carry (names, the preset
// list, layout: the page reloads), and WiFi scan results.
let scanDone = null, held = false, redrawDue = false, reloadDue = false;
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
    return;
  }
  const n = +a[3] - 1, key = a[4];
  if (a[2] == 'presets') {
    const p = S.presets[n];
    if (!p || key == 'play') return;
    const mine = n == S.current;
    if (key == 'effect') { if (p.effect != v - 1) reload(); return; }
    if (key == 'palette') p.palette = v - 1;
    else if (key == 'sparkle') { p.sparkle = v; if (mine) return moveSlider('sparkle', v); }
    else if (key == 'pulse') { p.pulse = v; if (mine) return moveSlider('pulse', v); }
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
    const j = +key.slice(6) - 1;
    if (j < q.colors.length && q.colors[j] != v) { q.colors[j] = v; redraw(); }
  }
}
const reload = () => { reloadDue = true; settle(); };

function live() {
  const ws = new WebSocket('ws://' + location.hostname + ':8010/');
  ws.binaryType = 'arraybuffer';
  ws.onopen = () => {
    ws.send(JSON.stringify({COMMAND: 'PAGE'}));
    ws.send(JSON.stringify({COMMAND: 'LISTEN', DATA: '/'}));
  };
  ws.onmessage = e => {
    if (typeof e.data != 'string') { for (const [addr, v] of oscMessages(e.data)) oscValue(addr, v); return; }
    const m = JSON.parse(e.data);
    if (m.COMMAND == 'STATUS') {
      const st = m.DATA;
      if (!S) return;
      if (st.rev != S.rev || st.presetCount != S.presets.length) { S.rev = st.rev; reload(); }
      const changed = st.current != S.current || st.on != S.on;
      Object.assign(S, st);
      if (changed) redraw(); else showStatus();
    } else if (m.COMMAND == 'SCAN') {
      if (scanDone) scanDone(m.DATA);
    } else if (m.COMMAND == 'PATH_ADDED' || m.COMMAND == 'PATH_REMOVED') {
      reload();
    }
  };
  // The board restarted or the network dropped: connect again (and reload, things may differ)
  ws.onclose = () => setTimeout(() => { reloadDue = true; settle(); live(); }, 3000);
}
load().then(live);
</script></body></html>)HTML";
