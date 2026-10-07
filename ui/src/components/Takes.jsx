import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import { invoke, native } from '../juce/bridge.js';

const clock = x => `${Math.floor((x || 0) / 60)}:${String(Math.floor((x || 0) % 60)).padStart(2, '0')}`;
export default function Takes({ status, onError }) {
  const [takes, setTakes] = useState([]), [selected, setSelected] = useState('');
  const [query, setQuery] = useState(''), [favorites, setFavorites] = useState(false);
  const [sort, setSort] = useState('newest'), [versionName, setVersionName] = useState('');
  const [name, setName] = useState(''), [favorite, setFavorite] = useState(false), [version, setVersion] = useState('processed');
  const [includeBacking, setIncludeBacking] = useState(false), [guitarDb, setGuitarDb] = useState(0), [backingDb, setBackingDb] = useState(0);
  const [tail, setTail] = useState(2), [start, setStart] = useState(0), [end, setEnd] = useState(0), [fadeMs, setFadeMs] = useState(10);
  const [recovering, setRecovering] = useState(false), recoveryPending = useRef(false);
  const available = native && status.deviceSettingsAvailable;
  const exporting = status.takes?.exporting, recording = (status.practice?.recordMode ?? 0) > 0;
  const review = status.review ?? {}, chosen = takes.find(t => t.id === selected);
  const selectedVersion = chosen?.versions?.find(v => v.id === version);
  const originalVersion = version === 'processed' || version === 'dry';
  const hasSnapshot = originalVersion ? chosen?.originalRig : Boolean(selectedVersion?.rigPath);
  const rigBusy = recovering || status.rigLoading;
  const duration = chosen ? (selectedVersion?.frames ?? chosen.frames) / chosen.sampleRate : 0;
  const rangeValid = Number.isFinite(start) && Number.isFinite(end) && start >= 0 && end > start && end <= duration + 1e-6;
  useLayoutEffect(() => { setVersionName(selectedVersion?.name || ''); }, [selected, version, selectedVersion?.name]);
  useLayoutEffect(() => { setStart(0); setEnd(duration); }, [selected, version, duration]);
  useEffect(() => {
    let active = true;
    if (available) invoke('getTakes').then(list => {
      if (!active) return;
      setTakes(Array.isArray(list) ? list : []);
      setSelected(id => list?.some?.(t => t.id === id) ? id : list?.[0]?.id ?? '');
    }).catch(() => { if (active) onError({title: 'Takes', text: 'Couldn’t read your take library.'}); });
    return () => { active = false; };
  }, [available, status.takes?.revision]);
  useLayoutEffect(() => {
    setName(chosen?.name ?? ''); setFavorite(!!chosen?.favorite); setVersion('processed');
    setIncludeBacking(Boolean(chosen?.hasBacking)); setGuitarDb(0); setBackingDb(0);
    setFadeMs(10);
  }, [selected]);
  const action = async (fn, ...args) => {
    try { const error = await invoke(fn, ...args); if (typeof error === 'string' && error) onError({title: 'Takes', text: error}); }
    catch { onError({title: 'Takes', text: 'Audio engine connection interrupted. Please try again.'}); }
  };
  const recoverRig = async () => {
    if (!available || recording || exporting || rigBusy || recoveryPending.current || !hasSnapshot || chosen?.incomplete) return;
    recoveryPending.current = true; setRecovering(true);
    try { await action('restoreTakeRig', chosen.id, version); }
    finally { recoveryPending.current = false; setRecovering(false); }
  };
  const newest = (a,b) => (Date.parse(b.created) || 0) - (Date.parse(a.created) || 0);
  const filtered = takes.filter(t => (!favorites || t.favorite) && `${t.name} ${t.created}`.toLowerCase().includes(query.toLowerCase()))
    .sort((a,b) => (sort === 'name' ? a.name.localeCompare(b.name) : (sort === 'favorites' ? Number(Boolean(b.favorite)) - Number(Boolean(a.favorite)) : 0) || newest(a,b)) || a.id.localeCompare(b.id));
  return <div className="take-browser">
    <div className="take-filters"><input aria-label="Search takes" placeholder="Search your takes…" value={query} onChange={e => setQuery(e.target.value)}/><label><input type="checkbox" checked={favorites} onChange={e => setFavorites(e.target.checked)}/> Favorites</label><select aria-label="Take sort" value={sort} onChange={e => setSort(e.target.value)}><option value="newest">Newest first</option><option value="name">Name</option><option value="favorites">Favorites first</option></select><button disabled={!available || recording} onClick={() => action('importTake')}>Import take folder</button></div>
    {!available && <p className="practice-note">Open standalone to record, review and reamp your guitar takes.</p>}
    <div className="take-columns"><div className="take-list" aria-label="Recorded takes">
      {filtered.map(t => <button key={t.id} className={`take-row${selected === t.id ? ' selected' : ''}`} aria-pressed={selected === t.id} onClick={() => setSelected(t.id)}><strong>{t.favorite ? '★ ' : ''}{t.name}</strong><small>{clock(t.frames / t.sampleRate)} · {(t.sampleRate / 1000).toFixed(1)} kHz{t.incomplete ? ' · Incomplete' : ''}</small></button>)}
      {!filtered.length && <p className="practice-note">{takes.length ? 'No takes match your search.' : 'Finished recordings appear here automatically. You can also import an earlier Cassian take folder.'}</p>}
    </div>{chosen && <div className="take-detail">
      <form className="take-name" onSubmit={e => { e.preventDefault(); action('editTake', chosen.id, name, favorite); }}><input aria-label="Take name" maxLength={80} value={name} onChange={e => setName(e.target.value)}/><label><input type="checkbox" aria-label="Favorite take" checked={favorite} onChange={e => setFavorite(e.target.checked)}/> ★</label><button disabled={!name.trim()}>Save</button></form>
      <div className="take-review"><select aria-label="Take version" value={version} onChange={e => setVersion(e.target.value)}><option value="processed">Original processed</option><option value="dry">Dry DI</option>{(chosen.versions ?? []).map(v => <option key={v.id} value={v.id}>{v.name}</option>)}</select><button disabled={recording} onClick={() => action('previewTake', chosen.id, version)}>Listen</button><button onClick={() => action('reviewControl', 'stop', 0)}>Stop review</button></div>
      {selectedVersion && <form className="take-name" onSubmit={e => { e.preventDefault(); if (versionName.trim()) action('renameTakeVersion',chosen.id,version,versionName.trim()); }}><input aria-label="Reamp version name" value={versionName} maxLength={80} disabled={recording || exporting} onChange={e => setVersionName(e.target.value)}/><button disabled={recording || exporting || !versionName.trim()}>Rename version</button></form>}
      <div className="take-export"><button disabled={!available || recording || exporting || rigBusy || chosen.incomplete || !hasSnapshot} onClick={recoverRig}>{recovering ? 'Loading saved rig…' : originalVersion ? 'Load recorded rig' : 'Load reamp rig'}</button><span className="practice-note">{hasSnapshot ? 'Loads this version’s tone into your current rig. Input calibration and listening levels stay as they are. Save your current edits first.' : 'This version has no saved rig snapshot.'}</span></div>
      <div className="practice-timeline"><span>{clock(review.position)}</span><input type="range" aria-label="Take review position" min={0} max={review.duration || 1} step={.01} value={Math.min(review.position || 0, review.duration || 0)} disabled={!review.duration} onChange={e => action('reviewControl', 'seek', Number(e.target.value))}/><span>{clock(review.duration)}</span></div>
      <label className="practice-volume">Review volume<input aria-label="Review volume" type="range" min={-60} max={0} step={1} value={review.level ?? -12} onChange={e => action('reviewControl', 'level', Number(e.target.value))}/><output>{review.level ?? -12} dB</output></label>
      <div className="take-export"><label>Effect tail <select aria-label="Reamp effect tail" value={tail} disabled={exporting || recording || rigBusy} onChange={e => setTail(Number(e.target.value))}>{[0, 1, 2, 5, 10].map(seconds => <option key={seconds} value={seconds}>{seconds ? `${seconds} seconds` : 'No extra tail'}</option>)}</select></label><button disabled={exporting || recording || rigBusy || chosen.incomplete} onClick={() => action('reampTake', chosen.id, tail)}>Reamp with current rig</button><button onClick={() => action('revealTake', chosen.id)}>Open take folder</button>{exporting && <><progress aria-label="Audio export progress" value={status.takes?.progress || 0} max={1}/><button onClick={() => action('cancelReamp')}>Cancel export</button></>}</div>
      <div className="take-export"><button disabled={!available || exporting || recording || rigBusy || chosen.incomplete || !(duration > 0)} onClick={() => action('exportVideoAudio',chosen.id,version,false,0,0,0,duration,.01)}>Export guitar WAV</button><span className="practice-note">Full selected version, guitar only · 48 kHz / 24-bit stereo · 10 ms edge fades. Uses peak protection; no backing or clicks.</span></div>
      <details className="video-export"><summary>Video soundtrack</summary>
        <p className="practice-note">Export the selected take version as a 48 kHz / 24-bit stereo WAV for your video editor. The export keeps the balance below and reduces peaks only when needed for −1 dBFS headroom.</p>
        <div className="take-range"><label>Start (seconds)<input aria-label="Export start seconds" type="number" min={0} max={duration} step="any" value={start} onChange={e => setStart(e.target.value === '' ? '' : Number(e.target.value))}/></label><label>End (seconds)<input aria-label="Export end seconds" type="number" min={0} max={duration} step="any" value={end} onChange={e => setEnd(e.target.value === '' ? '' : Number(e.target.value))}/></label><button onClick={() => { setStart(0); setEnd(duration); }}>Full take</button></div>
        <label className="practice-volume">Edge fades<input aria-label="Export fade milliseconds" type="range" min={0} max={100} step={1} value={fadeMs} onChange={e => setFadeMs(Number(e.target.value))}/><output>{fadeMs} ms</output></label>
        <p className={rangeValid ? 'practice-note' : 'practice-error'}>{rangeValid ? `${(end - start).toFixed(2)} seconds selected of ${duration.toFixed(2)} seconds. Trimming and fades affect only this export.` : 'Choose an end after the start, within the selected version.'}</p>
        <label className="practice-volume">Guitar balance<input type="range" aria-label="Video guitar balance" min={-60} max={12} step={1} value={guitarDb} onChange={e => setGuitarDb(Number(e.target.value))}/><output>{guitarDb} dB</output></label>
        <label><input type="checkbox" aria-label="Include recorded backing" checked={includeBacking && Boolean(chosen.hasBacking)} disabled={!chosen.hasBacking} onChange={e => setIncludeBacking(e.target.checked)}/> Include recorded backing</label>
        {includeBacking && chosen.hasBacking && <label className="practice-volume">Backing balance<input type="range" aria-label="Video backing balance" min={-60} max={12} step={1} value={backingDb} onChange={e => setBackingDb(Number(e.target.value))}/><output>{backingDb} dB</output></label>}
        <p className="practice-note">New takes include a synchronized backing stem from tracks loaded in Practice. Audio playing in a browser is not recorded. The count-in and metronome stay out of the soundtrack.</p>
        <button disabled={exporting || recording || rigBusy || chosen.incomplete || !rangeValid} onClick={() => action('exportVideoAudio', chosen.id, version, includeBacking && Boolean(chosen.hasBacking), guitarDb, backingDb, start, end, fadeMs / 1000)}>Export for video</button>
      </details>
      {status.takes?.lastExportPath && <p className="practice-note">Video audio saved. <button onClick={() => action('revealVideoExport')}>Show exported audio</button></p>}
      {chosen.incomplete && <p className="practice-error">This recording was interrupted. Check the audio before using it.</p>}
      <p className="practice-note">Review mutes live guitar and pauses the backing track. Reamping creates a new stereo float WAV and rig snapshot, preserving the original files. {chosen.originalRig ? 'Original rig snapshot saved with this take.' : 'No original rig snapshot in this older take.'}</p>
    </div>}</div>
    {status.takes?.error && <p className="practice-error" role="alert">{status.takes.error}</p>}
  </div>;
}
