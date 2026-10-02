import { useEffect, useRef, useState } from 'react';
import Knob from './Knob.jsx';
import { setParameter, useParameters } from '../parameterState.js';

const clampBpm = bpm => Math.max(40, Math.min(240, Math.round(bpm)));

// Header button: tempo and a light on every click. The panel opens beneath it.
export function MetronomeButton({ open, onToggle, status }) {
  const { METRO_ON, METRO_BPM } = useParameters(['METRO_ON', 'METRO_BPM']);
  const on = METRO_ON >= .5;
  const flash = useFlash(on ? status.metronomeClicks : null);
  const bpm = status.metronomeFollowsHost ? status.metronomeBpm : METRO_BPM;
  return <button className={`metro-toggle${on ? ' on' : ''}`} aria-expanded={open} aria-controls="metronome" aria-label="Metronome" onClick={onToggle}>
    <i className={`metro-led${flash ? ' flash' : ''}${flash && status.metronomeBeat === 0 ? ' accent' : ''}`} aria-hidden="true" />
    <span>{on ? `${Math.round(bpm)} BPM` : 'METRONOME'}</span>
  </button>;
}

// True for a moment each time `count` changes.
function useFlash(count) {
  const [flash, setFlash] = useState(false);
  const last = useRef(count);
  useEffect(() => {
    if (count === null || count === last.current) { last.current = count; return; }
    last.current = count; setFlash(true);
    const t = setTimeout(() => setFlash(false), 110);
    return () => clearTimeout(t);
  }, [count]);
  return flash;
}

export default function MetronomePanel({ status, onClose }) {
  const values = useParameters(['METRO_ON', 'METRO_BPM', 'METRO_BEATS']);
  const on = values.METRO_ON >= .5, beats = Math.round(values.METRO_BEATS);
  const taps = useRef([]);
  const tap = () => {
    const now = performance.now();
    // A pause of two seconds starts a new count.
    taps.current = [...taps.current.filter(t => now - t < 2000), now].slice(-5);
    if (taps.current.length < 2) return;
    const span = (taps.current.at(-1) - taps.current[0]) / (taps.current.length - 1);
    setParameter('METRO_BPM', clampBpm(60000 / span));
  };
  const nudge = by => setParameter('METRO_BPM', clampBpm(values.METRO_BPM + by));
  const keyDown = e => { if (e.key === 'Escape') { e.stopPropagation(); onClose(); } };
  const host = status.metronomeFollowsHost;
  return <div id="metronome" className="metro-panel" role="group" aria-label="Metronome settings" onKeyDown={keyDown}>
    <div className="metro-main">
      <button className={`metro-power${on ? ' on' : ''}`} aria-pressed={on} aria-label="Click" onClick={() => setParameter('METRO_ON', on ? 0 : 1)}>
        <i aria-hidden="true" />{on ? 'On' : 'Off'}
      </button>
      <div className="metro-tempo">
        <button className="metro-nudge" aria-label="Slower" onClick={() => nudge(-1)} disabled={host}>−</button>
        <output aria-live="off">{Math.round(host ? status.metronomeBpm : values.METRO_BPM)}<small>BPM</small></output>
        <button className="metro-nudge" aria-label="Faster" onClick={() => nudge(1)} disabled={host}>+</button>
      </div>
      <button className="text-button metro-tap" onClick={tap} disabled={host}>Tap</button>
    </div>
    <div className="metro-beats" aria-hidden="true">
      {Array.from({ length: beats }, (_, i) => <i key={i} className={`${i === 0 ? 'down' : ''}${on && status.metronomeBeat === i ? ' now' : ''}`} />)}
    </div>
    <div className="metro-knobs">
      <Knob id="METRO_BPM" small />
      <label className="metro-select">
        <span>Beats</span>
        <select aria-label="Beats per bar" value={beats} onChange={e => setParameter('METRO_BEATS', Number(e.target.value))}>
          {Array.from({ length: 12 }, (_, i) => <option key={i + 1} value={i + 1}>{i + 1}</option>)}
        </select>
      </label>
      <Knob id="METRO_LEVEL" small />
    </div>
    <p className="metro-note">{host ? 'Following your DAW’s tempo and bars while it plays.' : 'Accent on beat one · in a DAW it follows the song while playing.'}</p>
  </div>;
}
