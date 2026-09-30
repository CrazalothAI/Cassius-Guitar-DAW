import { useEffect, useRef, useState } from 'react';
import { byId, formatValue, fromFraction, origin, snap, toFraction } from '../parameters.js';
import { beginGesture, endGesture, setParameter, useParameter } from '../parameterState.js';

// Dragging is relative: pressing a knob never changes it, so Master cannot jump on a click.
const dragPixels = 200, fineDivisor = 10;
const keySteps = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1, PageUp: 10, PageDown: -10 };
const hint = 'Drag up or down · Shift for fine control · Double-click to reset';

export default function Knob({ id, small = false, muted = false }) {
  const p = byId[id];
  const value = useParameter(id);
  // The value in hand during a drag or held key; host echoes of earlier values must not pull it back.
  const [pending, setPending] = useState(null);
  // Focus taken by a press shows no keyboard ring; Chromium treats our programmatic focus as focus-visible.
  const [pressed, setPressed] = useState(false);
  const gesture = useRef(false), drag = useRef(null), input = useRef(null);
  const shown = pending ?? value;

  const begin = () => { if (!gesture.current) { gesture.current = true; beginGesture(id); } };
  const end = () => {
    drag.current = null;
    if (gesture.current) { gesture.current = false; endGesture(id); setPending(null); }
  };
  useEffect(() => () => { if (gesture.current) endGesture(id); }, [id]);

  const change = raw => {
    const next = snap(p, raw), oneShot = !gesture.current;
    if (oneShot) begin(); else setPending(next);
    setParameter(id, next, { gesture: false });
    if (oneShot) end();
  };

  const pointerDown = e => {
    if (e.button !== 0) return;
    e.preventDefault();
    setPressed(true);
    input.current?.focus({ preventScroll: true });
    e.currentTarget.setPointerCapture?.(e.pointerId);
    drag.current = { x: e.clientX, y: e.clientY, fraction: toFraction(p, shown) };
  };
  const pointerMove = e => {
    const d = drag.current;
    if (!d) return;
    const pixels = (e.clientX - d.x) - (e.clientY - d.y);
    d.x = e.clientX; d.y = e.clientY;
    d.fraction = Math.min(1, Math.max(0, d.fraction + pixels / dragPixels / (e.shiftKey ? fineDivisor : 1)));
    const next = snap(p, fromFraction(p, d.fraction));
    // The host gesture starts with the first real change, so a click writes no automation.
    if (next !== shown) { begin(); change(next); }
  };
  const keyDown = e => {
    let next;
    if (e.key in keySteps)
    {
      // 1% or 10% of the knob's travel (not of the value range, which matters on skewed
      // knobs); Shift moves one step. A press always moves at least one step.
      const direction = Math.sign(keySteps[e.key]);
      next = e.shiftKey ? shown + direction * p.step : fromFraction(p, toFraction(p, shown) + keySteps[e.key] / 100);
      if (snap(p, next) === shown) next = shown + direction * p.step;
    }
    else if (e.key === 'Home') next = p.min;
    else if (e.key === 'End') next = p.max;
    else return;
    e.preventDefault();
    setPressed(false);
    begin();
    change(next);
  };

  const fraction = toFraction(p, shown), zero = toFraction(p, origin(p));
  const { text, unit } = formatValue(p, shown);
  return <label className={`knob${small ? ' small' : ''}${muted ? ' muted' : ''}`}>
    <span className="knob-label">{p.label}</span>
    <span className="dial" title={hint} data-pressed={pressed || undefined}
      style={{ '--angle': `${-135 + fraction * 270}deg`, '--from': `${Math.min(zero, fraction) * 270}deg`, '--to': `${Math.max(zero, fraction) * 270}deg` }}
      onPointerDown={pointerDown} onPointerMove={pointerMove} onPointerUp={end} onPointerCancel={end} onLostPointerCapture={end}
      onDoubleClick={() => change(p.initial)}>
      <span className="dial-face"><span className="needle" /></span>
      <input ref={input} aria-label={p.label} aria-valuetext={unit ? `${text} ${unit}` : text} type="range"
        min={p.min} max={p.max} step={p.step} value={shown}
        onKeyDown={keyDown} onKeyUp={end} onBlur={() => { setPressed(false); end(); }} onChange={e => change(Number(e.target.value))} />
    </span>
    <output>{text}{unit && <> <span>{unit}</span></>}</output>
  </label>;
}
