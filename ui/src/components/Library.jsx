import { useEffect, useRef, useState } from 'react';
import { invoke, native } from '../juce/bridge.js';
import { restoreSnapshot, setParameter, snapshotParameters } from '../parameterState.js';
import { applyPreset, presets } from '../presets.js';

const builtins = [
  { id: 'lumen', name: 'Lumen', kind: 'amp', source: 1, ownership: 'Factory', tags: 'clean warm jazz', notes: 'Cassian built-in clean amp.' },
  { id: 'ferrum', name: 'Ferrum', kind: 'amp', source: 2, ownership: 'Factory', tags: 'high gain rock metal lead', notes: 'Cassian built-in high-gain amp.' },
  { id: 'natural', name: 'Natural DI', kind: 'amp', source: 4, ownership: 'Factory', tags: 'nylon acoustic neutral clean', notes: 'Neutral input path. No electric-to-nylon simulation. Select an external body IR intentionally if needed.' },
];
const previewRigs = () => { try { return JSON.parse(localStorage.getItem('cassian-preview-rigs') || '[]'); } catch { return []; } };
const writePreviewRigs = rigs => localStorage.setItem('cassian-preview-rigs', JSON.stringify(rigs));
const readFavorites = () => { try { return JSON.parse(localStorage.getItem('cassian-factory-favorites') || '{}'); } catch { return {}; } };

