import { useEffect, useState } from 'react';
import { invoke, native } from '../juce/bridge.js';
import { boardTypes, boardControls, boardId, byId } from '../parameters.js';
import { setParameter, useParameter } from '../parameterState.js';
import Knob from './Knob.jsx';
import Switch from './Switch.jsx';
import Scenes from './Scenes.jsx';
const names = ['Compressor', 'Overdrive', 'Captured pedal', 'EQ', 'Modulation', 'Chorus', 'Delay', 'Reverb', 'Ambience'];
const choices = { MOD_TYPE: ['Phaser', 'Flanger', 'Tremolo'], MOD_DIVISION: ['Whole', 'Half', 'Quarter', 'Eighth', 'Dotted eighth'], DELAY_DIVISION: ['Quarter', 'Eighth', 'Dotted eighth', 'Sixteenth', 'Half', 'Whole'], REVERB_STYLE: ['Room', 'Chamber', 'Hall'] };
function Control({kind, slot, base}) {
  const id = boardId(kind, slot, base), value = useParameter(id), p = byId[id];
  if (choices[base]) return <label className="slot-choice">{p.label}<select aria-label={p.label} value={Math.round(value)} onChange={e => setParameter(id, Number(e.target.value))}>{choices[base].map((name, index) => <option key={name} value={index}>{name}</option>)}</select></label>;
  if (!p.unit) return <label className="board-boolean">{p.label}<Switch id={id} name={p.label}/></label>;
  return <Knob id={id} small/>;
}
export default function Pedalboard({status, onError = () => {}}) {
  const board = status.board ?? {}, rows = board.blocks ?? [];
  const [selected, setSelected] = useState(null), [type, setType] = useState('overdrive'), [lane, setLane] = useState('pre');
  const [busy, setBusy] = useState(false), [assets, setAssets] = useState([]);
  const [replacement, setReplacement] = useState('eq');
  const blocked = !native || busy || board.loading || status.rigLoading;
  const current = rows.find(row => row.id === selected) ?? rows[0];
  const kind = boardTypes.indexOf(current?.type);
  useEffect(() => {
    let alive = true;
    if (native && board.serial) invoke('getLibrary').then(catalog => { if (alive) setAssets(catalog?.assets ?? []); }).catch(() => {});
    return () => { alive = false; };
  }, [status.libraryRevision, board.serial]);
  const command = async (action, args = {}) => {
    if (blocked) return; setBusy(true);
    try { const error = await invoke('boardCommand', action, args); if (error) throw new Error(error); }
    catch (e) { onError({title: 'Pedalboard', text: e.message || 'Could not edit this board.'}); }
    finally { setBusy(false); }
  };
  if (!board.serial) return <section className="serial-intro" aria-label="Serial pedalboard">
    <div><h2>Your pedalboard</h2><p>This rig uses the original effect chain. Enable serial editing to duplicate pedals and change their order.</p><p className="board-note">Conversion can change compression and reverb level. Undo restores the original chain.</p></div>
    <button className="text-button" disabled={blocked} onClick={() => command('convert')}>Enable serial editing</button>
    {!native && <p className="board-note">Open Cassian to edit an audio pedalboard.</p>}
  </section>;
  return <section className="serial-board" aria-label="Serial pedalboard">
    <div className="board-toolbar"><h2>Your pedalboard</h2><span className="board-count">{rows.length} pedals · {board.reserved}/16 reserved slots</span><button className="text-button" disabled={blocked || !board.canUndo} onClick={() => command('undo')}>Undo</button><button className="text-button" disabled={blocked || !board.canRedo} onClick={() => command('redo')}>Redo</button></div>
    <div className="board-lanes">{['pre','post'].map(position => <div role="region" className="board-lane" key={position} aria-label={position === 'pre' ? 'Before amp' : 'After cabinet'}>
      <div className="board-lane-heading"><h3>{position === 'pre' ? 'Before amp' : 'After cabinet'}</h3><small>{position === 'pre' ? 'Guitar → pedals → amp' : 'Cabinet → pedals → output'}</small></div>
      <div className="board-pedals" onDragOver={e => { if (!blocked) e.preventDefault(); }} onDrop={e => { e.preventDefault(); command('moveTo', {id: e.dataTransfer.getData('text/plain'), beforeId: '', lane: position}); }}>{rows.filter(row => row.lane === position).map((row, index, list) => <article key={row.id} className={`board-pedal${current?.id === row.id ? ' selected' : ''}`} draggable={!blocked} onDragStart={e => { e.dataTransfer.setData('text/plain', row.id); e.dataTransfer.effectAllowed = 'move'; }} onDragOver={e => { if (!blocked) e.preventDefault(); }} onDrop={e => { e.preventDefault(); e.stopPropagation(); command('moveTo', {id: e.dataTransfer.getData('text/plain'), beforeId: row.id, lane: position}); }}>
        <button className="board-pedal-title" aria-pressed={current?.id === row.id} onClick={() => setSelected(row.id)}><span>{String(index + 1).padStart(2, '0')}</span><strong>{names[boardTypes.indexOf(row.type)]} {Number(row.automationSlot) + 1}</strong><small>{row.assetName || 'Edit controls'}</small></button>
        <Switch id={row.enabledId} name={`Bypass ${names[boardTypes.indexOf(row.type)]} ${Number(row.automationSlot) + 1}`} disabled={blocked || ((row.type === 'neural-pedal' || row.type === 'ambience') && !row.assetId)}/>
        <div className="board-pedal-moves"><button aria-label={`Move ${names[boardTypes.indexOf(row.type)]} ${Number(row.automationSlot) + 1} earlier`} disabled={blocked || index === 0} onClick={() => command('move', {id: row.id, direction: -1})}>←</button><button aria-label={`Move ${names[boardTypes.indexOf(row.type)]} ${Number(row.automationSlot) + 1} later`} disabled={blocked || index === list.length - 1} onClick={() => command('move', {id: row.id, direction: 1})}>→</button></div>
      </article>)}{!rows.some(row => row.lane === position) && <p className="board-empty">Add a pedal to this lane.</p>}</div>
    </div>)}</div>
    <form className="board-add" onSubmit={e => { e.preventDefault(); command('add', {type, lane}); }}><label>Pedal<select aria-label="New pedal type" value={type} disabled={blocked} onChange={e => { setType(e.target.value); if (['overdrive','neural-pedal'].includes(e.target.value)) setLane('pre'); }}>{boardTypes.map((id, i) => <option key={id} value={id}>{names[i]}</option>)}</select></label><label>Position<select aria-label="New pedal position" value={lane} disabled={blocked || ['overdrive','neural-pedal'].includes(type)} onChange={e => setLane(e.target.value)}><option value="pre">Before amp</option><option value="post">After cabinet</option></select></label><button className="text-button" type="submit" disabled={blocked}>Add pedal</button></form>
    {current && <section className="board-inspector" aria-label="Selected pedal controls" key={current.id}>
      <div className="board-inspector-heading"><h3>{names[kind]} {Number(current.automationSlot) + 1}</h3><div><button className="text-button" disabled={blocked} onClick={() => command('duplicate', {id: current.id})}>Duplicate</button><button className="text-button" disabled={blocked || kind === 1 || kind === 2} onClick={() => command('lane', {id: current.id, lane: current.lane === 'pre' ? 'post' : 'pre'})}>{current.lane === 'pre' ? 'Move after cabinet' : 'Move before amp'}</button><button className="text-button quiet" disabled={blocked} onClick={() => command('remove', {id: current.id})}>Remove</button></div></div>
      {(kind === 2 || kind === 8) && <label className="board-capture">{kind === 2 ? 'Pedal capture' : 'Ambience response'}<select aria-label={kind === 2 ? 'Pedal capture' : 'Ambience response'} value={current.assetId || ''} disabled={blocked} onChange={e => command('capture', {id: current.id, assetId: e.target.value})}><option value="" disabled>Select from your library</option>{assets.filter(asset => asset.kind === (kind === 8 ? 'ambience' : 'pedal') && !asset.missing).map(asset => <option key={asset.id} value={asset.id}>{asset.name}</option>)}</select></label>}
      <form className="board-replace" onSubmit={e => { e.preventDefault(); command('replace', {id: current.id, type: replacement}); }}><label>Replace with<select aria-label="Replacement pedal type" value={replacement} disabled={blocked} onChange={e => setReplacement(e.target.value)}>{boardTypes.map((id, i) => <option key={id} value={id}>{names[i]}</option>)}</select></label><button className="text-button quiet" disabled={blocked}>Replace pedal</button></form>
      <div className="board-controls">{boardControls[kind].filter(id => !id.endsWith('_ON')).map(base => <Control key={base} base={base} kind={kind} slot={Number(current.automationSlot)}/>)}<Knob id={current.trimId} small/></div>
      {kind === 8 && <p className="board-note">Recorded ambience keeps its captured decay and repeats. Blend and output trim set its place in the tone.</p>}
    </section>}
    <p className="board-note">Drag pedals or use the arrow buttons to reorder. Removed pedals keep their automation slots reserved. Undo restores them. Board changes briefly fade the guitar and restart effect tails.</p>
    <p className="board-note">Multiple captured pedals and long ambience responses use more CPU. If you hear crackles, try a 256 or 512 sample buffer in Audio settings or your DAW.</p>
    <Scenes status={status} onError={onError}/>
  </section>;
}
