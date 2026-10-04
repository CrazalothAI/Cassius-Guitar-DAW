import { useEffect, useState } from 'react';
import { invoke, native } from '../juce/bridge.js';

const clock = x => `${Math.floor((x || 0) / 60)}:${String(Math.floor((x || 0) % 60)).padStart(2, '0')}`;
export default function Takes({ status, onError }) {
  const [takes, setTakes] = useState([]), [selected, setSelected] = useState('');
  const [query, setQuery] = useState(''), [favorites, setFavorites] = useState(false);
  const [name, setName] = useState(''), [favorite, setFavorite] = useState(false), [version, setVersion] = useState('processed');
  const available = native && status.deviceSettingsAvailable;
  const exporting = status.takes?.exporting, recording = (status.practice?.recordMode ?? 0) > 0;
  const review = status.review ?? {}, chosen = takes.find(t => t.id === selected);
  useEffect(() => {
    let active = true;
    if (available) invoke('getTakes').then(list => {
      if (!active) return;
      setTakes(Array.isArray(list) ? list : []);
      setSelected(id => list?.some?.(t => t.id === id) ? id : list?.[0]?.id ?? '');
    }).catch(() => { if (active) onError({title: 'Takes', text: 'Couldn’t read your take library.'}); });
    return () => { active = false; };
  }, [available, status.takes?.revision]);
  useEffect(() => {
    setName(chosen?.name ?? ''); setFavorite(!!chosen?.favorite); setVersion('processed');
  }, [selected]);
  const action = async (fn, ...args) => {
    try { const error = await invoke(fn, ...args); if (typeof error === 'string' && error) onError({title: 'Takes', text: error}); }
    catch { onError({title: 'Takes', text: 'Audio engine connection interrupted. Please try again.'}); }
  };
  const filtered = takes.filter(t => (!favorites || t.favorite) && `${t.name} ${t.created}`.toLowerCase().includes(query.toLowerCase()));
  return <div className="take-browser">
    <div className="take-filters"><input aria-label="Search takes" placeholder="Search your takes…" value={query} onChange={e => setQuery(e.target.value)}/><label><input type="checkbox" checked={favorites} onChange={e => setFavorites(e.target.checked)}/> Favorites</label><button disabled={!available || recording} onClick={() => action('importTake')}>Import take folder</button></div>
    {!available && <p className="practice-note">Open standalone to record, review and reamp your guitar takes.</p>}
    <div className="take-columns"><div className="take-list" aria-label="Recorded takes">
      {filtered.map(t => <button key={t.id} className={`take-row${selected === t.id ? ' selected' : ''}`} aria-pressed={selected === t.id} onClick={() => setSelected(t.id)}><strong>{t.favorite ? '★ ' : ''}{t.name}</strong><small>{clock(t.frames / t.sampleRate)} · {(t.sampleRate / 1000).toFixed(1)} kHz{t.incomplete ? ' · Incomplete' : ''}</small></button>)}
      {!filtered.length && <p className="practice-note">{takes.length ? 'No takes match your search.' : 'Finished recordings appear here automatically. You can also import an earlier Cassian take folder.'}</p>}
    </div>{chosen && <div className="take-detail">
      <form className="take-name" onSubmit={e => { e.preventDefault(); action('editTake', chosen.id, name, favorite); }}><input aria-label="Take name" maxLength={80} value={name} onChange={e => setName(e.target.value)}/><label><input type="checkbox" aria-label="Favorite take" checked={favorite} onChange={e => setFavorite(e.target.checked)}/> ★</label><button disabled={!name.trim()}>Save</button></form>
      <div className="take-review"><select aria-label="Take version" value={version} onChange={e => setVersion(e.target.value)}><option value="processed">Original processed</option><option value="dry">Dry DI</option>{(chosen.versions ?? []).map(v => <option key={v.id} value={v.id}>{v.name}</option>)}</select><button disabled={recording} onClick={() => action('previewTake', chosen.id, version)}>Listen</button><button onClick={() => action('reviewControl', 'stop', 0)}>Stop review</button></div>
      <div className="practice-timeline"><span>{clock(review.position)}</span><input type="range" aria-label="Take review position" min={0} max={review.duration || 1} step={.01} value={Math.min(review.position || 0, review.duration || 0)} disabled={!review.duration} onChange={e => action('reviewControl', 'seek', Number(e.target.value))}/><span>{clock(review.duration)}</span></div>
      <label className="practice-volume">Review volume<input aria-label="Review volume" type="range" min={-60} max={0} step={1} value={review.level ?? -12} onChange={e => action('reviewControl', 'level', Number(e.target.value))}/><output>{review.level ?? -12} dB</output></label>
      <div className="take-export"><button disabled={exporting || recording || chosen.incomplete} onClick={() => action('reampTake', chosen.id)}>Reamp with current rig</button><button onClick={() => action('revealTake', chosen.id)}>Open take folder</button>{exporting && <><progress aria-label="Reamp progress" value={status.takes?.progress || 0} max={1}/><button onClick={() => action('cancelReamp')}>Cancel export</button></>}</div>
      {chosen.incomplete && <p className="practice-error">This recording was interrupted. Check the audio before using it.</p>}
      <p className="practice-note">Review mutes live guitar and pauses the backing track. Reamping creates a new stereo float WAV and rig snapshot, preserving the original files. {chosen.originalRig ? 'Original rig snapshot saved with this take.' : 'No original rig snapshot in this older take.'}</p>
    </div>}</div>
    {status.takes?.error && <p className="practice-error" role="alert">{status.takes.error}</p>}
  </div>;
}
