// dpad-gemuboy.js — D-pad "model B" taken verbatim from GBoy-JS (gemuboy-js,
// same author: https://github.com/gabom88/gemuboy-js, js/dpad.js).
// Only the SVG markup is reused; touch handling lives in touch.js.
// D-pad "model B": a 3D cross whose outer silhouette never moves. Pressing a
// direction lowers that side; the opposite (raised) side shows more side wall,
// which eats into the face. All 9 states (rest + 8 directions) are built once
// into a single SVG and only the current one is shown.
const DpadModelB = {
    CROSS: 'M37 7 H63 V37 H93 V63 H63 V93 H37 V63 H7 V37 H37 Z',
    TRI: {
        up: 'M50 17 L57 28 H43 Z',
        down: 'M50 83 L57 72 H43 Z',
        left: 'M17 50 L28 43 V57 Z',
        right: 'M83 50 L72 43 V57 Z',
    },
    LIFT: 4.5,
    STATES: {
        rest: [], up: ['up'], down: ['down'], left: ['left'], right: ['right'],
        ul: ['up', 'left'], ur: ['up', 'right'], dl: ['down', 'left'], dr: ['down', 'right'],
    },

    // State key for a set of pressed directions.
    key(state) {
        const v = state.up ? 'u' : state.down ? 'd' : '';
        const h = state.left ? 'l' : state.right ? 'r' : '';
        const map = { u: 'up', d: 'down', l: 'left', r: 'right' };
        if (v && h) {
            return v + h;
        }
        return map[v || h] || 'rest';
    },

    shape(x, y, fill) {
        return `<path d="${this.CROSS}" fill="${fill}" stroke="${fill}" stroke-width="4" stroke-linejoin="round" transform="translate(${x} ${y})"/>`;
    },

    state(name, pressed) {
        const id = 'dpb-' + name + '-';
        const L = this.LIFT;
        let dx = 0;
        let dy = 0;
        if (pressed.includes('up')) dy -= L;
        if (pressed.includes('down')) dy += L;
        if (pressed.includes('left')) dx -= L;
        if (pressed.includes('right')) dx += L;

        // Face region = intersection of the cross with its copies shifted along the
        // lift vector (horizontal, vertical and diagonal), so diagonals erode cleanly.
        const comps = [[0, 0], [dx, 0], [0, dy], [dx, dy]]
            .filter((c, i, all) => all.findIndex((o) => o[0] === c[0] && o[1] === c[1]) === i);
        const box = 'maskUnits="userSpaceOnUse" x="0" y="0" width="100" height="100"';
        const faceMasks = comps.map(([x, y], i) =>
            `<mask id="${id}m${i}" ${box}>${this.shape(x, y, '#fff')}</mask>`).join('');
        const nest = (inner) => comps.reduce((acc, _, i) => `<g mask="url(#${id}m${i})">${acc}</g>`, inner);

        // Bevel: light band on the upper-left edges, shadow band on the lower-right,
        // following the real (possibly eroded) face edge.
        const bands = { hl: [1, 1.5], sh: [-1, -1.5] };
        const bandMasks = Object.entries(bands).map(([k, [sx, sy]]) => comps.map(([x, y], i) =>
            `<mask id="${id}${k}${i}" ${box}><rect width="100" height="100" fill="#fff"/>${this.shape(x + sx, y + sy, '#000')}</mask>`).join('')).join('');
        const band = (k, color) => comps.map((_, i) =>
            `<rect width="100" height="100" fill="${color}" mask="url(#${id}${k}${i})"/>`).join('');

        // Engraved arrows; the pressed ones light up.
        const tris = Object.entries(this.TRI).map(([dir, d]) => pressed.includes(dir)
            ? `<path d="${d}" fill="#000" fill-opacity=".35" stroke="#000" stroke-opacity=".35" stroke-width="2" stroke-linejoin="round" transform="translate(0 .8)"/>
               <path d="${d}" fill="url(#dpb-lit)" stroke="url(#dpb-lit)" stroke-width="2" stroke-linejoin="round"/>`
            : `<path d="${d}" fill="#fff" fill-opacity=".1" stroke="#fff" stroke-opacity=".1" stroke-width="2" stroke-linejoin="round" transform="translate(.5 .7)"/>
               <path d="${d}" fill="#000" fill-opacity=".45" stroke="#000" stroke-opacity=".45" stroke-width="2" stroke-linejoin="round" transform="translate(-.3 -.4)"/>
               <path d="${d}" fill="#3c3c41" stroke="#3c3c41" stroke-width="1.4" stroke-linejoin="round"/>`).join('');

        // The lowered (pressed) side of the face is a little darker.
        let shade = '';
        let shadeGrad = '';
        if (pressed.length) {
            const vx = Math.sign(dx);
            const vy = Math.sign(dy);
            shadeGrad = `<linearGradient id="${id}sg" gradientUnits="userSpaceOnUse" x1="${50 - vx * 10}" y1="${50 - vy * 10}" x2="${50 + vx * 45}" y2="${50 + vy * 45}">
                <stop offset="0" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity=".28"/></linearGradient>`;
            shade = `<path d="${this.CROSS}" fill="url(#${id}sg)" stroke="url(#${id}sg)" stroke-width="4" stroke-linejoin="round"/>`;
        }

        // Definitions go to the root <defs>: masks inside hidden groups are unreliable in Safari.
        const defs = shadeGrad + bandMasks + faceMasks;
        const body = `<g class="st st-${name}">
            ${nest(`
                <path d="${this.CROSS}" fill="url(#dpb-face)" stroke="url(#dpb-face)" stroke-width="4" stroke-linejoin="round"/>
                <path d="${this.CROSS}" fill="url(#dpb-spec)" stroke="url(#dpb-spec)" stroke-width="4" stroke-linejoin="round"/>
                ${shade}
                <g opacity=".26">${band('hl', '#fff')}</g>
                <g opacity=".42">${band('sh', '#000')}</g>`)}
            <g transform="translate(${dx / 2} ${dy / 2})">
                ${tris}
                <circle cx="50" cy="50" r="9.6" fill="#fff" fill-opacity=".1" transform="translate(.4 .6)"/>
                <circle cx="50" cy="50" r="9.6" fill="url(#dpb-dimple)"/>
            </g>
        </g>`;
        return { defs, body };
    },

    markup() {
        const states = Object.entries(this.STATES).map(([name, pressed]) => this.state(name, pressed));
        return `<svg class="dpad-b" viewBox="0 0 100 100" aria-hidden="true">
            <defs>
                <linearGradient id="dpb-wall" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="0" stop-color="#55555b"/><stop offset=".18" stop-color="#2c2c30"/><stop offset="1" stop-color="#111113"/>
                </linearGradient>
                <linearGradient id="dpb-face" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="0" stop-color="#535359"/><stop offset=".55" stop-color="#46464c"/><stop offset="1" stop-color="#3a3a3f"/>
                </linearGradient>
                <radialGradient id="dpb-spec" gradientUnits="userSpaceOnUse" cx="26" cy="30" r="40">
                    <stop offset="0" stop-color="#fff" stop-opacity=".16"/><stop offset="1" stop-color="#fff" stop-opacity="0"/>
                </radialGradient>
                <radialGradient id="dpb-lit" cx=".5" cy=".4" r=".7">
                    <stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#c9c8cf"/>
                </radialGradient>
                <linearGradient id="dpb-dimple" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="0" stop-color="#26262a"/><stop offset="1" stop-color="#55555b"/>
                </linearGradient>
                ${states.map((st) => st.defs).join('')}
            </defs>
            <path d="${this.CROSS}" fill="url(#dpb-wall)" stroke="url(#dpb-wall)" stroke-width="11" stroke-linejoin="round"/>
            ${states.map((st) => st.body).join('')}
        </svg>`;
    },
};
