import { useEffect, useRef, useState } from 'react';
import AmpHead from './components/AmpHead.jsx';
import MetronomePanel, { MetronomeButton } from './components/Metronome.jsx';
import PresetBrowser from './components/PresetBrowser.jsx';
import Stages from './components/Stages.jsx';
import Library from './components/Library.jsx';
import { invoke, native } from './juce/bridge.js';
import { restoreSnapshot, snapshotParameters, useParameter, useParameters, useToggle } from './parameterState.js';
import { applyPreset, matchPreset, presetParameterIds, presets } from './presets.js';
import cassianLogo from './assets/cassian-logo-192.png'; // shown at 34 px; the full-size original stays in assets

const initialStatus = {
  model: '', ir: '', pedal: '', input: 0, prePedal: 0, postPedal: 0, postAmp: 0, postCab: 0, output: 0, gate: 0, overruns: 0, dropouts: -1,
  tunerActive: false, tunerNote: '—', tunerCents: 0, tunerHz: 0, dynResCut: 0, metronomeClicks: 0, metronomeBeat: 0,
  message: native ? 'Connecting to audio engine…' : 'Browser preview · Open Cassian to play.',
};

// Polls the engine at 10 Hz. `overrunRecent` flags processing overruns, and `dropoutRecent`
// audio-device dropouts, within the last five seconds: either is heard as crackle.
function useEngineStatus() {
  const [status, setStatus] = useState(initialStatus);
  useEffect(() => {
    if (!native) return;
    let active = true, timer, lastCount = 0, lastAt = 0, lastDrops = -1, dropAt = 0;
    const poll = async () => {
      try {
        const next = await invoke('getStatus');
        if (active && next) {
          if ((next.overruns || 0) > lastCount) lastAt = Date.now();
          lastCount = next.overruns || 0;
          const drops = next.dropouts ?? -1;
          if (lastDrops >= 0 && drops > lastDrops) dropAt = Date.now();
          lastDrops = drops;
          const now = Date.now();
          setStatus({ ...next, message: String(next.message ?? ''), overrunRecent: lastAt > 0 && now - lastAt < 5000, dropoutRecent: dropAt > 0 && now - dropAt < 5000 });
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

// The next larger buffer the device offers, for the dropout warning's one-click fix.
const largerBuffer = status => (status.bufferSizes ?? []).find(size => size > (status.bufferSize || 0) && size >= 128);

function Alerts({ status, notice, dismissed, onDismiss, onBuffer }) {
  const { message } = status, alerts = [];
  if (status.inputClipped) alerts.push({ key: 'clip', kind: 'warning', title: 'Input is clipping', text: 'Turn down the input gain on your interface.' });
  if (status.dropoutRecent || status.overrunRecent) {
    const next = largerBuffer(status);
    alerts.push({ key: 'dropout', kind: 'warning', title: 'Audio is dropping out',
      text: status.bufferSizes ? `Crackles mean the ${status.bufferSize}-sample buffer is too small for this computer.`
        : 'Crackles mean the buffer is too small: raise it in your DAW’s audio settings.',
      action: next && { label: `Use ${next} samples`, run: () => onBuffer(next) } });
  }
  if (message.startsWith('Load failed:') && message !== dismissed)
    alerts.push({ key: 'load', kind: 'error', title: 'Couldn’t load the file', text: message.slice(12).trim(), dismiss: true });
  if (notice) alerts.push({ key: 'notice', kind: 'error', title: notice.title, text: notice.text, dismiss: true });
  if (message === 'Audio engine connection interrupted')
    alerts.push({ key: 'link', kind: 'error', title: 'Connection interrupted', text: 'Controls may not reach the audio engine. Reopen the editor if this persists.' });
  if (!alerts.length) return null;
  return <div className="alerts">{alerts.map(a =>
    <div key={a.key} className={`alert ${a.kind}`} role="alert">
      <strong>{a.title}</strong><span>{a.text}</span>
      {a.action && <button className="alert-action" onClick={a.action.run}>{a.action.label}</button>}
      {a.dismiss && <button className="alert-dismiss" aria-label="Dismiss" onClick={() => onDismiss(a.key)}>×</button>}
    </div>)}
  </div>;
}

export default function App() {
  const status = useEngineStatus();
  const legacyClean = useToggle('AMP_CLEAN'), source = Math.round(useParameter('AMP_SOURCE'));
  const clean = source === 1 || (source === 0 && legacyClean);
  const values = useParameters(presetParameterIds);
  const [chosen, setChosen] = useState('');
  const [tunerOpen, setTunerOpen] = useState(false);
  const [metronomeOpen, setMetronomeOpen] = useState(false);
  const [libraryOpen, setLibraryOpen] = useState(false);
  const comparing = useRef(false);
  const [page, setPage] = useState('Amp');
  const [dismissed, setDismissed] = useState('');
  const [compare, setCompare] = useState(null);
  const [compareSide, setCompareSide] = useState('A');
  const [matchCompare, setMatchCompare] = useState(false);
  // Local failures live apart from the polled status, which would overwrite them within 100 ms.
  const [notice, setNotice] = useState(null);
  // The engine's pitch analysis runs only while the tuner is on screen.
  useEffect(() => {
    if (!native) return;
    invoke('setTuner', tunerOpen).catch(() => {});
    return () => { invoke('setTuner', false).catch(() => {}); };
  }, [tunerOpen]);
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
  const toggleCompare = async () => {
    if (comparing.current) return;
    comparing.current = true;
    try {
    const current = native ? await invoke('getRig') : {parameters: snapshotParameters()};
    if (current?.error) throw new Error(current.error);
    if (!compare) {
      setCompare(current);
      setCompareSide('A');
      return;
    }
    if (native) { const error = await invoke('applyRig', ...[compare, ...(matchCompare ? [true] : [])]); if (error) throw new Error(error); }
    else restoreSnapshot(compare.parameters);
    setCompare(current);
    setCompareSide(side => side === 'A' ? 'B' : 'A');
    } catch (e) { setNotice({title: 'Couldn’t compare rigs', text: e.message || 'Please try again.'}); }
    finally { comparing.current = false; }
  };
  const remove = async stage => {
    try { await invoke('clearStage', stage); }
    catch { setNotice({ title: 'Couldn’t remove the file', text: 'Please try again.' }); }
  };
  const load = async type => {
    try { await invoke(type === 'amp' ? 'loadModel' : type === 'pedal' ? 'loadPedal' : 'loadIR'); }
    catch { setNotice({ title: 'Couldn’t open the file picker', text: 'Please try again.' }); }
  };
  const setBuffer = async size => {
    try {
      const error = await invoke('setBufferSize', size);
      if (typeof error === 'string' && error) setNotice({ title: 'Couldn’t change the buffer size', text: error });
    } catch { setNotice({ title: 'Couldn’t change the buffer size', text: 'Please try again.' }); }
  };

  // Recognise a preset from the parameters themselves, so the name survives reopening the editor.
  const matched = matchPreset(values), current = matched ?? (chosen || null), edited = !matched && Boolean(chosen);
  const { message } = status;
  const busy = /^(Loading|Restoring|Preparing|Packing|Importing)/.test(message);
  const footerMessage = !native || busy || message.startsWith('Load failed:') ? message
    : clean ? 'Clean ready' : status.model ? 'Rig ready' : message;
  const showLoad = status.overrunRecent || status.cpu >= 80;

  return <div className={`app-shell ${clean ? 'clean' : 'metal'}`}>
    <header>
      <div className="brand"><img className="brand-logo" src={cassianLogo} alt="" /><h1>CASSIAN</h1></div>
      <PresetBrowser current={current} edited={edited} onChoose={chooseTone} onRevert={() => chooseTone(current)}
        compare={compare} compareSide={compareSide} onCompare={toggleCompare} />
      <div className="header-tools">
        <MetronomeButton open={metronomeOpen} onToggle={() => setMetronomeOpen(!metronomeOpen)} status={status} />
        <button className="tuner-toggle" aria-pressed={tunerOpen} onClick={() => setTunerOpen(!tunerOpen)}>{tunerOpen ? 'TUNER ON' : 'TUNER'}</button>
        <span className="connection" title={native ? 'Connected to the audio engine' : 'Browser preview'}><i />{native ? 'LIVE' : 'PREVIEW'}</span>
      </div>
      {metronomeOpen && <MetronomePanel status={status} onClose={() => setMetronomeOpen(false)} />}
    </header>
    <main>
      <Alerts status={status} notice={notice} dismissed={dismissed} onDismiss={dismiss} onBuffer={setBuffer} />
      <div className="library-toolbar"><button className="text-button" onClick={() => setLibraryOpen(true)}>Library</button><span>Amps · Pedals · Cabinets · Saved rigs</span><label className="compare-match" title="Approximate A/B level matching from recent playing. Play similar notes before storing each side."><input type="checkbox" disabled={!native} checked={matchCompare} onChange={e => setMatchCompare(e.target.checked)} /> Match A/B loudness</label></div>
      <AmpHead clean={clean} tunerOpen={tunerOpen} status={status} />
      <Stages page={page} onPage={setPage} clean={clean} native={native} status={status} onLoad={load} onRemove={remove} />
    </main>
    {libraryOpen && <Library revision={status.libraryRevision} onClose={() => setLibraryOpen(false)} onPreset={chooseTone} />}
    <footer>
      <span role="status">{footerMessage}</span>
      <span>{status.sampleRate ? <>{(status.sampleRate / 1000).toFixed(1)} kHz · {status.bufferSizes?.length
        ? <select className="buffer-select" aria-label="Buffer size" value={status.bufferSize} onChange={e => setBuffer(Number(e.target.value))}>
            {status.bufferSizes.map(size => <option key={size} value={size}>{size}</option>)}
          </select>
        : status.bufferSize || '—'} samples
        {showLoad && <span className={status.overrunRecent || status.cpu >= 80 ? 'warn' : ''}> · DSP {Math.round(status.cpu || 0)}% · {status.overruns || 0} overruns</span>}</>
        : 'NO AUDIO IN PREVIEW'}</span>
    </footer>
  </div>;
}
