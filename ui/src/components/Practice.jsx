import { useState } from 'react';
import { invoke, native } from '../juce/bridge.js';

export const time = seconds => {
  const n = Math.max(0, Math.floor(Number(seconds) || 0));
  return `${Math.floor(n / 60)}:${String(n % 60).padStart(2, '0')}`;
};

export default function Practice({ status, onError }) {
  const p = status.practice ?? {}, [bars, setBars] = useState(1);
  const available = native && status.deviceSettingsAvailable;
  const recording = (p.recordMode ?? 0) > 0, counting = p.counting || p.recordMode === 2;
  const busy = recording || counting, duration = p.duration || 0;
  const action = async (name, ...args) => {
    try {
      const result = await invoke(name, ...args);
      if (typeof result === 'string' && result) onError({ title: 'Practice', text: result });
      if (result === false) onError({ title: 'Practice', text: 'The action could not be completed.' });
    } catch { onError({ title: 'Practice', text: 'Audio engine connection interrupted. Please try again.' }); }
  };
  const control = (name, x = 0) => action('practiceControl', name, x);
  const mode = ['Ready', 'Preparing take…', 'Count-in', 'Recording', 'Saving take…'][p.recordMode ?? 0];
  return <section className="practice-panel" aria-label="Practice and recording">
    <div className="practice-heading">
      <div><span className="practice-kicker">PLAY · PRACTICE · CAPTURE</span><h2>{p.track || 'Your next take'}</h2></div>
      <button className="text-button" disabled={!available || busy || p.loading} onClick={() => action('loadBackingTrack')}>{p.loading ? 'Loading track…' : 'Load backing track'}</button>
    </div>
    <div className="practice-transport">
      <button disabled={!available || !duration || busy || p.loading} onClick={() => p.playing ? control('pause') : action('practiceStart', 'play', bars)}>{p.playing ? 'Pause' : 'Play'}</button>
      <button disabled={!available} onClick={() => control('stop')}>Stop</button>
      <button className={`practice-record${recording ? ' recording' : ''}`} disabled={!available || p.loading || p.recordMode === 1 || p.recordMode === 4 || (!recording && counting)} onClick={() => recording ? control('pause') : action('practiceStart', 'record', bars)}>{recording ? 'Finish take' : 'Record guitar'}</button>
      <label>Count-in<select aria-label="Count-in" value={bars} disabled={busy} onChange={e => setBars(Number(e.target.value))}><option value={0}>Off</option><option value={1}>1 bar</option><option value={2}>2 bars</option></select></label>
      <output className={recording ? 'record-status' : ''} aria-live="polite">{counting ? `Count-in · ${p.countBeat || 1}` : mode}{p.recordMode === 3 && ` · ${time(p.recordSeconds)}`}</output>
    </div>
    <div className="practice-timeline"><span>{time(p.position)}</span><input type="range" aria-label="Backing track position" min={0} max={duration || 1} step={.01} value={Math.min(p.position || 0, duration)} disabled={!available || !duration || busy} onChange={e => control('seek', Number(e.target.value))} /><span>{time(duration)}</span></div>
    <div className="practice-options">
      <label className="practice-volume">Backing volume<input type="range" aria-label="Backing volume" min={-60} max={6} step={1} value={p.level ?? -12} disabled={!available} onChange={e => control('level', Number(e.target.value))} /><output>{p.level ?? -12} dB</output></label>
      <div className="practice-loop"><button disabled={!available || !duration || busy} onClick={() => control('a', p.position || 0)}>Set A · {time(p.a)}</button><button disabled={!available || !duration || busy} onClick={() => control('b', p.position || 0)}>Set B · {time(p.b)}</button><label><input type="checkbox" aria-label="Loop section" checked={!!p.loop} disabled={!available || !duration || busy} onChange={e => control('loop', e.target.checked ? 1 : 0)} /> Loop A–B</label></div>
    </div>
    <p className="practice-note">{available ? '32-bit float WAV: dry mono + processed stereo before Master. Backing and clicks stay out of guitar recordings. Count-in uses the metronome tempo.' : native ? 'Use your DAW’s backing tracks and recording. This practice transport is available in the standalone app.' : 'Open the standalone app to load a backing track and record your guitar.'}</p>
    {p.error && <p className="practice-error" role="alert">{p.error}</p>}
    {p.takePath && !recording && <div className="practice-take"><span title={p.takePath}>Last take: {p.takePath}</span><button className="text-button" onClick={() => action('openTakeFolder')}>Open take folder</button></div>}
  </section>;
}
