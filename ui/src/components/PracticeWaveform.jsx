import { useEffect, useMemo, useState } from 'react';
import { invoke } from '../juce/bridge.js';

const clamp = (x, lo, hi) => Math.min(hi, Math.max(lo, x));
export default function PracticeWaveform({p, available, disabled, onSeek, onError, takeId, takeVersion, label = 'Backing waveform position', emptyText = 'Load a backing track to see its waveform'}) {
  const [wave, setWave] = useState(null);
  const [failed, setFailed] = useState(false);
  useEffect(() => {
    let active = true; setFailed(false);
    if (available && p.waveRevision > 0) (takeId ? invoke('getTakeReviewWaveform', takeId, takeVersion) : invoke('getPracticeWaveform')).then(data => {
      if (!active) return;
      if (data?.error || !Array.isArray(data?.peaks)) throw new Error(data?.error || 'Waveform unavailable.');
      if (data.revision === p.waveRevision && (!takeId || (data.takeId === takeId && data.version === takeVersion))) setWave(data);
    }).catch(() => { if (active) { setFailed(true); onError({title: takeId ? 'Takes' : 'Practice', text: takeId ? 'Couldn’t read this version’s waveform. Use the position slider.' : 'Couldn’t read the backing waveform.'}); } });
    return () => { active = false; };
  }, [available, p.waveRevision, takeId, takeVersion]);
  const peaks = useMemo(() => available && wave && wave.revision === p.waveRevision && (!takeId || (wave.takeId === takeId && wave.version === takeVersion))
    ? wave.peaks.filter(pair => Array.isArray(pair) && pair.length === 2 && pair.every(Number.isFinite)) : [], [available, wave, p.waveRevision, takeId, takeVersion]);
  const path = useMemo(() => peaks.map(([low, high], i) => {
    const x = (i + .5) * 1000 / peaks.length;
    return `M${x.toFixed(2)},${(50 - clamp(high, -1, 1) * 45).toFixed(2)}V${(50 - clamp(low, -1, 1) * 45).toFixed(2)}`;
  }).join(' '), [peaks]);
  const seconds = p.duration || 0, cursor = clamp((p.position || 0) / (seconds || 1), 0, 1) * 1000;
  const a = clamp((p.a || 0) / (seconds || 1), 0, 1) * 1000, b = clamp((p.b || 0) / (seconds || 1), 0, 1) * 1000;
  const blocked = disabled || !available || !seconds;
  const keyboard = e => {
    if (blocked) return;
    const step = e.shiftKey ? 10 : 1, here = p.position || 0;
    const target = {ArrowLeft: here - step, ArrowDown: here - step, ArrowRight: here + step, ArrowUp: here + step, Home: 0, End: seconds}[e.key];
    if (target !== undefined) { e.preventDefault(); onSeek(clamp(target, 0, seconds)); }
  };
  return <div className="practice-waveform" role="slider" aria-label={label} aria-valuemin={0} aria-valuemax={seconds} aria-valuenow={clamp(p.position || 0, 0, seconds)} aria-valuetext={`${(p.position || 0).toFixed(1)} of ${seconds.toFixed(1)} seconds`} aria-disabled={blocked} tabIndex={blocked ? -1 : 0} onKeyDown={keyboard} onClick={e => {
    if (blocked) return;
    const bounds = e.currentTarget.getBoundingClientRect(); if (bounds.width > 0) onSeek(clamp((e.clientX - bounds.left) / bounds.width, 0, 1) * seconds);
  }}>
    <svg viewBox="0 0 1000 100" preserveAspectRatio="none" aria-hidden="true">
      <line x1="0" x2="1000" y1="50" y2="50" className="wave-midline"/>
      {b > a && <rect x={a} width={b - a} height="100" className={p.loop ? 'wave-loop active' : 'wave-loop'}/>}
      <path d={path} className="wave-peaks"/>
      {seconds > 0 && <><line x1={a} x2={a} y1="0" y2="100" className="wave-boundary"/><text x={clamp(a + 7, 5, 982)} y="14">A</text><line x1={b} x2={b} y1="0" y2="100" className="wave-boundary"/><text x={clamp(b - 14, 5, 982)} y="14">B</text><line x1={cursor} x2={cursor} y1="0" y2="100" className="wave-cursor"/></>}
    </svg>
    {!peaks.length && <span className="wave-placeholder">{failed ? 'Waveform unavailable. Use the position slider.' : seconds ? 'Preparing waveform…' : emptyText}</span>}
  </div>;
}
