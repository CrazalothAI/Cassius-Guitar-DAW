import { useEffect, useRef, useState } from 'react';
import Drawer from './components/Drawer.jsx';
import Knob from './components/Knob.jsx';
import Tuner from './components/Tuner.jsx';
import { invoke, native } from './juce/bridge.js';
import { restoreSnapshot, setParameter, snapshotParameters, useParameters, useToggle } from './parameterState.js';
import { applyPreset, familyOf, matchPreset, notes, presetParameterIds, presets, voices } from './presets.js';
import cassianLogo from './assets/cassian-logo.png';

const mainControls = ['DRIVE_GAIN', 'AMP_BASS', 'AMP_MID', 'AMP_TREBLE', 'REVERB_MIX', 'MASTER_VOL'];
const initialStatus = {
  model: '', ir: '', pedal: '', pedalFallback: true, input: 0, prePedal: 0, postPedal: 0, postAmp: 0, postCab: 0, output: 0, gate: 0, overruns: 0,
  tunerActive: false, tunerNote: '—', tunerCents: 0, tunerHz: 0, dynResCut: 0,
  message: native ? 'Connecting to audio engine…' : 'Browser preview · Open Cassian to play.',
};

// Polls the engine at 10 Hz. `overrunRecent` flags processing overruns within the last five seconds.
function useEngineStatus() {
  const [status, setStatus] = useState(initialStatus);
  useEffect(() => {
    if (!native) return;
    let active = true, timer, lastCount = 0, lastAt = 0;
    const poll = async () => {
      try {
        const next = await invoke('getStatus');
        if (active && next) {
          if ((next.overruns || 0) > lastCount) lastAt = Date.now();
          lastCount = next.overruns || 0;
          setStatus({ ...next, message: String(next.message ?? ''), overrunRecent: lastAt > 0 && Date.now() - lastAt < 5000 });
        }
      } catch {
        if (active) setStatus(s => ({ ...s, message: 'Audio engine connection interrupted' }));
      }
      if (active) timer = setTimeout(poll, 100);
    };
    poll();
    return () => { active = false; clearTimeout(timer); };
  }, []);
  return status;
}

function Meter({ label, value }) {
  const db = Math.max(-60, Math.min(0, value > 0 ? 20 * Math.log10(value) : -60));
  return <div className={`meter${db > -1 ? ' hot' : ''}`}>
    <span>{label}</span>
    <div className="meter-track" role="meter" aria-label={`${label} level`} aria-valuemin={-60} aria-valuemax={0} aria-valuenow={Math.round(db)}>
      <i style={{ width: `${(db + 60) / 60 * 100}%` }} />
    </div>
  </div>;
}

function Alerts({ status, notice, dismissed, onDismiss }) {
  const { message } = status, alerts = [];
  if (status.inputClipped) alerts.push({ key: 'clip', kind: 'warning', title: 'Input is clipping', text: 'Turn down the input gain on your interface.' });
  if (message.startsWith('Load failed:') && message !== dismissed)
    alerts.push({ key: 'load', kind: 'error', title: 'Couldn’t load the file', text: message.slice(12).trim(), dismiss: true });
  if (notice) alerts.push({ key: 'notice', kind: 'error', title: notice.title, text: notice.text, dismiss: true });
  if (message === 'Audio engine connection interrupted')
    alerts.push({ key: 'link', kind: 'error', title: 'Connection interrupted', text: 'Controls may not reach the audio engine. Reopen the editor if this persists.' });
  if (!alerts.length) return null;
  return <div className="alerts">{alerts.map(a =>
    <div key={a.key} className={`alert ${a.kind}`} role="alert">
      <strong>{a.title}</strong><span>{a.text}</span>
      {a.dismiss && <button className="alert-dismiss" aria-label="Dismiss" onClick={() => onDismiss(a.key)}>×</button>}
    </div>)}
  </div>;
}

