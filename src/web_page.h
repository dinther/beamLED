// The web page: presets, brightness, effect parameters, palettes, network settings.
#pragma once
#include <pgmspace.h>

static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>beamLED</title>
<style>
:root{--bg:#121212;--card:#1c1c1c;--line:#333;--fg:#ddd;--dim:#888;--acc:#e0a020}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,sans-serif}
main{max-width:640px;margin:auto;padding:12px 16px 40px}
h1{font-size:20px;margin:0;flex:1}
h2{font-size:14px;text-transform:uppercase;letter-spacing:.06em;color:var(--dim);margin:22px 0 8px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:10px 12px}
button{background:#2a2a2a;color:var(--fg);border:1px solid #444;border-radius:7px;padding:7px 12px;font:inherit;cursor:pointer}
button.on{background:var(--acc);border-color:var(--acc);color:#000}
.top{display:flex;align-items:center;gap:10px}
.top button{min-width:72px}
.presets{display:grid;grid-template-columns:repeat(auto-fill,minmax(110px,1fr));gap:6px}
.presets button{text-align:left;padding:9px 10px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.row{display:flex;align-items:center;gap:10px;margin:7px 0}
.row label{flex:0 0 130px;color:#bbb}
.row input[type=range]{flex:1;min-width:0;accent-color:var(--acc)}
.row output{flex:0 0 46px;text-align:right;font-variant-numeric:tabular-nums;color:var(--dim)}
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
</style></head><body><main id="app">Loading...</main>
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
const decimals = s => s >= 1 ? 0 : s >= 0.1 ? 1 : 2;
function slider(label, min, max, step, val, fmt, onin) {
  const o = h('output', {textContent: fmt(val)});
  const r = h('input', {type: 'range', min, max, step, value: val});
  r.oninput = () => { o.textContent = fmt(+r.value); onin(+r.value); };
  return h('div', {className: 'row'}, h('label', {textContent: label}), r, o);
}
function select(label, items, val, onch) {
  const s = h('select');
  items.forEach((t, i) => s.append(h('option', {value: i, textContent: t, selected: i == val})));
  s.onchange = () => onch(+s.value);
  return h('div', {className: 'row'}, h('label', {textContent: label}), s);
}
function field(label, input) { return h('div', {className: 'row'}, h('label', {textContent: label}), input); }

async function load() {
  S = await (await fetch('/api/state')).json();
  N = await (await fetch('/api/net')).json();
  draw();
}

function draw() {
  const app = document.getElementById('app');
  app.replaceChildren();
  const p = S.presets[S.current], ef = S.effects[p.effect];

  const onb = h('button', {textContent: S.on ? 'On' : 'Off', className: S.on ? 'on' : ''});
  onb.onclick = () => { S.on = !S.on; post('/api/set', {on: S.on}); draw(); };
  app.append(h('div', {className: 'top'}, h('h1', {textContent: 'beamLED'}), onb));
  app.append(h('div', {className: 'card'},
    slider('Brightness', 0, 255, 1, S.brightness, v => Math.round(v / 2.55) + '%',
      v => { S.brightness = v; throttle('b', () => post('/api/set', {brightness: v})); }),
    h('div', {className: 'status', id: 'status'})));

  app.append(h('h2', {textContent: 'Presets'}));
  const g = h('div', {className: 'presets'});
  S.presets.forEach((q, i) => {
    const b = h('button', {textContent: (i + 1) + '  ' + q.name, className: i == S.current ? 'on' : ''});
    b.onclick = () => { S.current = i; S.on = true; post('/api/set', {current: i}); draw(); };
    g.append(b);
  });
  app.append(g);

  app.append(h('h2', {textContent: 'Preset ' + (S.current + 1)}));
  const card = h('div', {className: 'card'});
  const nm = h('input', {type: 'text', value: p.name, maxLength: 19});
  nm.onchange = () => { p.name = nm.value; post('/api/preset', {index: S.current, name: nm.value}); draw(); };
  card.append(field('Name', nm));
  card.append(select('Effect', S.effects.map(e => e.name), p.effect, v =>
    post('/api/preset', {index: S.current, effect: v}).then(np => { S.presets[S.current] = np; draw(); })));
  card.append(select('Palette', S.palettes.map(q => q.name), p.palette, v => {
    p.palette = v; post('/api/preset', {index: S.current, palette: v}); }));
  ef.params.forEach((d, j) => card.append(slider(d.name, d.min, d.max, d.step, p.params[j],
    v => v.toFixed(decimals(d.step)),
    v => { p.params[j] = v; throttle('p', () => post('/api/preset', {index: S.current, params: p.params})); })));
  if (ef.sparkle) card.append(slider('Sparkle overlay', 0, 1, 0.01, p.sparkle, v => v.toFixed(2),
    v => { p.sparkle = v; throttle('s', () => post('/api/preset', {index: S.current, sparkle: v})); }));
  const reset = h('button', {textContent: 'Reset parameters'});
  reset.onclick = () => { p.params = ef.params.map(d => d.def); post('/api/preset', {index: S.current, params: p.params}); draw(); };
  card.append(h('div', {className: 'row'}, reset));
  app.append(card);

  app.append(h('h2', {textContent: 'Palettes'}));
  const pc = h('div', {className: 'card'});
  S.palettes.forEach((q, i) => pc.append(paletteEditor(q, i)));
  app.append(pc);

  app.append(h('h2', {textContent: 'Output'}));
  const tgt = h('input', {type: 'text', value: S.target});
  tgt.onchange = () => post('/api/set', {target: tgt.value.trim()}).then(() => { S.target = tgt.value.trim(); });
  const quick = (t, ip) => { const b = h('button', {textContent: t}); b.onclick = () => { tgt.value = ip; tgt.onchange(); }; return b; };
  app.append(h('div', {className: 'card'},
    field('Art-Net to', tgt),
    h('div', {className: 'row'}, h('label'), quick('PixLite', '192.168.0.50'), quick('Beam (LAN)', '192.168.1.2'), quick('Beam', '192.168.0.20')),
    slider('Gamma', 1, 3, 0.1, S.gamma, v => v.toFixed(1), v => { S.gamma = v; throttle('g', () => post('/api/set', {gamma: v})); })));

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
  const nm = h('input', {type: 'text', value: q.name, maxLength: 15});
  nm.onchange = () => { q.name = nm.value; send(); draw(); };
  box.append(nm);
  q.colors.forEach((c, j) => {
    const ci = h('input', {type: 'color', value: c});
    ci.oninput = () => { q.colors[j] = ci.value; paint(); send(); };
    box.append(ci);
  });
  if (q.colors.length < 5) {
    const b = h('button', {textContent: '+', title: 'Add a colour'});
    b.onclick = () => { q.colors.push(q.colors[q.colors.length - 1]); send(); draw(); };
    box.append(b);
  }
  if (q.colors.length > 2) {
    const b = h('button', {textContent: '\u2212', title: 'Remove the last colour'});
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
  const grid = h('div', {className: 'unis'});
  const table = () => {
    const rows = Math.max(1, Math.min(64, +ny.value || 1));
    while (uni.length < rows) uni.push(uni.length ? uni[uni.length - 1] + upr() : 0);
    grid.replaceChildren();
    for (let b = 0; b < rows; b++) {
      const range = h('span', {className: 'hint'});
      const showRange = () => range.textContent = upr() > 1 ? '\u2013' + (uni[b] + upr() - 1) : '';
      const inp = h('input', {type: 'number', min: 0, max: 32767, step: 1, value: uni[b]});
      inp.oninput = () => { uni[b] = Math.max(0, Math.round(+inp.value)); showRange(); upd(); };
      showRange();
      grid.append(h('label', {}, h('span', {textContent: 'Row ' + (b + 1)}), inp, range));
    }
  };
  const fillFrom = num(uni[0] || 0, 1);
  const fill = h('button', {textContent: 'Fill'});
  fill.onclick = () => { for (let b = 0; b < uni.length; b++) uni[b] = +fillFrom.value + b * upr(); table(); upd(); };
  const upd = () => {
    const pitch = +len.value / Math.max(1, +nx.value), leds = +nx.value * +ny.value;
    const m = meas.checked && +ny.value == L.measuredRows;
    const used = uni.slice(0, +ny.value), dup = new Set();
    used.forEach((u, i) => used.forEach((v, j) => { if (i != j && u < v + upr() && v < u + upr()) dup.add(i + 1); }));
    info.textContent = 'LED spacing ' + (pitch * 1000).toFixed(1) + ' mm, row spacing ' +
      (m ? 'measured (ceiling)' : (pitch * +asp.value).toFixed(3) + ' m') + ' \u00b7 ' + leds + ' LEDs' +
      (leds > L.maxLeds ? ' (max ' + L.maxLeds + ')' : '') + ' \u00b7 ' + upr() + ' universe' + (upr() > 1 ? 's' : '') +
      ' per row' + (dup.size ? ' \u00b7 overlapping universes: rows ' + [...dup].join(', ') : '');
  };
  nx.oninput = () => { table(); upd(); };
  ny.oninput = () => { table(); upd(); };
  [len, asp, meas].forEach(e => e.oninput = upd);
  const apply = h('button', {textContent: 'Apply', className: 'on'});
  const msg = h('div', {className: 'hint'});
  apply.onclick = () => post('/api/layout', {nx: +nx.value, ny: +ny.value, length: +len.value, aspect: +asp.value,
      measured: meas.checked, rowUni: uni.slice(0, +ny.value)})
    .then(st => { S = st; draw(); }).catch(e => { msg.textContent = e.message; });
  table();
  upd();
  return h('div', {className: 'card'},
    field('LEDs per row', nx), field('Rows', ny), field('Row length (m)', len),
    field('Aspect', asp), h('div', {className: 'row'}, h('label'), h('span', {className: 'hint', textContent: 'row spacing \u00f7 LED spacing'})),
    field('Measured rows', meas),
    h('div', {className: 'row'}, h('label', {textContent: 'Row universes'}), h('span', {className: 'hint', textContent: 'consecutive from'}), fillFrom, fill),
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
  const scan = h('button', {textContent: 'Scan'});
  scan.onclick = async () => {
    scan.disabled = true; msg.textContent = 'Scanning...';
    let r = await post('/api/scan', {});
    for (let k = 0; k < 30 && r.state == 1; k++) { await new Promise(res => setTimeout(res, 700)); r = await (await fetch('/api/scan')).json(); }
    scan.disabled = false;
    msg.textContent = r.state == 2 ? '' : 'Scan failed';
    nets.replaceChildren(...r.nets.map(n => { const b = h('button', {textContent: n.s + '  (' + n.r + ' dBm' + (n.l ? ', locked' : '') + ')'}); b.onclick = () => { ssid.value = n.s; wp.focus(); }; return b; }));
  };
  const save = h('button', {textContent: 'Save and restart', className: 'on'});
  save.onclick = () => post('/api/net', {name: name.value, fallback: fb.value, ssid: ssid.value, wifiPass: wp.value, apPass: ap.value})
    .then(() => { msg.textContent = 'Restarting... reconnect at http://' + name.value + '.local in a few seconds.'; })
    .catch(e => { msg.textContent = e.message; });
  return h('div', {className: 'card'},
    field('Name', name), field('Fallback IP', fb), field('Backup WiFi', ssid), field('WiFi password', wp),
    h('div', {className: 'row'}, h('label'), scan), nets, field('Hotspot password', ap),
    h('div', {className: 'row'}, h('label'), save), msg,
    h('p', {className: 'hint', textContent: 'DHCP first; with no answer after 10 s the fallback IP is used (keep it in the PixLite subnet, 192.168.0.x). On the fallback IP, or with the cable unplugged and no backup WiFi, the board opens a hotspot named after it: http://192.168.4.1.'}));
}

function showStatus() {
  const el = document.getElementById('status');
  if (el) el.textContent = S.fps + ' fps, render ' + S.renderMs + ' ms \u00b7 ' + S.net + ' \u00b7 WiFi: ' + S.wifi;
}

// Follow button presses on the board without disturbing a slider being dragged
setInterval(async () => {
  try {
    const st = await (await fetch('/api/status')).json();
    const redraw = st.current != S.current || st.on != S.on;
    Object.assign(S, st);
    if (redraw) draw(); else showStatus();
  } catch (e) {}
}, 2000);
load();
</script></body></html>)HTML";
