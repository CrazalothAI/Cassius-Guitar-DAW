import { useEffect, useRef, useState } from 'react';
import { byId } from '../parameters.js';
import { slider } from '../juce/bridge.js';
import { useParameter, setParameter } from '../parameterState.js';
export default function Knob({ id, small = false }) {
  const p = byId[id];
  const [value, setValue] = useState(p.initial);
  const external = useParameter(id);
  useEffect(() => setValue(external), [external]);
  const state = slider(id);
  const dragging = useRef(false);
  const begin = () => { if (!dragging.current) { dragging.current = true; state?.sliderDragStarted(); } };
  const end = () => { if (dragging.current) { dragging.current = false; state?.sliderDragEnded(); } };
  useEffect(() => {
    return () => {
      if (dragging.current) { state?.sliderDragEnded(); dragging.current = false; }
    };
  }, [state]);
  const change = next => {
    const oneShot = !dragging.current;
    if (oneShot) begin();
    setValue(next);
    state?.setNormalisedValue((next - p.min) / (p.max - p.min));
    if (!state) setParameter(id, next);
    if (oneShot) end();
  };
  const fraction = Math.max(0, Math.min(1, (value - p.min) / (p.max - p.min)));
  return <label className={`knob ${small ? 'small' : ''}`}>
    <span className="knob-label">{p.label}</span>
    <span className="dial" style={{ '--angle': `${-135 + fraction * 270}deg`, '--fill': `${fraction * 270}deg` }}>
      <span className="dial-face"><span className="needle" /></span>
      <input aria-label={p.label} aria-valuetext={`${value.toFixed(1)} ${p.unit}`} type="range" min={p.min} max={p.max} step="0.1" value={value}
        onPointerDown={e => { e.currentTarget.setPointerCapture(e.pointerId); begin(); }}
        onPointerUp={end} onPointerCancel={end} onLostPointerCapture={end} onBlur={end}
        onKeyDown={e => { if (['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Home', 'End', 'PageUp', 'PageDown'].includes(e.key)) begin(); }}
        onKeyUp={end} onChange={e => change(Number(e.target.value))} onDoubleClick={() => change(p.initial)} />
    </span>
    <output>{value.toFixed(p.unit === 'ms' ? 0 : 1)} <span>{p.unit}</span></output>
  </label>;
}
