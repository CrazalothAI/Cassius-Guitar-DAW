import { useEffect, useRef, useState } from 'react';
import AmpHead from './components/AmpHead.jsx';
import MetronomePanel, { MetronomeButton } from './components/Metronome.jsx';
import PresetBrowser from './components/PresetBrowser.jsx';
import Stages from './components/Stages.jsx';
import Pedalboard from './components/Pedalboard.jsx';
import Library, { previewRigs } from './components/Library.jsx';
import Practice from './components/Practice.jsx';
import PlayAlong from './components/PlayAlong.jsx';
import Takes from './components/Takes.jsx';
import CompactAmp from './components/CompactAmp.jsx';
import RigBar from './components/RigBar.jsx';
import UtilityDialog from './components/UtilityDialog.jsx';
import { ampIdentity } from './ampIdentity.js';
import Midi from './components/Midi.jsx';
import Help from './components/Help.jsx';
import { invoke, native } from './juce/bridge.js';
import { restoreSnapshot, snapshotParameters, useParameter, useParameters, useToggle } from './parameterState.js';
import { applyPreset, matchPreset, presets } from './presets.js';
import { allParameters } from './parameters.js';
import { applyStartingPreview, resolveStartingRigs, startingRigs } from './startingRigs.js';
import cassianLogo from './assets/cassian-logo-192.png'; // shown at 34 px; the full-size original stays in assets
import { releaseLabel } from './releaseLabel.js';
import WorkspaceIcon from './components/WorkspaceIcon.jsx';
import { headForTone, headDriveControl, headSpaceControl } from './ampHeads.js';

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
      text: status.bufferSizes ? `The device or processor missed an audio deadline. Try a larger buffer than ${status.bufferSize} samples.`
        : 'The processor missed an audio deadline. Try a larger buffer; raise it in your DAW’s audio settings.',
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
  const clean = source === 1 || source === 4 || ((source === 0 || source === 3) && legacyClean);
  const values = useParameters(allParameters.map(p => p.id));
  const [chosen, setChosen] = useState('');
  const [tunerOpen, setTunerOpen] = useState(false);
  const [metronomeOpen, setMetronomeOpen] = useState(false);
  const [libraryOpen, setLibraryOpen] = useState(false);
  const [view, setView] = useState('Tone');
  const [takeSelection, setTakeSelection] = useState(null);
  const [utility, setUtility] = useState(null);
  const [previewActive, setPreviewActive] = useState(null);
  const [presetAssets, setPresetAssets] = useState([]), [presetLoading, setPresetLoading] = useState(false);
  const [savedRigs, setSavedRigs] = useState([]);
  const selectingPreset = useRef(false);
  useEffect(() => {
    if (!native) { setSavedRigs(previewRigs()); return; }
    let active = true;
    invoke('getLibrary').then(next => { if (active) { setPresetAssets(next?.assets || []); setSavedRigs(next?.rigs || []); } }).catch(() => {});
    return () => { active = false; };
  }, [status.libraryRevision, libraryOpen, previewActive]);
  const comparing = useRef(false);
  const [page, setPage] = useState('Amp');
  const [tonePage, setTonePage] = useState('Amp');
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
    const saved = savedRigs.find(r => r.id === name);
    if (saved || startingRigs.some(r => r.id === name)) {
      if (selectingPreset.current || status.rigLoading) return false;
      selectingPreset.current = true; setPresetLoading(true);
      try {
        if (native) { const error = await invoke(saved ? 'loadRig' : 'loadStartingRig', name); if (error) throw new Error(error); }
        else if (saved) { restoreSnapshot(saved.parameters); setPreviewActive(saved); }
        else setPreviewActive(applyStartingPreview(name));
        setChosen('');
        return true;
      } catch (e) { setNotice({title: 'Couldn’t load the rig', text: e.message || 'Please try again.'}); return false; }
      finally { selectingPreset.current = false; setPresetLoading(false); }
      return;
    }
    if (!presets[name]) return false;
    applyPreset(name); setChosen(name);
    if (!native) setPreviewActive(null);
    return true;
  };
  // A/B: the first press stores A; each later press swaps the stored state with the current one.
  const toggleCompare = async () => {
    if (comparing.current) return;
    comparing.current = true;
    try {
    const current = native ? await invoke('getRig') : {parameters: snapshotParameters(), identity: previewActive};
    if (current?.error) throw new Error(current.error);
    if (!compare) {
      setCompare(current);
      setCompareSide('A');
      return;
    }
    if (native) { const error = await invoke('applyRig', ...[compare, ...(matchCompare ? [true] : [])]); if (error) throw new Error(error); }
    else { restoreSnapshot(compare.parameters); setPreviewActive(compare.identity || null); }
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
    try { await invoke(type === 'amp' ? 'loadModel' : type === 'pedal' ? 'loadPedal' : type === 'cabB' ? 'loadIRB' : 'loadIR'); }
    catch { setNotice({ title: 'Couldn’t open the file picker', text: 'Please try again.' }); }
  };
  const setBuffer = async size => {
    try {
      const error = await invoke('setBufferSize', size);
      if (typeof error === 'string' && error) setNotice({ title: 'Couldn’t change the buffer size', text: error });
    } catch { setNotice({ title: 'Couldn’t change the buffer size', text: 'Please try again.' }); }
  };
  const deviceAction = async (name, ...args) => {
    try { const error = await invoke(name, ...args); if (typeof error === 'string' && error) setNotice({title: 'Audio settings', text: error}); }
    catch { setNotice({title: 'Audio settings', text: 'Couldn’t update the device. Please try again.'}); }
  };

  // Recognise a preset from the parameters themselves, so the name survives reopening the editor.
  const matched = chosen ? matchPreset(values) : null, current = matched ?? (chosen || null), edited = !matched && Boolean(chosen);
  const activeId = native ? status.activeRigId : previewActive?.id;
  const currentRig = startingRigs.find(r => r.id === activeId) || savedRigs.find(r => r.id === activeId);
  const { message } = status;
  const busy = /^(Loading|Restoring|Preparing|Packing|Importing)/.test(message);
  const footerMessage = !native || busy || message.startsWith('Load failed:') ? message
    : clean ? 'Clean ready' : status.model ? 'Rig ready' : message;
  const showLoad = status.overrunRecent || status.cpu >= 80;
  const identity = ampIdentity(source, clean, status);
  const previewEdited = !!previewActive && Object.entries(previewActive.parameters).some(([id, value]) => Math.abs(values[id] - value) > .005);
  const destinations = ['Tone', 'Board', 'Practice', 'Takes'];
  const navigate = destination => { if (destination !== 'Takes') setTakeSelection(null); setView(destination); setUtility(null); };
  const openPracticeTake = link => { setTakeSelection({ ...link }); navigate('Takes'); };
  const head = headForTone(source, clean, currentRig || previewActive, values, status.board);
  const driveControl = headDriveControl(source, values, status.board, !native ? currentRig || previewActive : null);
  const spaceControl = headSpaceControl(values, status.board, !native ? currentRig || previewActive : null);

  return <div className={`app-shell ${clean ? 'clean' : 'metal'} head-${head.id}`}>
    <header>
      <div className="brand"><img className="brand-logo" src={cassianLogo} alt="" /><div><h1>CASSIAN</h1><span className="brand-subtitle">AMPLIFIER STUDIO</span></div></div>
      <PresetBrowser current={currentRig ? null : current} currentRig={currentRig && {...currentRig, amp: currentRig.amp || identity, saved: savedRigs.some(r => r.id === currentRig.id)}} rigs={resolveStartingRigs(presetAssets, !native)} savedRigs={savedRigs} loading={presetLoading || status.rigLoading || status.practice?.recordMode > 0 || status.review?.playing} edited={currentRig ? (native ? status.activeRigEdited : previewEdited) : edited} onChoose={chooseTone} onRevert={() => chooseTone(currentRig?.id || current)}
        compare={compare} compareSide={compareSide} onCompare={toggleCompare} showCompare={false} />
      <div className="header-tools">
        <MetronomeButton open={metronomeOpen} onToggle={() => setMetronomeOpen(!metronomeOpen)} status={status} />
        <button className="tuner-toggle" aria-pressed={tunerOpen} onClick={() => setTunerOpen(!tunerOpen)}>{tunerOpen ? 'TUNER ON' : 'TUNER'}</button>
        <span className="connection" title={native ? 'Connected to the audio engine' : 'Browser preview'}><i />{native ? 'LIVE' : 'PREVIEW'}</span>
      </div>
      {metronomeOpen && <MetronomePanel status={status} onClose={() => setMetronomeOpen(false)} />}
    </header>
    <main>
      <Alerts status={status} notice={notice} dismissed={dismissed} onDismiss={dismiss} onBuffer={setBuffer} />
      <RigBar status={status} amp={identity} previewActive={previewActive} previewEdited={previewEdited} onPreviewRig={setPreviewActive} onError={setNotice} onCompare={toggleCompare} compare={compare} compareSide={compareSide} matchCompare={matchCompare} onMatch={setMatchCompare}/>
      <div className="workspace-nav"><div role="tablist" aria-label="Workspace" onKeyDown={e => {
        let next; const i = destinations.indexOf(view);
        if (e.key === 'ArrowRight') next = (i + 1) % destinations.length;
        else if (e.key === 'ArrowLeft') next = (i + destinations.length - 1) % destinations.length;
        else if (e.key === 'Home') next = 0; else if (e.key === 'End') next = destinations.length - 1;
        else return;
        e.preventDefault(); navigate(destinations[next]); e.currentTarget.querySelectorAll('[role="tab"]')[next]?.focus();
      }}>{destinations.map(destination => <button key={destination} role="tab" id={`view-${destination}`} aria-controls="workspace-content" aria-selected={view === destination} tabIndex={view === destination ? 0 : -1} onClick={() => navigate(destination)}><WorkspaceIcon name={destination}/>{destination}</button>)}</div><div className="workspace-tools"><button className="text-button" onClick={() => setLibraryOpen(true)}>Library</button><button className="text-button" onClick={() => { setNotice(null); setUtility('Mix'); }}>Mix</button><button className="text-button" onClick={() => { setNotice(null); setUtility('Performance'); }}>Performance</button></div></div>
      <div className={`workspace-content view-${view.toLowerCase()}`} role="tabpanel" id="workspace-content" aria-labelledby={`view-${view}`}>
        {view === 'Tone' ? <AmpHead head={head} driveControl={driveControl} spaceControl={spaceControl} clean={clean} tunerOpen={tunerOpen} status={status}/> : <CompactAmp head={head} driveControl={driveControl} clean={clean} tunerOpen={tunerOpen} status={status}/>}
        {view === 'Board' && !status.board?.serial && <Pedalboard status={status} onError={setNotice}/>}
        {view === 'Practice' ? <Practice status={status} onError={setNotice} onTakes={() => { setTakeSelection(null); navigate('Takes'); }} onOpenTake={openPracticeTake}/> : view === 'Takes' ? <section className="takes-workspace" aria-label="Take library"><div className="practice-heading"><h2>Your take library</h2><button className="text-button" onClick={() => { setTakeSelection(null); navigate('Practice'); }}>Record a take</button></div><Takes status={status} onError={setNotice} selectionRequest={takeSelection}/></section> : view === 'Board' && status.board?.serial ? <Pedalboard status={status} onError={setNotice}/> : <Stages page={view === 'Tone' ? tonePage : page} onPage={view === 'Tone' ? setTonePage : setPage} availablePages={view === 'Tone' ? ['Amp', 'Cab'] : undefined} showScenes={view === 'Board'} clean={clean} native={native} status={status} onLoad={load} onRemove={remove} onError={setNotice}/>}
      </div>
    </main>
    {libraryOpen && <Library revision={status.libraryRevision} loading={status.rigLoading} onClose={() => setLibraryOpen(false)} onPreset={chooseTone} onPreviewRig={setPreviewActive} previewActiveId={previewActive?.id}/>}
    {utility && <UtilityDialog title={utility === 'Mix' ? 'Play along mix' : utility === 'Help' ? 'Help & setup' : 'Performance settings'} onClose={() => setUtility(null)} notice={notice}>{utility === 'Mix' ? <PlayAlong status={status} onError={setNotice}/> : utility === 'Help' ? <Help status={status} onError={setNotice} onNavigate={navigate} onChooseRig={async id => { if (await chooseTone(id)) navigate('Tone'); }}/> : <Midi status={status} onError={setNotice}/>}</UtilityDialog>}
    <footer>
      <span role="status">{footerMessage}</span>
      <button className="device-settings" onClick={() => { setNotice(null); setUtility('Help'); }}>Help &amp; setup</button>
      <span title="Cassian version" aria-label="Cassian version">v{releaseLabel(status,native)}</span>
      {native && status.review?.playing && <button className="device-settings" onClick={() => deviceAction('reviewControl', 'stop', 0)}>Stop take review</button>}
      {native && status.practice?.recordMode > 0 && <button className="device-settings" onClick={() => navigate('Practice')}>Recording · Open Practice</button>}
      <span className="device-controls">
      {native && status.deviceSettingsAvailable && <button className="device-settings" onClick={() => deviceAction('showAudioSettings')}>Audio settings</button>}
      {native && status.inputChannels?.length > 0 && <select className="buffer-select input-select" aria-label="Guitar input" title="Physical guitar input · input monitoring is controlled in Audio settings" value={status.selectedInput ?? -1} onChange={e => deviceAction('setInputChannel', Number(e.target.value))}>
        <option value={-1} disabled>No input</option>{status.inputChannels.map((name, index) => <option key={index} value={index}>{index + 1} · {name}</option>)}
      </select>}
      <span>{status.sampleRate ? <>{(status.sampleRate / 1000).toFixed(1)} kHz · {status.bufferSizes?.length
        ? <select className="buffer-select" aria-label="Buffer size" value={status.bufferSize} onChange={e => setBuffer(Number(e.target.value))}>
            {status.bufferSizes.map(size => <option key={size} value={size}>{size}</option>)}
          </select>
        : status.bufferSize || '—'} samples
        {showLoad && <span className={status.overrunRecent || status.cpu >= 80 ? 'warn' : ''}> · DSP {Math.round(status.cpu || 0)}% · {status.overruns || 0} overruns</span>}</>
        : 'NO AUDIO IN PREVIEW'}</span></span>
    </footer>
  </div>;
}