export default function Library({ revision, onClose, onPreset = applyPreset }) {
  const [tab, setTab] = useState('amp'), [search, setSearch] = useState(''), [ownership, setOwnership] = useState('All');
  const [favorites, setFavorites] = useState(false), [selected, setSelected] = useState(null), [name, setName] = useState('');
  const [catalog, setCatalog] = useState({ assets: [], rigs: [] }), [error, setError] = useState(''), [busy, setBusy] = useState(false);
  const [previewFavorites, setPreviewFavorites] = useState(readFavorites);
  const panel = useRef(null);
  useEffect(() => {
    const previous = document.activeElement;
    panel.current?.querySelector('button')?.focus();
    return () => previous?.focus();
  }, []);
  const refresh = async () => {
    const next = native ? await invoke('getLibrary') : { assets: [], rigs: previewRigs() };
    setCatalog(next || {assets: [], rigs: []});
  };
  useEffect(() => {
    let active = true;
    (native ? invoke('getLibrary') : Promise.resolve({assets: [], rigs: previewRigs()}))
      .then(next => { if (active && next) setCatalog(next); })
      .catch(() => { if (active) setError('Could not read the library.'); });
    return () => { active = false; };
  }, [revision]);
  const action = async run => {
    if (busy) return; setBusy(true); setError('');
    try { const result = await run(); if (typeof result === 'string' && result) throw new Error(result); await refresh(); }
    catch (e) { setError(e.message || 'Could not complete the library action.'); }
    finally { setBusy(false); }
  };
  const use = row => action(async () => {
    if (row.preset) await onPreset(row.name);
    else if (row.source != null) {
      if (row.source === 4) applyPreset('Natural Nylon'); else setParameter('AMP_SOURCE', row.source);
    } else if (row.kind === 'rig') {
      if (native) { const result = await invoke('loadRig', row.id); if (result) return result; }
      else restoreSnapshot(row.parameters);
    } else if (!await invoke('selectAsset', row.id)) return 'Asset is missing. Relink the original file first.';
    onClose();
  });
  const save = e => {
    e.preventDefault(); if (!name.trim()) return;
    action(async () => {
      if (native) { const result = await invoke('saveRig', name.trim()); if (result) return result; }
      else writePreviewRigs([...previewRigs(), {id: `preview-${Date.now()}-${Math.random()}`, name: name.trim(), parameters: snapshotParameters()}]);
      setName(''); setTab('rig');
    });
  };
  const favorite = row => action(async () => {
    if (row.source != null || row.preset) setPreviewFavorites(prev => {
      const next = {...prev, [row.id]: !prev[row.id]};
      localStorage.setItem('cassian-factory-favorites', JSON.stringify(next)); return next;
    });
    else if (native) await invoke('editAsset', row.id, {favorite: !row.favorite});
    else writePreviewRigs(previewRigs().map(r => r.id === row.id ? {...r, favorite: !r.favorite} : r));
  });
  const rows = tab === 'rig' ? [
    ...Object.keys(presets).map(title => ({id: `preset-${title}`, name: title, kind: 'rig', preset: true, ownership: 'Factory', tags: 'starting point'})),
    ...(catalog.rigs || []).map(r => ({...r, kind: 'rig', ownership: 'User'})),
  ] : [...builtins, ...(catalog.assets || [])].filter(r => r.kind === tab);
  const shown = rows.filter(r => (ownership === 'All' || r.ownership === ownership)
    && (!favorites || (r.favorite || previewFavorites[r.id]))
    && `${r.name} ${r.gear || ''} ${r.creator || ''} ${r.tags || ''} ${r.notes || ''}`.toLowerCase().includes(search.toLowerCase()));
  const detail = selected && rows.find(r => r.id === selected);
  return <div className="library-overlay" onKeyDown={e => {
    if (e.key === 'Escape') { e.stopPropagation(); onClose(); }
    if (e.key === 'Tab') {
      const elements = [...panel.current.querySelectorAll('button, input, select, textarea')].filter(el => !el.disabled);
      const first = elements[0], last = elements.at(-1);
      if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last?.focus(); }
      else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first?.focus(); }
    }
  }}>
    <section ref={panel} className="library-panel" role="dialog" aria-modal="true" aria-labelledby="library-title">
      <div className="library-heading"><h2 id="library-title">Your library</h2><button className="text-button" onClick={onClose}>Close library</button></div>
      <div className="library-tabs">{[['amp','Amps'], ['pedal','Pedals'], ['cab','Cabinets'], ['rig','Presets']].map(([id,label]) =>
        <button className={`chip${tab === id ? ' active' : ''}`} aria-pressed={tab === id} key={id} onClick={() => { setTab(id); setSelected(null); }}>{label}</button>)}</div>
      <div className="library-filters">
        <input aria-label="Search library" placeholder="Search gear, creator, tone or genre…" value={search} onChange={e => setSearch(e.target.value)} />
        <select aria-label="Library ownership" value={ownership} onChange={e => setOwnership(e.target.value)}>{['All','Factory','User'].map(x => <option key={x}>{x}</option>)}</select>
        <label><input type="checkbox" checked={favorites} onChange={e => setFavorites(e.target.checked)} /> Favorites</label>
      </div>
      {error && <p className="library-error" role="alert">{error}</p>}
      <div className="library-content"><div className="library-results">
        {shown.map(row => <article key={row.id} className={`library-row${selected === row.id ? ' selected' : ''}`}>
          <button className="library-info" onClick={() => setSelected(row.id)}><strong>{row.name}</strong><small>{row.ownership}{row.missing ? ' · Missing file' : row.sampleRate > 0 ? ` · ${row.sampleRate / 1000} kHz` : ''}{row.preset ? ' · Starting point' : ''}</small></button>
          <button className="text-button quiet" aria-label={`Favorite ${row.name}`} aria-pressed={Boolean(row.favorite || previewFavorites[row.id])} disabled={busy} onClick={() => favorite(row)}>☆</button>
          {row.missing ? <button className="text-button" disabled={busy || !native} onClick={() => action(() => invoke('relinkAsset', row.id))}>Relink</button>
            : <button className="text-button" disabled={busy} onClick={() => use(row)}>Use</button>}
          {row.kind === 'rig' && !row.preset && <button className="text-button quiet" disabled={busy} aria-label={`Remove rig ${row.name}`} onClick={() => action(async () => {
            if (native) await invoke('removeRig', row.id); else writePreviewRigs(previewRigs().filter(r => r.id !== row.id));
          })}>Remove</button>}
        </article>)}
        {!shown.length && <p className="library-empty">No matches. Import your own files or change the filters.</p>}
      </div>{detail && <aside className="library-detail"><h3>{detail.name}</h3><p>{detail.notes || detail.gear || 'Saved rig with amp, pedal, cabinet, routing, and effect settings.'}</p>
        {detail.creator && <p>Creator: {detail.creator}</p>}
        {detail.inputLevelDbu != null && <p>Capture input calibration: {detail.inputLevelDbu} dBu</p>}
        {detail.rights && <p>{detail.rights}</p>}
        {detail.path && <p className="library-path">{detail.path}</p>}
        {detail.kind === 'cab' && <button className="text-button" disabled={!native || busy || detail.missing} onClick={() => action(async () => {
          if (!await invoke('selectAsset', detail.id, 'cabB')) return 'Cabinet is missing. Relink the original file first.';
        })}>Use as cabinet B</button>}
        {detail.kind !== 'rig' && detail.ownership === 'User' && <form onSubmit={e => { e.preventDefault(); const data = new FormData(e.currentTarget);
          action(() => invoke('editAsset', detail.id, Object.fromEntries(data))); }}>
          <label>Friendly name<input name="name" defaultValue={detail.name} key={`name-${detail.id}`} /></label>
          <label>Tone / genre / gain tags<input name="tags" defaultValue={detail.tags || ''} key={`tags-${detail.id}`} /></label>
          <label>Creator<input name="creator" defaultValue={detail.creator || ''} key={`creator-${detail.id}`} /></label>
          <label>Source URL<input name="sourceURL" defaultValue={detail.sourceURL || ''} key={`url-${detail.id}`} /></label>
          <label>Capture settings / mic / pickup notes<textarea name="notes" defaultValue={detail.notes || ''} key={`notes-${detail.id}`} /></label>
          <button className="text-button" disabled={busy}>Save metadata</button>
        </form>}
      </aside>}</div>
      <div className="library-footer">
        {tab !== 'rig' ? <button className="text-button" disabled={!native || busy} onClick={() => action(() => invoke('importAssets', tab))}>Import {tab === 'cab' ? 'WAV IRs' : 'NAM files'}</button>
          : <><button className="text-button" disabled={!native || busy} onClick={() => action(() => invoke('importRig'))}>Import rig</button><button className="text-button" disabled={!native || busy} onClick={() => action(() => invoke('exportRig'))}>Export current rig</button><button className="text-button" disabled={!native || busy} onClick={() => action(() => invoke('importRigPack'))}>Import pack</button><button className="text-button" disabled={!native || busy} onClick={() => action(() => invoke('exportRigPack'))}>Export pack</button></>}
        <form onSubmit={save}><input aria-label="Rig name" placeholder="Name this rig" value={name} maxLength={80} onChange={e => setName(e.target.value)} /><button className="text-button" disabled={busy || !name.trim()}>Save current rig</button></form>
      </div>
      <p className="library-note">{native ? 'Shared library keeps managed asset copies. Rig JSON references files; portable ZIP packs include the selected amp, pedal, and cabinet files.' : 'Browser preview: saved rigs contain control settings. Play and import files in the native app.'}</p>
    </section>
  </div>;
}
