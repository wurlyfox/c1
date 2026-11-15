// touch.js — configurable on-screen controller for phones and tablets.
//
// Ported from ReChan Web (https://github.com/gabom88/ReChan, web/shell/js/touch.js)
// and adapted to the PlayStation pad that Crash Bandicoot expects.
//
// Landscape and portrait each have their own layout (in portrait the game
// sits at the top and the controls below it, handheld-style). Every control
// can be moved, resized or hidden from the in-app editor; layouts, opacity
// and the d-pad/stick choice are saved in localStorage. The pressed state is
// reported as a set of pad button names; c1web.js merges it with the keyboard
// and physical gamepads, so the engine needs no changes for touch.
//
// The d-pad is GBoy-JS's 3D "model B" cross (js/dpad-gemuboy.js).
'use strict';

const C1Touch = (() => {
  const STORAGE_KEY = 'c1web.touch.v1';

  // 24x24 line icons (stroke = currentColor): the PlayStation face symbols.
  const ICONS = {
    cross: '<path d="M6 6l12 12M18 6 6 18" stroke-width="2.4"/>',
    square: '<rect x="5.5" y="5.5" width="13" height="13" rx="1" stroke-width="2.2"/>',
    circle: '<circle cx="12" cy="12" r="7.2" stroke-width="2.2"/>',
    triangle: '<path d="M12 4.8 20 18.6H4z" stroke-width="2.2"/>',
    start: '<path d="M8 5.5v13l11-6.5z" fill="currentColor" stroke="none"/>',
    select: '<rect x="5" y="8.5" width="14" height="7" rx="1.5"/>',
  };

  function iconSvg(name) {
    return `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${ICONS[name] || ''}</svg>`;
  }

  // x/y: centre as a fraction of the viewport; size: fraction of the
  // viewport's shorter side. bind: pad button name (see KEYS in c1web.js).
  const CONTROL_DEFS = [
    { id: 'dpad',     type: 'dpad',   label: 'D-pad' },
    { id: 'stick',    type: 'stick',  label: 'Stick' },
    { id: 'cross',    type: 'button', bind: 'cross',    icon: 'cross',    label: 'Cross (jump)' },
    { id: 'circle',   type: 'button', bind: 'circle',   icon: 'circle',   label: 'Circle (spin)' },
    { id: 'square',   type: 'button', bind: 'square',   icon: 'square',   label: 'Square (spin)' },
    { id: 'triangle', type: 'button', bind: 'triangle', icon: 'triangle', label: 'Triangle' },
    { id: 'l1',       type: 'button', bind: 'l1',       text: 'L1',       label: 'L1', minor: true },
    { id: 'r1',       type: 'button', bind: 'r1',       text: 'R1',       label: 'R1', minor: true },
    { id: 'l2',       type: 'button', bind: 'l2',       text: 'L2',       label: 'L2', minor: true },
    { id: 'r2',       type: 'button', bind: 'r2',       text: 'R2',       label: 'R2', minor: true },
    { id: 'select',   type: 'button', bind: 'select',   icon: 'select',   label: 'Select', small: true },
    { id: 'start',    type: 'button', bind: 'start',    icon: 'start',    label: 'Start',  small: true },
  ];

  // The shoulder buttons are not used by the game: hidden until "Show all".
  const DEFAULT_LAYOUTS = {
    landscape: {
      dpad:     { x: 0.15, y: 0.67, size: 0.40, visible: true },
      stick:    { x: 0.15, y: 0.67, size: 0.40, visible: false },
      cross:    { x: 0.86, y: 0.83, size: 0.16, visible: true },
      circle:   { x: 0.95, y: 0.64, size: 0.16, visible: true },
      square:   { x: 0.77, y: 0.64, size: 0.16, visible: true },
      triangle: { x: 0.86, y: 0.45, size: 0.16, visible: true },
      l1:       { x: 0.06, y: 0.31, size: 0.12, visible: false },
      r1:       { x: 0.94, y: 0.27, size: 0.12, visible: false },
      l2:       { x: 0.06, y: 0.12, size: 0.12, visible: false },
      r2:       { x: 0.94, y: 0.11, size: 0.12, visible: false },
      select:   { x: 0.40, y: 0.09, size: 0.10, visible: true },
      start:    { x: 0.60, y: 0.09, size: 0.10, visible: true },
    },
    // The game occupies the top 4:3 band (see .portrait-play in c1web.css).
    portrait: {
      dpad:     { x: 0.26, y: 0.72, size: 0.44, visible: true },
      stick:    { x: 0.26, y: 0.72, size: 0.44, visible: false },
      cross:    { x: 0.75, y: 0.81, size: 0.19, visible: true },
      circle:   { x: 0.90, y: 0.71, size: 0.19, visible: true },
      square:   { x: 0.60, y: 0.71, size: 0.19, visible: true },
      triangle: { x: 0.75, y: 0.61, size: 0.19, visible: true },
      l1:       { x: 0.30, y: 0.50, size: 0.15, visible: false },
      r1:       { x: 0.70, y: 0.50, size: 0.15, visible: false },
      l2:       { x: 0.10, y: 0.50, size: 0.15, visible: false },
      r2:       { x: 0.90, y: 0.50, size: 0.15, visible: false },
      select:   { x: 0.40, y: 0.93, size: 0.12, visible: true },
      start:    { x: 0.60, y: 0.93, size: 0.12, visible: true },
    },
  };

  const DEFAULT_SETTINGS = { opacity: 0.7, mode: 'dpad', haptics: true };

  let layouts = {};
  let settings = {};
  let layer = null;
  let editing = false;
  let selectedId = null;
  let shown = false;
  const elements = new Map();
  const pointers = new Map(); // pointerId -> { id, ... }
  let onSelect = null;
  let onState = null; // called with the list of pressed pad button names

  function clone(o) { return JSON.parse(JSON.stringify(o)); }

  // Orientation comes from the screen, not from the layer's size: on iOS a
  // layout that overflowed during a rotation could widen the layout viewport
  // (and the fixed layer with it), which then kept selecting the landscape
  // layout in portrait with oversized controls.
  const portraitQuery = window.matchMedia ? window.matchMedia('(orientation: portrait)') : null;
  function orientation() {
    if (portraitQuery) return portraitQuery.matches ? 'portrait' : 'landscape';
    return window.innerHeight > window.innerWidth ? 'portrait' : 'landscape';
  }

  // Control definition merged with the current orientation's placement.
  function controls() {
    const layout = layouts[orientation()];
    return CONTROL_DEFS.map((d) => {
      const c = Object.assign({}, d, layout[d.id]);
      c.active = c.visible;
      return c;
    });
  }
  function find(id) { return controls().find((c) => c.id === id) || null; }

  function load() {
    layouts = clone(DEFAULT_LAYOUTS);
    settings = { ...DEFAULT_SETTINGS };
    try {
      const saved = JSON.parse(localStorage.getItem(STORAGE_KEY) || 'null');
      if (saved) {
        Object.assign(settings, saved.settings || {});
        for (const o of ['landscape', 'portrait']) {
          const so = (saved.layouts || {})[o] || {};
          for (const id in so) {
            if (layouts[o][id]) Object.assign(layouts[o][id], so[id]);
          }
        }
      }
    } catch (_) { /* corrupt or blocked storage: use defaults */ }
    applyMode();
  }

  function save() {
    try { localStorage.setItem(STORAGE_KEY, JSON.stringify({ settings, layouts })); } catch (_) { /* ignore */ }
  }

  function applyMode() {
    for (const o of ['landscape', 'portrait']) {
      layouts[o].dpad.visible = settings.mode !== 'stick';
      layouts[o].stick.visible = settings.mode === 'stick';
    }
  }

  // All geometry is relative to the touch layer itself, never to the window:
  // on iOS Safari the fixed layer can be offset from / sized differently to
  // window.inner* (toolbars, standalone status bar), which shifted the touch
  // areas away from the drawn buttons.
  function layerRect() {
    return layer ? layer.getBoundingClientRect() : { left: 0, top: 0, width: window.innerWidth, height: window.innerHeight };
  }
  function W() { return (layer && layer.clientWidth) || layerRect().width || window.innerWidth; }
  function H() { return (layer && layer.clientHeight) || layerRect().height || window.innerHeight; }
  function lx(e) { return e.clientX - layerRect().left; }
  function ly(e) { return e.clientY - layerRect().top; }

  function unit() { return Math.min(W(), H()); }

  function build() {
    layer.innerHTML = '';
    elements.clear();
    for (const c of CONTROL_DEFS) {
      const el = document.createElement('div');
      el.className = 'tc tc-' + c.type + (c.minor ? ' tc-minor' : '') + (c.small ? ' tc-small' : '');
      el.dataset.id = c.id;
      if (c.type === 'stick') {
        el.innerHTML = '<div class="tc-knob"></div>';
      } else if (c.type === 'dpad') {
        el.classList.add('ctrl-dpad');
        el.dataset.st = 'rest';
        el.innerHTML = (typeof DpadModelB !== 'undefined') ? DpadModelB.markup() : '';
      } else {
        el.innerHTML = c.icon ? `<span class="tc-icon">${iconSvg(c.icon)}</span>` : `<span class="tc-text">${c.text}</span>`;
        el.setAttribute('aria-label', c.label);
      }
      layer.appendChild(el);
      elements.set(c.id, el);
    }
    layout();
  }

  function layout() {
    if (!layer) return;
    const u = unit();
    layer.style.setProperty('--tc-opacity', settings.opacity);
    for (const c of controls()) {
      const el = elements.get(c.id);
      if (!el) continue;
      const s = c.size * u;
      el.style.width = s + 'px';
      el.style.height = s + 'px';
      // Keep every control fully inside the layer so nothing can overflow
      // (and widen the page) while a rotation is in progress.
      const w = W(), hgt = H();
      el.style.left = Math.max(0, Math.min(w - s, c.x * w - s / 2)) + 'px';
      el.style.top = Math.max(0, Math.min(hgt - s, c.y * hgt - s / 2)) + 'px';
      el.style.fontSize = (s * 0.2) + 'px';
      el.style.display = c.active ? '' : 'none';
      el.classList.toggle('tc-selected', editing && c.id === selectedId);
    }
  }

  function controlAt(x, y, slop) {
    const u = unit();
    let best = null;
    let bestD = Infinity;
    for (const c of controls()) {
      if (!c.active) continue;
      const r = c.size * u / 2 * slop;
      const dx = (x - c.x * W()) / r;
      const dy = (y - c.y * H()) / r;
      const d = dx * dx + dy * dy;
      if (d <= 1 && d < bestD) { best = c; bestD = d; }
    }
    return best;
  }

  function buzz() {
    if (settings.haptics && navigator.vibrate) {
      try { navigator.vibrate(8); } catch (_) { /* ignore */ }
    }
  }

  // Recompute the pad state from every active pointer.
  const STICK_THRESHOLD = 0.35; // the game only has digital directions
  function recompute() {
    const down = {};
    const dirs = { up: false, down: false, left: false, right: false };
    for (const p of pointers.values()) {
      const c = find(p.id);
      if (!c) continue;
      if (c.type === 'button') {
        down[c.bind] = true;
      } else if (c.type === 'stick') {
        if ((p.ax || 0) < -STICK_THRESHOLD) dirs.left = true;
        if ((p.ax || 0) > STICK_THRESHOLD) dirs.right = true;
        if ((p.ay || 0) < -STICK_THRESHOLD) dirs.up = true;
        if ((p.ay || 0) > STICK_THRESHOLD) dirs.down = true;
      } else if (c.type === 'dpad') {
        for (const d of (p.dirs || [])) dirs[d] = true;
      }
    }
    for (const d in dirs) if (dirs[d]) down[d] = true;
    if (onState) onState(Object.keys(down));
    for (const [id, el] of elements) {
      let pressed = false;
      for (const p of pointers.values()) if (p.id === id) pressed = true;
      el.classList.toggle('tc-pressed', pressed);
    }
    const dpadEl = elements.get('dpad');
    if (dpadEl) dpadEl.dataset.st = (typeof DpadModelB !== 'undefined') ? DpadModelB.key(dirs) : 'rest';
  }

  function updateStick(p, c, x, y) {
    const r = c.size * unit() / 2;
    let dx = (x - c.x * W()) / r;
    let dy = (y - c.y * H()) / r;
    const len = Math.hypot(dx, dy);
    if (len > 1) { dx /= len; dy /= len; }
    p.ax = Math.abs(dx) < 0.08 ? 0 : dx;
    p.ay = Math.abs(dy) < 0.08 ? 0 : dy;
    const knob = elements.get(c.id).querySelector('.tc-knob');
    knob.style.transform = `translate(${dx * 50}%, ${dy * 50}%)`;
  }

  function resetStickKnob(id) {
    const el = elements.get(id);
    const knob = el && el.querySelector('.tc-knob');
    if (knob) knob.style.transform = '';
  }

  function updateDpad(p, c, x, y) {
    const dx = x - c.x * W();
    const dy = y - c.y * H();
    const dead = c.size * unit() * 0.1;
    const dirs = [];
    if (Math.hypot(dx, dy) > dead) {
      const a = Math.atan2(dy, dx) * 180 / Math.PI; // 0 = right, 90 = down
      if (a > -67.5 && a < 67.5) dirs.push('right');
      if (a > 22.5 && a < 157.5) dirs.push('down');
      if (a > 112.5 || a < -112.5) dirs.push('left');
      if (a > -157.5 && a < -22.5) dirs.push('up');
    }
    const key = dirs.join('+');
    if (key !== p.dirKey && key) buzz();
    p.dirKey = key;
    p.dirs = dirs;
  }

  // --- play mode pointer handling ---
  function onDown(e) {
    if (editing) return editDown(e);
    const c = controlAt(lx(e), ly(e), 1.2);
    if (!c) return;
    e.preventDefault();
    try { layer.setPointerCapture(e.pointerId); } catch (_) { /* ignore */ }
    const p = { id: c.id };
    pointers.set(e.pointerId, p);
    if (c.type === 'stick') updateStick(p, c, lx(e), ly(e));
    else if (c.type === 'dpad') updateDpad(p, c, lx(e), ly(e));
    else buzz();
    recompute();
  }

  function onMove(e) {
    if (editing) return editMove(e);
    const p = pointers.get(e.pointerId);
    if (!p) return;
    e.preventDefault();
    const c = find(p.id);
    if (!c) return;
    if (c.type === 'stick') updateStick(p, c, lx(e), ly(e));
    else if (c.type === 'dpad') updateDpad(p, c, lx(e), ly(e));
    else {
      // Let a thumb slide across the action buttons (e.g. jump -> spin).
      const over = controlAt(lx(e), ly(e), 1.0);
      if (over && over.type === 'button' && over.id !== p.id) { p.id = over.id; buzz(); }
    }
    recompute();
  }

  function onUp(e) {
    if (editing) return editUp(e);
    const p = pointers.get(e.pointerId);
    if (!p) return;
    pointers.delete(e.pointerId);
    resetStickKnob(p.id);
    recompute();
  }

  // --- edit mode ---
  let drag = null;
  function editDown(e) {
    e.preventDefault();
    const c = controlAt(lx(e), ly(e), 1.1);
    selectedId = c ? c.id : null;
    if (c) {
      drag = { pointerId: e.pointerId, id: c.id,
               ox: lx(e) - c.x * W(), oy: ly(e) - c.y * H() };
      try { layer.setPointerCapture(e.pointerId); } catch (_) { /* ignore */ }
    }
    layout();
    if (onSelect) onSelect(selectedId ? find(selectedId) : null);
  }
  function editMove(e) {
    if (!drag || drag.pointerId !== e.pointerId) return;
    e.preventDefault();
    const place = layouts[orientation()][drag.id];
    place.x = Math.min(0.98, Math.max(0.02, (lx(e) - drag.ox) / W()));
    place.y = Math.min(0.98, Math.max(0.02, (ly(e) - drag.oy) / H()));
    layout();
  }
  function editUp(e) {
    if (drag && drag.pointerId === e.pointerId) { drag = null; save(); }
  }

  function init(layerEl, stateCallback) {
    layer = layerEl;
    onState = stateCallback || null;
    load();
    build();
    layer.addEventListener('pointerdown', onDown, { passive: false });
    layer.addEventListener('pointermove', onMove, { passive: false });
    layer.addEventListener('pointerup', onUp);
    layer.addEventListener('pointercancel', onUp);
    layer.addEventListener('lostpointercapture', onUp);
    // iOS: stop double-tap zoom, long-press callouts and rubber-banding.
    layer.addEventListener('touchstart', (e) => e.preventDefault(), { passive: false });
    layer.addEventListener('touchmove', (e) => e.preventDefault(), { passive: false });
    // Re-layout whenever the layer's real size changes. iOS fires "resize"
    // (and "orientationchange") before the rotation has settled, so layout
    // computed then used the old size/orientation and stayed wrong; a
    // ResizeObserver fires once the new size is actually applied.
    let lastOrientation = orientation();
    const relayout = () => {
      const o = orientation();
      if (o !== lastOrientation) {
        // Orientation flips swap layouts: drop presses that belonged to the old one.
        lastOrientation = o;
        pointers.clear();
        recompute();
      }
      layout();
    };
    if (typeof ResizeObserver !== 'undefined') new ResizeObserver(relayout).observe(layer);
    const settle = () => { relayout(); setTimeout(relayout, 250); setTimeout(relayout, 600); };
    window.addEventListener('resize', settle);
    window.addEventListener('orientationchange', settle);
    if (window.visualViewport) window.visualViewport.addEventListener('resize', settle);
    if (portraitQuery && portraitQuery.addEventListener) portraitQuery.addEventListener('change', settle);
  }

  function show(on) {
    shown = !!on;
    layer.classList.toggle('tc-hidden', !shown);
    if (!shown) { pointers.clear(); recompute(); }
  }

  function setEditing(on, callbacks) {
    editing = !!on;
    selectedId = null;
    pointers.clear();
    recompute();
    layer.classList.toggle('tc-editing', editing);
    if (callbacks) onSelect = callbacks.onSelect || null;
    if (editing) show(true);
    layout();
  }

  function updateSelected(changes) {
    if (!selectedId) return;
    Object.assign(layouts[orientation()][selectedId], changes);
    save();
    layout();
  }

  function setSetting(key, value) {
    settings[key] = value;
    if (key === 'mode') applyMode();
    save();
    layout();
  }

  // Resets the layout of the current orientation (and the shared settings).
  function reset() {
    const o = orientation();
    layouts[o] = clone(DEFAULT_LAYOUTS[o]);
    settings = { ...DEFAULT_SETTINGS };
    applyMode();
    save();
    build();
  }

  function showAll() {
    const layout_ = layouts[orientation()];
    for (const id in layout_) {
      if (id === 'stick' || id === 'dpad') continue;
      layout_[id].visible = true;
    }
    applyMode();
    save();
    layout();
  }

  return {
    init, show, setEditing, updateSelected, setSetting, reset, showAll, orientation,
    get settings() { return settings; },
    get shown() { return shown; },
    get editing() { return editing; },
    get selected() { return selectedId ? find(selectedId) : null; },
  };
})();
