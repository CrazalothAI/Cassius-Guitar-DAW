import { useEffect, useState } from 'react';
import { invoke, native } from '../juce/bridge.js';

const defaults = Array.from({length: 8}, (_, i) => ({type: 'cc', channel: 0, number: 16 + i, action: 'none', rig: '', scene: 0, inverted: false}));
const actions = [['none', 'Unassigned'], ['rig', 'Recall saved rig'], ['scene', 'Recall scene'], ['overdrive', 'Toggle overdrive'], ['pedal', 'Toggle NAM pedal'], ['eq', 'Toggle EQ'], ['gate', 'Toggle gate'], ['metronome', 'Toggle metronome'], ['modulation', 'Toggle modulation'], ['master', 'Master expression'], ['drive', 'Drive expression'], ['reverb', 'Reverb expression'], ['delay', 'Delay expression'], ['wah1', 'Wah 1 position'], ['wah2', 'Wah 2 position']];
const expression = name => ['master', 'drive', 'reverb', 'delay', 'wah1', 'wah2'].includes(name);

export default function Midi({status, onError}) {
  const midi = status.midi ?? {}, config = midi.config ?? {enabled: false, mappings: defaults};
  const rows = config.mappings ?? defaults;
  const [slot, setSlot] = useState(0), [draft, setDraft] = useState(rows[0]);
  const [dirty, setDirty] = useState(false), [rigs, setRigs] = useState([]), [busy, setBusy] = useState(false);
  // Refresh from confirmed engine revisions; Apply retains the submitted draft
  // until the next poll rather than reverting to the older status immediately.
  useEffect(() => { if (!dirty) setDraft(rows[slot] ?? defaults[slot]); }, [slot, midi.revision]);
  useEffect(() => () => { if (native) invoke('learnMidi', -1).catch(() => {}); }, []);
  useEffect(() => {
    let active = true;
    if (native) invoke('getLibrary').then(library => { if (active) setRigs(library?.rigs ?? []); }).catch(() => { if (active) onError({title: 'MIDI', text: 'Couldn’t read saved rigs.'}); });
    return () => { active = false; };
  }, [status.libraryRevision]);
  const change = values => { setDraft(row => ({...row, ...values})); setDirty(true); };
  const action = async (name, ...args) => {
    if (busy) return false; setBusy(true);
    try { const failure = await invoke(name, ...args); if (typeof failure === 'string' && failure) throw new Error(failure); return true; }
    catch (e) { onError({title: 'MIDI', text: e.message || 'Couldn’t update MIDI control.'}); return false; }
    finally { setBusy(false); }
  };
  const learning = (midi.learning ?? -1) >= 0;
  return <section className="midi-panel" aria-label="MIDI foot control">
    <div className="midi-heading"><div><span className="practice-kicker">PLAY · SWITCH · EXPRESS</span><h2>MIDI foot control</h2></div><label><input type="checkbox" aria-label="Enable MIDI mapping" checked={!!config.enabled} disabled={!native || busy} onChange={e => action('setMidiEnabled', e.target.checked)}/> Enable mapping</label></div>
    {!native && <p className="practice-note">Open Cassian to connect a controller. Browser preview has no MIDI input.</p>}
    {native && (status.deviceSettingsAvailable ? <div className="midi-devices"><strong>MIDI inputs</strong>{status.midiInputs?.length ? status.midiInputs.map(device => <label key={device.id}><input type="checkbox" aria-label={`MIDI input ${device.name}`} checked={!!device.enabled} disabled={busy} onChange={e => action('setMidiInput', device.id, e.target.checked)}/>{device.name}</label>) : <span>No MIDI inputs detected. Connect your controller or open Audio settings.</span>}</div> : <p className="practice-note">Route MIDI to Cassian in your DAW. The host selects the controller and input port.</p>)}
    <div className="midi-layout"><div className="midi-slots" aria-label="MIDI assignments">{rows.map((row, i) => <button key={i} aria-pressed={slot === i} className={slot === i ? 'selected' : ''} onClick={() => { setSlot(i); setDirty(false); setDraft(row); }}><strong>{i + 1} · {actions.find(([id]) => id === row.action)?.[1] ?? 'Unassigned'}</strong><small>{row.type.toUpperCase()} {row.number} · {row.channel === 0 ? 'All channels' : `Channel ${row.channel}`}</small></button>)}</div>
    <form className="midi-editor" onSubmit={async e => { e.preventDefault(); if (await action('setMidiMapping', slot, draft)) setDirty(false); }}>
      <div className="midi-fields"><label>Message<select aria-label="MIDI message type" value={draft.type} disabled={!native || busy || learning} onChange={e => change({type: e.target.value})}><option value="cc">Control change · CC</option><option value="pc" disabled={expression(draft.action)}>Program change · PC</option></select></label><label>Number<input aria-label="MIDI number" type="number" min={0} max={127} value={draft.number} disabled={!native || busy || learning} onChange={e => change({number: Number(e.target.value)})}/></label><label>Channel<select aria-label="MIDI channel" value={draft.channel} disabled={!native || busy || learning} onChange={e => change({channel: Number(e.target.value)})}><option value={0}>All channels</option>{Array.from({length: 16}, (_, i) => <option key={i + 1} value={i + 1}>{i + 1}</option>)}</select></label></div>
      <label>Action<select aria-label="MIDI action" value={draft.action} disabled={!native || busy || learning} onChange={e => change({action: e.target.value, ...(expression(e.target.value) ? {type: 'cc'} : {})})}>{actions.map(([id, label]) => <option key={id} value={id}>{label}</option>)}</select></label>
      {draft.action === 'rig' && <label>Saved rig<select aria-label="MIDI saved rig" value={draft.rig} disabled={!native || busy || learning} onChange={e => change({rig: e.target.value})}><option value="">Choose a saved rig</option>{draft.rig && !rigs.some(r => r.id === draft.rig) && <option value={draft.rig}>Missing saved rig</option>}{rigs.map(r => <option key={r.id} value={r.id}>{r.name}</option>)}</select></label>}
      {draft.action === 'scene' && <label>Scene<select aria-label="MIDI scene" value={draft.scene ?? 0} disabled={!native || busy || learning} onChange={e => change({scene: Number(e.target.value)})}>{Array.from({length: 4}, (_, i) => <option key={i} value={i}>{i + 1} · {status.scenes?.slots?.[i]?.stored ? status.scenes.slots[i].name : 'Empty'}</option>)}</select></label>}
      {expression(draft.action) && <label><input type="checkbox" aria-label="Invert expression" checked={!!draft.inverted} disabled={!native || busy || learning} onChange={e => change({inverted: e.target.checked})}/> Reverse pedal direction</label>}
      <div className="midi-actions"><button type="submit" disabled={!native || !dirty || busy || learning || (draft.action === 'rig' && !draft.rig)}>Apply assignment</button><button type="button" disabled={!native || dirty || busy || learning} onClick={() => action('learnMidi', slot)}>Learn controller</button>{learning && <button type="button" disabled={busy} onClick={() => action('learnMidi', -1)}>Cancel learn</button>}</div>
      {dirty && <p className="practice-note">Apply changes before learning this controller. Selecting another assignment discards these unsaved edits.</p>}
      {learning && <p className="midi-learning" role="status">Listening for assignment {(midi.learning ?? 0) + 1}… press a switch or move its expression pedal. Learning won’t execute the action.</p>}
      <p className="practice-note">CC toggles trigger once when the value crosses 64; release below 64 before pressing again. PC numbers use 0–127. Master expression spans −60 to 0 dB. Mappings stay with this app/DAW session when rigs change.</p>
    </form></div>
    <div className="midi-monitor"><span>Last input: {midi.lastInput || 'Waiting for MIDI'}</span>{midi.lastAction && <span>Last action: {actions.find(([id]) => id === midi.lastAction)?.[1] ?? midi.lastAction}</span>}</div>
    {midi.dropped > 0 && <p className="practice-error" role="alert">{midi.dropped} MIDI messages exceeded the queue limit. Reduce controller traffic.</p>}
    {midi.error && <p className="practice-error" role="alert">{midi.error}</p>}
  </section>;
}
