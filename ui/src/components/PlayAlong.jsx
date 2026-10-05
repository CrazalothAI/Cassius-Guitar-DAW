import Knob from './Knob.jsx';
import { setParameter } from '../parameterState.js';
import { invoke, native } from '../juce/bridge.js';

export default function PlayAlong({ status, onError }) {
  const p = status.practice ?? {};
  const forward = () => {
    setParameter('GUITAR_MIX_LEVEL', 3);
    setParameter('GUITAR_MIX_FOCUS', 45);
  };
  const reset = () => {
    setParameter('GUITAR_MIX_LEVEL', 0);
    setParameter('GUITAR_MIX_FOCUS', 0);
  };
  const backingLevel = async value => {
    try {
      const error = await invoke('practiceControl', 'level', value);
      if (typeof error === 'string' && error) onError({title: 'Backing volume', text: error});
    } catch { onError({title: 'Backing volume', text: 'Audio engine connection interrupted. Please try again.'}); }
  };
  return <section className="stage-panel play-along-panel" aria-label="Play along mix">
    <div className="play-along-heading"><div><span className="practice-kicker">GUITAR · BACKING · BALANCE</span><h2>Hear every note</h2></div><div><button className="text-button" onClick={forward}>Bring guitar forward</button><button className="text-button" onClick={reset}>Neutral mix</button></div></div>
    <div className="play-along-controls"><Knob id="GUITAR_MIX_LEVEL" small/><Knob id="GUITAR_MIX_FOCUS" small/><p>Guitar balance changes listening volume after the amp and effects. Mix focus adds broad note definition and reduces low-end overlap. It works with clean and distorted tones.</p></div>
    <p className="practice-note"><strong>YouTube or browser backing:</strong> start with the video volume around 25%, then adjust Guitar balance to hear your notes clearly. Cassian cannot change browser audio. Master controls Cassian’s output.</p>
    {native && status.deviceSettingsAvailable && p.duration > 0 && <label className="practice-volume">Cassian backing volume<input type="range" aria-label="Cassian backing volume" min={-60} max={6} step={1} value={p.level ?? -12} onChange={e => backingLevel(Number(e.target.value))}/><output>{p.level ?? -12} dB</output></label>}
    {native && !status.deviceSettingsAvailable && <p className="practice-note">In a DAW, adjust backing track volume in the host mixer.</p>}
    {status.outputPeakWarning && <p className="practice-error" role="alert">Mix peaks reached the −0.5 dBFS output ceiling within the last second. Lower Master for more headroom.</p>}
    <p className="practice-note">Listening controls stay put when changing tones, rigs or scenes. Dry/processed recordings and offline reamps keep the original rig sound.</p>
  </section>;
}
