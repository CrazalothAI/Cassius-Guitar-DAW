import { useEffect, useRef, useState } from 'react';
import { invoke, native } from '../juce/bridge.js';
import { restoreSnapshot, setParameter, snapshotParameters } from '../parameterState.js';
import { applyPreset, presets, notes, familyOf } from '../presets.js';
import { applyStartingPreview, resolveStartingRigs } from '../startingRigs.js';
import { catalogRow, captureLabels, gainLabels, matchesCatalog, title } from '../libraryCatalog.js';

const builtins = [
  { id: 'lumen', name: 'Lumen', kind: 'amp', source: 1, ownership: 'Factory', tags: 'clean warm jazz', notes: 'Cassian built-in clean amp.' },
  { id: 'ferrum', name: 'Ferrum', kind: 'amp', source: 2, ownership: 'Factory', tags: 'high gain rock metal lead', notes: 'Cassian built-in high-gain amp.' },
  { id: 'natural', name: 'Natural DI', kind: 'amp', source: 4, ownership: 'Factory', tags: 'nylon acoustic neutral clean', notes: 'Neutral input path. No electric-to-nylon simulation. Select an external body IR intentionally if needed.' },
];
export const previewRigs = () => { try { return JSON.parse(localStorage.getItem('cassian-preview-rigs') || '[]'); } catch { return []; } };
export const writePreviewRigs = rigs => localStorage.setItem('cassian-preview-rigs', JSON.stringify(rigs));
const readFavorites = () => { try { return JSON.parse(localStorage.getItem('cassian-factory-favorites') || '{}'); } catch { return {}; } };