export default function App() {
  const status = useEngineStatus();
  const clean = useToggle('AMP_CLEAN'), gateEnabled = useToggle('GATE_ON');
  const values = useParameters(presetParameterIds);
  const [chosen, setChosen] = useState('');
  const [expanded, setExpanded] = useState(false);
  const [tunerOpen, setTunerOpen] = useState(false);
  const [page, setPage] = useState('Shape');
  const [dismissed, setDismissed] = useState('');
  const [compare, setCompare] = useState(null);
  const [compareSide, setCompareSide] = useState('A');
  // Local failures live apart from the polled status, which would overwrite them within 100 ms.
  const [notice, setNotice] = useState(null);
  const drawer = useRef(null);
  useEffect(() => { if (expanded) drawer.current?.scrollIntoView?.({ block: 'nearest', behavior: 'smooth' }); }, [expanded, page]);
  useEffect(() => { if (!notice) return; const t = setTimeout(() => setNotice(null), 8000); return () => clearTimeout(t); }, [notice]);
  const dismiss = key => key === 'notice' ? setNotice(null) : setDismissed(status.message);

  const chooseTone = async name => {
    if (!presets[name]) return;
    applyPreset(name); setChosen(name);
    if (native && !presets[name].AMP_CLEAN) {
      try { await invoke('selectAmpVoice', name === '80s rock' ? 'Blue-I' : 'Red-I'); }
      catch { setNotice({ title: 'Couldn’t switch amp voice', text: 'Your current capture is still active.' }); }
    }
  };
  // A/B: the first press stores A; each later press swaps the stored state with the current one.
  const toggleCompare = () => {
    const current = snapshotParameters();
    if (!compare) {
      setCompare(current);
      setCompareSide('A');
      return;
    }
    restoreSnapshot(compare);
    setCompare(current);
    setCompareSide(side => side === 'A' ? 'B' : 'A');
  };
  // Sets input gain so the raw interface peak lands near -12 dBFS.
  const autoTrim = () => {
    if (!native || !(status.input > 0.0001)) return;
    const peakDb = 20 * Math.log10(status.input);
    setParameter('INPUT_GAIN', Math.max(-12, Math.min(12, -12 - peakDb)));
  };
  const load = async type => {
    try { await invoke(type === 'amp' ? 'loadModel' : type === 'pedal' ? 'loadPedal' : 'loadIR'); }
    catch { setNotice({ title: 'Couldn’t open the file picker', text: 'Please try again.' }); }
  };

  // Recognise a preset from the parameters themselves, so the name survives reopening the editor.
  const matched = matchPreset(values), current = matched ?? (chosen || null), edited = !matched && Boolean(chosen);
  const family = current && familyOf(current);
  const ampName = clean ? 'Lumen · built-in clean'
    : status.model ? status.model.replace('APP-5153-Ivory-', 'EVH 5150III · ').replace(/\.nam$/i, '').replace(/-/g, ' ') : 'Load your amp capture';
  const { message } = status;
  const busy = /^(Loading|Restoring)/.test(message);
  const footerMessage = !native || busy || message.startsWith('Load failed:') ? message
    : clean ? 'Clean ready' : status.model ? 'Rig ready' : message;
  const showLoad = expanded || status.overrunRecent || status.cpu >= 80;
  const gateText = !gateEnabled ? 'Gate off' : !native ? 'Gate on' : status.gate > .1 ? 'Gate open' : 'Gate closed';

  return <div className={`app-shell ${clean ? 'clean' : 'metal'}`}>
    <header>
      <div className="brand"><img className="brand-logo" src={cassianLogo} alt="" /><h1>CASSIAN</h1></div>
      <div className="header-tools">
        <button className="compare-toggle" aria-label="A/B compare" onClick={toggleCompare}>{compare ? `A/B · ${compareSide}` : 'A/B'}</button>
        <button className="tuner-toggle" aria-pressed={tunerOpen} onClick={() => setTunerOpen(!tunerOpen)}>{tunerOpen ? 'TUNER ON' : 'TUNER'}</button>
        <label className="preset">
          <select aria-label="Tone starting point" value="" onChange={e => chooseTone(e.target.value)}>
            <option value="">More tones…</option>
            {Object.keys(presets).map(name => <option key={name}>{name}</option>)}
          </select>
        </label>
      </div>
      <span className="connection"><i />{native ? 'LIVE' : 'PREVIEW'}</span>
    </header>
    <main>
      <Alerts status={status} notice={notice} dismissed={dismissed} onDismiss={dismiss} />
      <section className="voice-section">
        <nav className="tone-types" aria-label="Tone families">
          {voices.map(v => <button key={v.label} aria-pressed={family === v.label} onClick={() => chooseTone(v.presets[0])}>{v.label}</button>)}
        </nav>
        <p className="voice-note">{current
          ? <><strong>{current}</strong>{edited && <em>Edited</em>}<span>{notes[current]}</span></>
          : 'Choose a starting point. Make it yours.'}</p>
      </section>
      <section className="amp-stage" aria-label="Amplifier">
        <div className="amp-handle" />
        <div className="amp-head">
          <div className="grille">
            <span className="corner tl" /><span className="corner tr" />
            <div className="tube-bank" aria-hidden="true">{[0, 1, 2, 3, 4, 5].map(i => <span className="glass-tube" key={i}><i /></span>)}</div>
            <div className="amp-series">{clean ? 'LUMEN' : 'FERRUM'}<small>{clean ? 'CLEAN' : 'CAPTURE'}</small></div>
            {tunerOpen && <Tuner status={status} />}
          </div>
          <div className="faceplate">
            <img className="amp-logo" src={cassianLogo} alt="Cassian" />
            <div className="input-jack" aria-hidden="true"><i /><span>INPUT</span></div>
            <div className="amp-controls">{mainControls.map(id => <Knob key={id} id={id} />)}</div>
            <div className="power" aria-hidden="true"><i /><span>ON</span></div>
          </div>
          <div className="amp-lower" />
        </div>
        <div className="amp-feet"><i /><i /></div>
      </section>
      <section className="capture-strip" aria-label="Amp source">
        <div><span className="source-dot" /><span className="source-name" title={status.model}>{ampName}</span></div>
        <div className="live-meters">
          <Meter label="IN" value={status.input} /><Meter label="PRE" value={status.prePedal} /><Meter label="AMP" value={status.postAmp} />
          <Meter label="CAB" value={status.postCab} /><Meter label="OUT" value={status.output} />
          <button className="auto-trim" disabled={!native || !(status.input > 0.0001)} onClick={autoTrim}>AUTO TRIM</button>
          <span className={`gate-summary${gateText === 'Gate open' ? ' open' : ''}`}><i aria-hidden="true" />{gateText}</span>
        </div>
      </section>
      <section className="effects" ref={drawer}>
        <button className="drawer-toggle" aria-expanded={expanded} aria-controls="effects-panel" onClick={() => setExpanded(!expanded)}>
          <span>RIG & TONE</span><span aria-hidden="true">{expanded ? '−' : '+'}</span>
        </button>
        {expanded && <Drawer page={page} onPage={setPage} clean={clean} native={native} status={status} onLoad={load} />}
      </section>
    </main>
    <footer>
      <span role="status">{footerMessage}</span>
      <span>{status.sampleRate ? <>{(status.sampleRate / 1000).toFixed(1)} kHz · {status.bufferSize || '—'} samples
        {showLoad && <span className={status.overrunRecent || status.cpu >= 80 ? 'warn' : ''}> · DSP {Math.round(status.cpu || 0)}% · {status.overruns || 0} overruns</span>}</>
        : 'NO AUDIO IN PREVIEW'}</span>
    </footer>
  </div>;
}
