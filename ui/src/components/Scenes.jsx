import { useEffect, useState } from 'react';
import { invoke, native } from '../juce/bridge.js';
const empty = Array.from({length: 4}, () => ({stored: false, name: ''}));

export default function Scenes({status, onError = () => {}}) {
  const bank = status.scenes ?? {}, rows = bank.slots ?? empty;
  const [selected, setSelected] = useState(0), [editing, setEditing] = useState(false), [name, setName] = useState('Scene 1'), [busy, setBusy] = useState(false);
  const savedName = rows[selected]?.name || `Scene ${selected + 1}`;
  const [destination, setDestination] = useState(-1);
  useEffect(() => { setName(savedName); }, [selected, savedName]);
  useEffect(() => { setDestination(-1); }, [selected]);
  const blocked = busy || !!status.rigLoading;
  const action = async (method, ...args) => {
    if (blocked) return; setBusy(true);
    try { const failure = await invoke(method, ...args); if (failure) throw new Error(String(failure)); }
    catch (e) { onError({title: 'Scenes', text: e.message || 'Couldn’t update this scene.'}); }
    finally { setBusy(false); }
  };
  return <section className="scene-bank" aria-label="Performance scenes">
    <div className="scene-strip"><span className="scene-heading">SCENES</span>{rows.map((row, i) => <button key={i} className={`scene-slot${bank.active === i ? ' active' : ''}${selected === i ? ' selected' : ''}`} aria-label={`Scene ${i + 1}: ${row.stored ? row.name : 'Empty'}`} aria-pressed={bank.active === i} disabled={blocked} onClick={() => { setSelected(i); if (native && row.stored && !editing) action('recallScene', i); }}><span>{i + 1}</span><strong>{row.stored ? row.name : 'Empty'}</strong>{bank.active === i && bank.edited && <small>Edited</small>}</button>)}<button className="text-button" aria-expanded={editing} onClick={() => setEditing(!editing)}>{editing ? 'Close scene edit' : 'Edit scenes'}</button></div>
    {editing && <form className="scene-editor" onSubmit={e => { e.preventDefault(); action('storeScene', selected, name.trim()); }}>
      <label>Scene {selected + 1}<input aria-label="Scene name" maxLength={48} value={name} disabled={blocked || !native} onChange={e => setName(e.target.value)}/></label>
      <button type="submit" disabled={!native || blocked || !name.trim()}>{rows[selected]?.stored ? 'Replace with current tone' : 'Store current tone'}</button>
      <button type="button" disabled={!native || blocked || !rows[selected]?.stored || !name.trim() || name.trim() === savedName} onClick={() => action('renameScene', selected, name.trim())}>Rename scene</button>
      <button type="button" disabled={!native || blocked || !rows[selected]?.stored} onClick={() => action('clearScene', selected)}>Clear scene</button>
      <label>Copy saved scene to<select aria-label="Scene copy destination" value={destination} disabled={!native || blocked || !rows[selected]?.stored} onChange={e => setDestination(Number(e.target.value))}><option value={-1}>Choose a slot</option>{rows.map((row,i)=><option key={i} value={i} disabled={i===selected}>{i+1} · {row.stored?row.name:'Empty'}</option>)}</select></label>
      <button type="button" disabled={!native || blocked || !rows[selected]?.stored || destination < 0 || destination === selected || !name.trim()} onClick={() => action('copyScene', selected, destination, name.trim())}>{rows[destination]?.stored ? `Replace scene ${destination+1} with copy` : 'Copy saved scene'}</button>
      <p>Copy uses the selected scene's saved settings and the name above. It replaces the destination slot without changing your current tone.</p>
      <p>Selecting a slot while editing does not recall it. Rename keeps its saved sound; Replace stores the current tone.</p>
      <p>Scenes share the current amp, pedal and cabinet files. Input, Master and click settings stay unchanged. Save the complete rig to keep all four scenes.</p>
      {!native && <p>Open Cassian to store and recall scenes. Browser preview has no native scene bank.</p>}
    </form>}
    {bank.error && <p className="practice-error" role="alert">{bank.error}</p>}
  </section>;
}