export default function Library({ revision, loading = false, onClose, onPreset = applyPreset, onPreviewRig = () => {}, previewActiveId }) {
  const [tab, setTab] = useState('amp'), [search, setSearch] = useState(''), [ownership, setOwnership] = useState('All');
  const [favorites, setFavorites] = useState(false), [selected, setSelected] = useState(null), [name, setName] = useState('');
  const [catalog, setCatalog] = useState({ assets: [], rigs: [] }), [error, setError] = useState(''), [busy, setBusy] = useState(false);
  const [previewFavorites, setPreviewFavorites] = useState(readFavorites);
  const [style, setStyle] = useState(''), [gain, setGain] = useState('all'), [speaker, setSpeaker] = useState(''), [pack, setPack] = useState('');
  const [rigType, setRigType] = useState(''), [sort, setSort] = useState('name');
  const [availability, setAvailability] = useState(''), [captureType, setCaptureType] = useState('');
  const [rigDetails, setRigDetails] = useState(null);
  const clearFilters = () => { setSearch(''); setOwnership('All'); setFavorites(false); setStyle(''); setGain('all'); setSpeaker(''); setPack(''); setRigType(''); setAvailability(''); setCaptureType(''); };
  const panel = useRef(null), actionPending = useRef(false);
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
  useEffect(() => {
    let active = true; setRigDetails(null);
    if (native && tab === 'rig' && catalog.rigs?.some(r => r.id === selected)) invoke('inspectRig', selected)
      .then(result => { if (active) setRigDetails({id:selected, assets:result?.assets || [], error:result?.error}); })
      .catch(() => { if (active) setRigDetails({id:selected, assets:[], error:'Could not inspect this saved rig.'}); });
    return () => { active = false; };
  }, [selected, tab, catalog]);
  const action = async run => {
    if (actionPending.current) return; actionPending.current = true; setBusy(true); setError('');
    try { const result = await run(); if (typeof result === 'string' && result) throw new Error(result); if (result === false) throw new Error('Could not complete the library action. Please try again.'); await refresh(); }
    catch (e) { setError(e.message || 'Could not complete the library action.'); }
    finally { actionPending.current = false; setBusy(false); }
  };
  const use = row => action(async () => {
    if (row.starter) {
      if (native) { const result = await invoke('loadStartingRig', row.id); if (result) return result; }
      else onPreviewRig(applyStartingPreview(row.id));
    } else if (row.preset) await onPreset(row.name);
    else if (row.source != null) {
      if (row.source === 4) applyPreset('Natural Nylon'); else setParameter('AMP_SOURCE', row.source);
    } else if (row.kind === 'rig') {
      if (native) { const result = await invoke('loadRig', row.id); if (result) return result; }
      else { restoreSnapshot(row.parameters); onPreviewRig(row); }
    } else if (row.kind === 'ambience') return 'Add an Ambience pedal in Board, then select this response there.';
    else if (!await invoke('selectAsset', row.id)) return row.kind === 'pedal' ? 'Could not select this pedal. Relink a missing file or add a Captured pedal in Board.' : 'Asset is missing. Relink the original file first.';
    onClose();
  });
  const save = e => {
    e.preventDefault(); if (!name.trim()) return;
    action(async () => {
      if (native) { const result = await invoke('saveRig', name.trim()); if (result) return result; }
      else { const row = {id: `preview-${Date.now()}-${Math.random()}`, name: name.trim(), parameters: snapshotParameters()}; writePreviewRigs([...previewRigs(), row]); onPreviewRig(row); }
      setName(''); setTab('rig');
    });
  };
  const favorite = row => action(async () => {
    if (row.source != null || row.preset || row.starter) setPreviewFavorites(prev => {
      const next = {...prev, [row.id]: !prev[row.id]};
      localStorage.setItem('cassian-factory-favorites', JSON.stringify(next)); return next;
    });
    else if (native) return invoke(row.kind === 'rig' ? 'editRig' : 'editAsset', row.id, {favorite: !row.favorite});
    else writePreviewRigs(previewRigs().map(r => r.id === row.id ? {...r, favorite: !r.favorite} : r));
  });
  const rows = tab === 'rig' ? [
    ...resolveStartingRigs(catalog.assets, !native),
    ...Object.keys(presets).map(name => ({id: `preset-${name}`, name, kind: 'rig', preset: true, ownership: 'Factory', styles: [familyOf(name)?.toLowerCase()].filter(Boolean), gain: presets[name].AMP_CLEAN ? 'clean' : 'high-gain', tags: 'control starting point', notes: notes[name]})),
    ...(catalog.rigs || []).map(r => ({...r, kind: 'rig', ownership: 'User'})),
  ] : [...builtins, ...(catalog.assets || [])].filter(r => r.kind === tab);
  const categorized = rows.map(catalogRow), starred = r => Boolean(r.favorite || previewFavorites[r.id]);
  const options = field => [...new Set(categorized.flatMap(r => Array.isArray(r[field]) ? r[field] : [r[field]]).filter(Boolean))].sort((a,b) => a.localeCompare(b));
  const shown = categorized.filter(r => matchesCatalog(r, {ownership, favorites, search, style, gain, speaker, pack, rigType, availability:tab === 'rig' ? '' : availability, captureType:tab === 'amp' ? captureType : ''}, starred(r)))
    .sort((a,b) => (sort === 'favorites' ? Number(starred(b)) - Number(starred(a)) : 0) || String(a.name || '').localeCompare(String(b.name || '')));
  const detail = selected && categorized.find(r => r.id === selected);
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
      <div className="library-tabs">{[['amp','Amps'], ['pedal','Pedals'], ['cab','Cabinets'], ['ambience','Ambience'], ['rig','Presets']].map(([id,label]) =>
        <button className={`chip${tab === id ? ' active' : ''}`} aria-pressed={tab === id} key={id} onClick={() => { setTab(id); setSelected(null); setStyle(''); setGain('all'); setSpeaker(''); setPack(''); setRigType(''); }}>{label}</button>)}</div>
      <div className="library-filters">
        <input aria-label="Search library" placeholder="Search gear, creator, tone or genre…" value={search} onChange={e => setSearch(e.target.value)} />
        <select aria-label="Library ownership" value={ownership} onChange={e => setOwnership(e.target.value)}>{['All','Factory','User'].map(x => <option key={x}>{x}</option>)}</select>
        <label><input type="checkbox" checked={favorites} onChange={e => setFavorites(e.target.checked)} /> Favorites</label>
      </div>
      <div className="library-tone-filters">
        <label>Style<select aria-label="Library style" value={style} onChange={e => setStyle(e.target.value)}><option value="">All styles</option>{options('styles').map(x => <option key={x} value={x}>{title(x)}</option>)}</select></label>
        <label>Gain<select aria-label="Library gain" value={gain} onChange={e => setGain(e.target.value)}><option value="all">All gain levels</option>{Object.entries(gainLabels).map(([id,label]) => <option key={id} value={id}>{label}</option>)}</select></label>
        <label>Speaker<select aria-label="Library speaker" value={speaker} onChange={e => setSpeaker(e.target.value)}><option value="">All speakers</option>{options('speaker').map(x => <option key={x} value={x}>{x}</option>)}</select></label>
        {tab !== 'rig' && <label>File status<select aria-label="Library file status" value={availability} onChange={e => setAvailability(e.target.value)}><option value="">All sounds</option><option value="available">Available</option><option value="missing">Missing files</option></select></label>}
        {tab === 'amp' && <label>Capture type<select aria-label="Library capture type" value={captureType} onChange={e => setCaptureType(e.target.value)}><option value="">All amp types</option>{Object.entries(captureLabels).map(([id,label]) => <option key={id} value={id}>{label}</option>)}</select></label>}
        <label>Source pack<select aria-label="Library source pack" value={pack} onChange={e => setPack(e.target.value)}><option value="">All packs</option>{options('pack').map(x => <option key={x} value={x}>{x.replace(/\.zip$/i, '')}</option>)}</select></label>
        {tab === 'rig' && <label>Rig type<select aria-label="Library rig type" value={rigType} onChange={e => setRigType(e.target.value)}><option value="">All rig types</option><option value="starter">Complete starter rigs</option><option value="saved">Saved rigs</option><option value="controls">Control starting points</option></select></label>}
        <label>Sort<select aria-label="Library sort" value={sort} onChange={e => setSort(e.target.value)}><option value="name">Name</option><option value="favorites">Favorites first</option></select></label>
        <button className="text-button quiet" onClick={clearFilters}>Clear filters</button>
      </div>
      <p className="library-count" role="status">{shown.length} of {rows.length} {tab === 'rig' ? 'rigs and starting points' : 'sounds'}</p>
      {error && <p className="library-error" role="alert">{error}</p>}
      <div className="library-content"><div className="library-results">
        {shown.map(row => <article key={row.id} className={`library-row${selected === row.id ? ' selected' : ''}`}>
          <button className="library-info" onClick={() => setSelected(row.id)}><strong>{row.name}</strong><small>{row.ownership}{row.starter ? ' · Complete starter rig' : row.preset ? ' · Control starting point' : ''}{row.missing ? ' · Missing file' : row.sampleRate > 0 ? ` · ${row.sampleRate / 1000} kHz` : ''}{row.gain ? ` · ${gainLabels[row.gain]}` : ''}{row.speaker ? ` · ${row.speaker}` : ''}</small></button>
          <button className="text-button quiet" aria-label={`Favorite ${row.name}`} aria-pressed={Boolean(row.favorite || previewFavorites[row.id])} disabled={busy} onClick={() => favorite(row)}>☆</button>
          {row.missing ? <button className="text-button" disabled={busy || !native} onClick={() => action(() => invoke('relinkAsset', row.id))}>Relink</button>
            : <button className="text-button" disabled={busy || loading || row.unavailable || row.previewUnavailable || (rigDetails?.id === row.id && (Boolean(rigDetails.error) || rigDetails.assets.some(a => a.missing)))} title={row.unavailable ? `Missing: ${row.missingSounds.join(', ')}` : undefined} onClick={() => use(row)}>Use</button>}
          {row.kind === 'rig' && !row.preset && !row.starter && <button className="text-button quiet" disabled={busy} aria-label={`Remove rig ${row.name}`} onClick={() => action(async () => {
            if (native) await invoke('removeRig', row.id); else writePreviewRigs(previewRigs().filter(r => r.id !== row.id));
          })}>Remove</button>}
        </article>)}
        {!shown.length && <p className="library-empty">No matches. Import your own files or change the filters.</p>}
      </div>{detail && <aside className="library-detail"><h3>{detail.name}</h3><p>{detail.notes || detail.gear || 'Saved rig with amp, pedal, cabinet, routing, and effect settings.'}</p>
        {detail.amp && <p>Amp: {detail.amp}</p>}
        {detail.captureType && <p>Capture type: {captureLabels[detail.captureType]}. {detail.source == null && (detail.captureType === 'full' ? 'Includes a captured cabinet; Auto avoids adding another cabinet.' : detail.captureType === 'preamp' ? 'Preamp capture; this does not add a separate power-amp model.' : 'Check source notes and cabinet routing before loading.')}</p>}
        {detail.styles.length > 0 && <p>Style: {detail.styles.map(title).join(', ')}</p>}
        <p>Gain: {gainLabels[detail.gain]}{detail.speaker ? ` · Speaker: ${detail.speaker}` : ''}</p>
        {detail.pack && <p>Source pack: {detail.pack}</p>}
        {detail.kind !== 'rig' && <p className="library-note">File status checks availability only. Imported amp types come from capture metadata and filename hints; verify the source notes before choosing cabinet routing.</p>}
        {detail.inferred && <p className="library-note">Some categories are filename/tag hints. Save metadata below to correct them.</p>}
        {detail.starter && <p>{detail.assets ? 'Loads the exact captures, cabinet and complete board in this recipe.' : 'Loads a complete board with built-in sounds and no external files.'} Save your edited version as a new rig.</p>}
        {detail.assets && <p>Sounds: {Object.values(detail.assets).map(a => a.name).join(' · ')}</p>}
        {detail.unavailable && <p className="library-error">Missing sounds: {detail.missingSounds.join(', ')}. Import the matching sound packs to enable this recipe.</p>}
        {detail.creator && <p>Creator: {detail.creator}</p>}
        {detail.inputLevelDbu != null && <p>Capture input calibration: {detail.inputLevelDbu} dBu</p>}
        {detail.rights && <p>{detail.rights}</p>}
        {detail.path && <p className="library-path">{detail.path}</p>}
        {rigDetails?.id === detail.id && <div aria-label="Saved rig sounds">
          {rigDetails.error ? <p className="library-error" role="alert">{rigDetails.error}</p> : <>
            <h4>Referenced sounds</h4>
            {!rigDetails.assets.length && <p>No external sound files referenced.</p>}
            {rigDetails.assets.map(a => <p key={a.stage}><strong>{a.stage === 'model' ? 'Amp' : a.stage.startsWith('ir') ? 'Cabinet' : a.stage.startsWith('ambience') ? 'Ambience' : 'Captured pedal'}:</strong> {a.name} · {a.missing ? 'Missing' : 'Available'} {a.missing && a.canRelink && <button disabled={busy || loading} className="text-button" aria-label={`Relink ${a.name}`} onClick={() => action(() => invoke('relinkAsset',a.id))}>Relink</button>}</p>)}
            {rigDetails.assets.some(a => a.missing) && <p className="library-error">Restore the missing sounds before loading this rig. Relink the original file, or import its sound pack when no relink entry is available.</p>}
            <p className="library-note">Checks file availability. Actual format, content and processing checks happen during loading.</p>
          </>}
        </div>}
        {detail.kind === 'cab' && <button className="text-button" disabled={!native || busy || detail.missing} onClick={() => action(async () => {
          if (!await invoke('selectAsset', detail.id, 'cabB')) return 'Cabinet is missing. Relink the original file first.';
        })}>Use as cabinet B</button>}
        {detail.kind !== 'rig' && detail.ownership === 'User' && <form onSubmit={e => { e.preventDefault(); const data = new FormData(e.currentTarget);
          action(() => invoke('editAsset', detail.id, Object.fromEntries(data))); }}>
          <label>Friendly name<input name="name" defaultValue={detail.name} key={`name-${detail.id}`} /></label>
          <label>Tone / genre / gain tags<input name="tags" defaultValue={detail.tags || ''} key={`tags-${detail.id}`} /></label>
          <label>Styles (comma separated)<input name="styles" defaultValue={detail.styles.join(', ')} key={`styles-${detail.id}`} /></label>
          <label>Gain<select aria-label="Asset gain" name="gain" defaultValue={detail.gain} key={`gain-${detail.id}`}>{Object.entries(gainLabels).map(([id,label]) => <option key={id} value={id}>{label}</option>)}</select></label>
          <label>Speaker<input name="speaker" defaultValue={detail.speaker} key={`speaker-${detail.id}`} /></label>
          <label>Creator<input name="creator" defaultValue={detail.creator || ''} key={`creator-${detail.id}`} /></label>
          <label>Source URL<input name="sourceURL" defaultValue={detail.sourceURL || ''} key={`url-${detail.id}`} /></label>
          <label>Capture settings / mic / pickup notes<textarea name="notes" defaultValue={detail.notes || ''} key={`notes-${detail.id}`} /></label>
          <button className="text-button" disabled={busy}>Save metadata</button>
        </form>}
        {detail.kind === 'rig' && !detail.starter && !detail.preset && <button className="text-button quiet" disabled={busy || loading} aria-label={`Duplicate rig ${detail.name}`} onClick={() => action(async () => {
          const copyName = `${detail.name.slice(0, 75)} copy`;
          if (native) return invoke('duplicateRig', detail.id, copyName);
          const original = previewRigs().find(r => r.id === detail.id);
          if (!original) return 'Saved rig not found.';
          writePreviewRigs([...previewRigs(), {...original, id: `preview-${Date.now()}-${Math.random()}`, name: copyName, favorite: false}]);
        })}>Duplicate saved rig</button>}
        {detail.kind === 'rig' && !detail.starter && !detail.preset && <form key={detail.id} aria-label="Saved rig metadata" onSubmit={e => {
          e.preventDefault(); const changes = Object.fromEntries(new FormData(e.currentTarget));
          changes.name = changes.name.trim(); changes.styles = [...new Set(changes.styles.toLowerCase().split(/[\s,;]+/).filter(Boolean))].join(', ');
          action(async () => {
            if (!changes.name) return 'Give the rig a name.';
            if (native) return invoke('editRig', detail.id, changes);
            const original = previewRigs().find(r => r.id === detail.id);
            if (!original) return 'Saved rig not found.';
            const updated = {...original, ...changes};
            writePreviewRigs(previewRigs().map(r => r.id === detail.id ? updated : r));
            if (previewActiveId === detail.id) onPreviewRig(updated);
          });
        }}>
          <label>Saved rig name<input name="name" defaultValue={detail.name} maxLength={80} required disabled={busy || loading}/></label>
          <label>Rig tags<input name="tags" defaultValue={detail.tags || ''} maxLength={1000} disabled={busy || loading}/></label>
          <label>Rig styles (comma separated)<input name="styles" defaultValue={detail.styles.join(', ')} maxLength={1000} disabled={busy || loading}/></label>
          <label>Rig gain<select name="gain" defaultValue={detail.gain} disabled={busy || loading}>{Object.entries(gainLabels).map(([id,label]) => <option key={id} value={id}>{label}</option>)}</select></label>
          <label>Rig notes<textarea name="notes" defaultValue={detail.notes || ''} maxLength={1000} disabled={busy || loading}/></label>
          <button className="text-button" disabled={busy || loading}>Save rig metadata</button>
          <p className="library-note">Changes names and search categories only. Duplicate copies the saved tone; current unsaved edits stay in your playing rig.</p>
        </form>}
      </aside>}</div>
      <div className="library-footer">
        <button className="text-button" disabled={!native || busy} onClick={() => action(() => invoke('importAssets', 'pack'))}>Import sound ZIPs</button>
        {tab !== 'rig' ? <button className="text-button" disabled={!native || busy} onClick={() => action(() => invoke('importAssets', tab))}>Import {(tab === 'cab' || tab === 'ambience') ? 'WAV IRs' : 'NAM files'}</button>
          : <><button className="text-button" disabled={!native || busy} onClick={() => action(() => invoke('importRig'))}>Import rig</button><button className="text-button" disabled={!native || busy} onClick={() => action(() => invoke('exportRig'))}>Export current rig</button><button className="text-button" disabled={!native || busy} onClick={() => action(() => invoke('importRigPack'))}>Import pack</button><button className="text-button" disabled={!native || busy} onClick={() => action(() => invoke('exportRigPack'))}>Export pack</button></>}
        <form onSubmit={save}><input aria-label="Rig name" placeholder="Name this rig" value={name} maxLength={80} onChange={e => setName(e.target.value)} /><button className="text-button" disabled={busy || !name.trim()}>Save current rig</button></form>
      </div>
      <p className="library-note">{tab === 'rig' ? 'Complete starter rigs replace the amp, files, board and scenes. Control starting points change knobs. Input, Master and Play Along settings stay where they are. ' : ''}{native ? 'Shared library keeps managed copies. Rig JSON references files; portable packs include referenced sound files.' : 'Browser preview saves control settings only. Use Cassian for audio and complete boards.'}</p>
    </section>
  </div>;
}
