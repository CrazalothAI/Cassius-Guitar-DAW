import { useState } from 'react';
import { invoke, native } from '../juce/bridge.js';
import Takes from './Takes.jsx';
import PracticeWaveform from './PracticeWaveform.jsx';
import PracticeSections from './PracticeSections.jsx';
import { time } from '../practiceTime.js';
export { time } from '../practiceTime.js';

export default function Practice({ status, onError }) {
  const p = status.practice ?? {}, [bars, setBars] = useState(1);
  const [takesOpen, setTakesOpen] = useState(false);
  const available = native && status.deviceSettingsAvailable;
  const recording = (p.recordMode ?? 0) > 0, counting = p.counting || p.starting || p.recordMode === 2;
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
      <div><span className="practice-kicker">PLAY · PRACTICE · CAPTURE</span><h2>{takesOpen ? 'Your take library' : p.track || 'Your next take'}</h2></div>
      <div className="practice-heading-actions"><button className="text-button" aria-expanded={takesOpen} onClick={() => setTakesOpen(!takesOpen)}>{takesOpen ? 'Practice transport' : 'Take library'}</button>{!takesOpen && <button className="text-button" disabled={!available || busy || p.loading} onClick={() => action('loadBackingTrack')}>{p.loading ? `Preparing track · ${Math.round((p.loadProgress || 0) * 100)}%` : 'Load backing track'}</button>}</div>
    </div>
    {takesOpen ? <Takes status={status} onError={onError}/> : <>
    <div className="practice-transport">
      {p.loading && <button disabled={!available} onClick={() => control('cancelLoad')}>Cancel preparation</button>}
      <button disabled={!available || !duration || busy || p.loading} onClick={() => p.playing ? control('pause') : action('practiceStart', 'play', bars)}>{p.playing ? 'Pause' : 'Play'}</button>
      <button disabled={!available} onClick={() => control('stop')}>Stop</button>
      <button className={`practice-record${recording ? ' recording' : ''}`} disabled={!available || p.loading || p.recordMode === 1 || p.recordMode === 4 || (!recording && counting)} onClick={() => recording ? control('pause') : action('practiceStart', 'record', bars)}>{recording ? 'Finish take' : 'Record guitar'}</button>
      <label>Count-in<select aria-label="Count-in" value={bars} disabled={busy} onChange={e => setBars(Number(e.target.value))}><option value={0}>Off</option><option value={1}>1 bar</option><option value={2}>2 bars</option></select></label>
      <label>Speed<select aria-label="Practice speed" value={p.requestedSpeed ?? p.speed ?? 1} disabled={!available || !duration || busy || p.loading} onChange={e => control('speed', Number(e.target.value))}>{[.5, .65, .75, .85, 1, 1.15, 1.25, 1.5].map(speed => <option key={speed} value={speed}>{Math.round(speed * 100)}%</option>)}</select></label>
      <output className={recording ? 'record-status' : ''} aria-live="polite">{counting ? `Count-in · ${p.countBeat || 1}` : mode}{p.recordMode === 3 && ` · ${time(p.recordSeconds)}`}</output>
    </div>
    <label className="practice-fade">Loop edge fade<select aria-label="Loop edge fade" value={p.fade ?? 5} disabled={!available || busy || p.loading} onChange={e => control('fade', Number(e.target.value))}><option value={0}>Off</option><option value={2}>2 ms</option><option value={5}>5 ms</option><option value={10}>10 ms</option><option value={20}>20 ms</option></select></label>
    <p className="practice-note">Speed changes keep pitch and prepare while paused. The timeline and loop points use the original track time. Count-in and metronome tempo stay at your chosen BPM.</p>
    <PracticeWaveform p={p} available={available} disabled={busy || p.loading} onSeek={seconds => control('seek', seconds)} onError={onError}/>
    <div className="practice-timeline"><span>{time(p.position)}</span><input type="range" aria-label="Backing track position" min={0} max={duration || 1} step={.01} value={Math.min(p.position || 0, duration)} disabled={!available || !duration || busy || p.loading} onChange={e => control('seek', Number(e.target.value))} /><span>{time(duration)}</span></div>
    <div className="practice-options">
      <label className="practice-volume">Backing volume<input type="range" aria-label="Backing volume" min={-60} max={6} step={1} value={p.level ?? -12} disabled={!available} onChange={e => control('level', Number(e.target.value))} /><output>{p.level ?? -12} dB</output></label>
      <div className="practice-loop"><button disabled={!available || !duration || busy || p.loading} onClick={() => control('a', p.position || 0)}>Set A · {time(p.a)}</button><button disabled={!available || !duration || busy || p.loading} onClick={() => control('b', p.position || 0)}>Set B · {time(p.b)}</button><label><input type="checkbox" aria-label="Loop section" checked={!!p.loop} disabled={!available || !duration || busy || p.loading} onChange={e => control('loop', e.target.checked ? 1 : 0)} /> Loop A–B</label></div>
    </div>
    <PracticeSections p={p} available={available} disabled={busy || p.loading} onError={onError}/>
    <p className="practice-note">{available ? '32-bit float WAV: dry mono + processed stereo before Master. Backing and clicks stay out of guitar recordings. Count-in uses the metronome tempo.' : native ? 'Use your DAW’s backing tracks and recording. This practice transport is available in the standalone app.' : 'Open the standalone app to load a backing track and record your guitar.'}</p>
    {p.error && <p className="practice-error" role="alert">{p.error}</p>}
    {p.takePath && !recording && <div className="practice-take"><span title={p.takePath}>Last take: {p.takePath}</span><button className="text-button" onClick={() => action('openTakeFolder')}>Open take folder</button></div>}
    </>}
  </section>;
}
