/* =============================================================
   CVKnob — Entropia Audio knob renderer
   Public API:  window.CVKnob.build(containerId, paramConfig)

   Vital-style conic-gradient arc with 3-pass layered glow,
   bound to a JUCE WebSliderRelay (or any object with same shape):
     {
       label:        "Decay",
       unit:         "s" | "%" | "Hz" | "dB" | "ms" | "" | custom,
       min:          0.1,
       max:          60,
       section:      "space",     // key into CVKnobConfig.sectionColors (optional)
       defaultNorm:  0.4,         // 0..1 — used for double-click / Cmd-click
       tip:          "Reverb tail",
       accentColor:  "#3ab8d4",   // optional direct override
       state:        wsliderRelay,
       lockState:    toggleRelay, // optional
       format:       (raw, norm) => string  // optional custom formatter
     }

   Globals consumed:
     window.CVKnobConfig.tooltipsEnabled  (bool, default true)
     window.CVKnobConfig.sectionColors    (object — section → "#rrggbb")
   ============================================================= */

(function (global) {
  'use strict';

  const DPR = window.devicePixelRatio || 1;
  const KNOB_SIZE = 44;        // canvas drawing area (oversized for glow)
  const ARC_RADIUS = 14;
  const ARC_START = 135;
  const ARC_END = 405;         // 135 + 270 = 270° sweep

  function degToRad(d) { return d * Math.PI / 180; }

  function lerpColor(a, b, t) {
    t = Math.max(0, Math.min(1, t));
    const ar = parseInt(a.slice(1, 3), 16);
    const ag = parseInt(a.slice(3, 5), 16);
    const ab = parseInt(a.slice(5, 7), 16);
    const br = parseInt(b.slice(1, 3), 16);
    const bg = parseInt(b.slice(3, 5), 16);
    const bb = parseInt(b.slice(5, 7), 16);
    return `rgb(${Math.round(ar+(br-ar)*t)},${Math.round(ag+(bg-ag)*t)},${Math.round(ab+(bb-ab)*t)})`;
  }

  /* 3-pass layered glow canvas renderer.
     Photoshop "Outer Glow" approximation:
       pass 1: blur 14, alpha 0.35 (outer halo)
       pass 2: blur 7,  alpha 0.60 (mid glow)
       pass 3: blur 2,  alpha 1.00 (crisp inner arc)
     Cyan→accent gradient drawn segment-by-segment along arc. */
  function drawKnob(canvas, normValue, accentColor, locked) {
    const ctx = canvas.getContext('2d');
    ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
    const cx = KNOB_SIZE / 2, cy = KNOB_SIZE / 2;
    const r = ARC_RADIUS, arcWidth = 1.2;
    ctx.clearRect(0, 0, KNOB_SIZE, KNOB_SIZE);

    const startRad = degToRad(ARC_START);
    const endRad   = degToRad(ARC_END);

    // 1. Background arc track (warm dark)
    ctx.beginPath();
    ctx.arc(cx, cy, r, startRad, endRad);
    ctx.strokeStyle = '#1f1c19';
    ctx.lineWidth = arcWidth;
    ctx.lineCap = 'round';
    ctx.stroke();

    if (normValue <= 0.001) return;

    // 2. Value arc — cyan→accent gradient (segment-by-segment)
    const totalAngle = normValue * 270;
    const segments = Math.max(2, Math.round(totalAngle * 0.5));

    const strokeArc = () => {
      for (let i = 0; i < segments; i++) {
        const t0 = i / segments;
        const t1 = (i + 1) / segments;
        const segStart = degToRad(ARC_START + t0 * totalAngle);
        const segEnd   = degToRad(ARC_START + t1 * totalAngle) + 0.012;
        const c = lerpColor('#00d4ff', accentColor, (t0 + t1) * 0.5);
        ctx.beginPath();
        ctx.arc(cx, cy, r, segStart, segEnd);
        ctx.strokeStyle = locked ? '#666' : c;
        ctx.lineWidth = arcWidth;
        ctx.lineCap = (i === 0 || i === segments - 1) ? 'round' : 'butt';
        ctx.stroke();
      }
    };

    if (locked) {
      ctx.save();
      ctx.shadowBlur = 0;
      strokeArc();
      ctx.restore();
      return;
    }

    // 3-pass glow
    ctx.save();
    ctx.shadowColor = accentColor;
    ctx.shadowBlur = 14;
    ctx.globalAlpha = 0.35;
    strokeArc();
    ctx.restore();

    ctx.save();
    ctx.shadowColor = accentColor;
    ctx.shadowBlur = 7;
    ctx.globalAlpha = 0.6;
    strokeArc();
    ctx.restore();

    ctx.save();
    ctx.shadowColor = accentColor;
    ctx.shadowBlur = 2;
    ctx.globalAlpha = 1.0;
    strokeArc();
    ctx.restore();
  }

  /* Default value formatter — override per-knob via config.format(raw, norm) */
  function formatValue(config, normValue) {
    const v = config.min + normValue * (config.max - config.min);
    if (typeof config.format === 'function') return config.format(v, normValue);
    if (config.unit === 's')  return v.toFixed(1) + 's';
    if (config.unit === 'ms') return Math.round(v) + 'ms';
    if (config.unit === 'Hz') {
      if (v >= 1000) return (v / 1000).toFixed(1) + 'kHz';
      if (v < 1)     return v.toFixed(2) + 'Hz';
      return Math.round(v) + 'Hz';
    }
    if (config.unit === '%')  return Math.round(v) + '%';
    if (config.unit === 'dB') return (v > 0 ? '+' : '') + v.toFixed(1) + 'dB';
    if (!config.unit)         return Math.round(v).toString();
    return Math.round(v) + (config.unit || '');
  }

  let isDragging = false;

  function build(containerId, p) {
    const container = (typeof containerId === 'string')
      ? document.getElementById(containerId)
      : containerId;
    if (!container) return null;

    const sectionColors = (global.CVKnobConfig && global.CVKnobConfig.sectionColors) || {};
    const accentColor = p.accentColor || sectionColors[p.section] || '#888888';

    const wrap = document.createElement('div');
    wrap.className = 'knob-wrap';
    wrap.setAttribute('data-param', p.id || p.label);

    const bodyEl = document.createElement('div');
    bodyEl.className = 'knob-body';

    const canvas = document.createElement('canvas');
    canvas.width = KNOB_SIZE * DPR;
    canvas.height = KNOB_SIZE * DPR;
    canvas.style.cssText = 'position:absolute;top:-6px;left:-6px;width:44px;height:44px;';
    bodyEl.appendChild(canvas);

    const face = document.createElement('div');
    face.className = 'knob-face';

    const indicator = document.createElement('div');
    indicator.className = 'knob-indicator';
    indicator.style.background = accentColor;
    face.appendChild(indicator);
    bodyEl.appendChild(face);

    const labelEl = document.createElement('div');
    labelEl.className = 'knob-label';
    labelEl.textContent = p.label || '';

    const valueEl = document.createElement('div');
    valueEl.className = 'knob-value';

    // Optional tooltip
    if (p.tip) {
      const tipEl = document.createElement('div');
      tipEl.className = 'knob-tooltip';
      tipEl.textContent = p.tip;
      document.body.appendChild(tipEl);

      wrap.addEventListener('mouseenter', () => {
        const cfg = global.CVKnobConfig || {};
        if (isDragging || cfg.tooltipsEnabled === false) return;
        const rect = wrap.getBoundingClientRect();
        tipEl.style.left = (rect.left + rect.width / 2) + 'px';
        tipEl.style.top = (rect.top - 8) + 'px';
        tipEl.style.transform = 'translate(-50%, -100%)';
        tipEl.classList.add('visible');
      });
      wrap.addEventListener('mouseleave', () => tipEl.classList.remove('visible'));
    }

    wrap.appendChild(bodyEl);
    wrap.appendChild(labelEl);
    wrap.appendChild(valueEl);
    container.appendChild(wrap);

    // Refresh from state
    function refresh() {
      const norm = p.state.getNormalisedValue();
      const locked = p.lockState ? p.lockState.getValue() : false;

      drawKnob(canvas, norm, accentColor, locked);

      const rot = -135 + norm * 270;
      indicator.style.transform = `rotate(${rot}deg) translateY(-7px)`;
      indicator.style.background = locked ? '#aaaaaa' : '#ffffff';
      indicator.style.boxShadow = locked ? 'none' : `0 0 4px ${accentColor}`;
      indicator.style.opacity = locked ? '0.85' : '1';

      if (locked) {
        face.style.background = '#181612';
        face.style.borderColor = '#2e2a25';
        face.style.opacity = '0.5';
        bodyEl.classList.add('locked');
      } else {
        face.style.background = '#1f1c19';
        face.style.borderColor = '#3e3a35';
        face.style.opacity = '1';
        bodyEl.classList.remove('locked');
      }

      valueEl.textContent = formatValue(p, norm);
      valueEl.style.color = locked ? '#444' : 'var(--text-secondary)';
    }

    // rAF-throttled redraws — multiple events per frame collapse to one draw
    let pendingFrame = null;
    const scheduleRefresh = () => {
      if (pendingFrame !== null) return;
      pendingFrame = requestAnimationFrame(() => {
        pendingFrame = null;
        refresh();
      });
    };
    p.state.valueChangedEvent.addListener(scheduleRefresh);
    if (p.lockState) p.lockState.valueChangedEvent.addListener(scheduleRefresh);
    refresh();

    // Interactions
    const getCurrent = () => p.state.getNormalisedValue();
    const defaultNorm = (typeof p.defaultNorm === 'number') ? p.defaultNorm : 0.5;
    let lastY = null;

    wrap.addEventListener('mousedown', (e) => {
      if (e.metaKey || e.ctrlKey) {
        e.preventDefault();
        e.stopPropagation();
        p.state.setNormalisedValue(defaultNorm);
        return;
      }
      e.preventDefault();
      lastY = e.clientY;
      isDragging = true;
      document.querySelectorAll('.knob-tooltip.visible').forEach(t => t.classList.remove('visible'));
      document.body.style.cursor = 'ns-resize';

      const onMove = (me) => {
        const deltaY = lastY - me.clientY;
        lastY = me.clientY;
        const newNorm = Math.max(0, Math.min(1, getCurrent() + deltaY * 0.005));
        p.state.setNormalisedValue(newNorm);
      };
      const onUp = () => {
        isDragging = false;
        document.body.style.cursor = 'default';
        lastY = null;
        document.removeEventListener('mousemove', onMove);
        document.removeEventListener('mouseup', onUp);
      };
      document.addEventListener('mousemove', onMove);
      document.addEventListener('mouseup', onUp);
    });

    wrap.addEventListener('wheel', (e) => {
      e.preventDefault();
      const direction = e.deltaY < 0 ? 1 : -1;
      p.state.setNormalisedValue(Math.max(0, Math.min(1, getCurrent() + direction * 0.01)));
    }, { passive: false });

    wrap.addEventListener('dblclick', (e) => {
      e.preventDefault();
      p.state.setNormalisedValue(defaultNorm);
    });

    if (p.lockState) {
      wrap.addEventListener('contextmenu', (e) => {
        e.preventDefault();
        p.lockState.setValue(!p.lockState.getValue());
        return false;
      });
    }

    return { refresh, wrap };
  }

  global.CVKnob = {
    build,
    drawKnob,
    lerpColor,
    KNOB_SIZE,
    ARC_RADIUS,
    ARC_START,
    ARC_END
  };

})(window);
