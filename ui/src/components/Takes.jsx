import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import { invoke, native } from '../juce/bridge.js';
import PracticeWaveform from './PracticeWaveform.jsx';
import PracticeSections from './PracticeSections.jsx';

const clock = x => `${Math.floor((x || 0) / 60)}:${String(Math.floor((x || 0) % 60)).padStart(2, '0')}`;
export default function Takes({ status, onError, selectionRequest }) {
  const [takes, setTakes] = useState([]), [selected, setSelected] = useState('');
  const [query, setQuery] = useState(''), [favorites, setFavorites] = useState(false);
  const [sort, setSort] = useState('newest'), [versionName, setVersionName] = useState('');
  const [notes, setNotes] = useState('');
  const [recoveryChecked, setRecoveryChecked] = useState(false);
  const [externalRecoveryReview, setExternalRecoveryReview] = useState(false);
  const [name, setName] = useState(''), [favorite, setFavorite] = useState(false), [version, setVersion] = useState('processed');
  const [includeBacking, setIncludeBacking] = useState(false), [guitarDb, setGuitarDb] = useState(0), [backingDb, setBackingDb] = useState(0);
  const [tail, setTail] = useState(2), [start, setStart] = useState(0), [end, setEnd] = useState(0), [fadeMs, setFadeMs] = useState(10);
  const [recovering, setRecovering] = useState(false), recoveryPending = useRef(false);
  const [libraryLoaded, setLibraryLoaded] = useState(false), appliedRequest = useRef(null);
  const available = native && status.deviceSettingsAvailable;
  const exporting = status.takes?.exporting, recording = (status.practice?.recordMode ?? 0) > 0;
  const review = status.review ?? {}, chosen = takes.find(t => t.id === selected);
  const reviewLoaded = available && !recording && !status.takes?.reviewLoading && !review.loading && status.takes?.reviewId === selected && status.takes?.reviewVersion === version;
  const reviewingTake = takes.find(t => t.id === status.takes?.reviewId);
  const reviewingVersion = status.takes?.reviewVersion;
  const reviewingName = reviewingVersion === 'processed' ? 'Original processed' : reviewingVersion === 'dry' ? 'Dry DI' : reviewingTake?.versions?.find(v => v.id === reviewingVersion)?.name;
  const selectedVersion = chosen?.versions?.find(v => v.id === version);
  const originalVersion = version === 'processed' || version === 'dry';
  const hasSnapshot = originalVersion ? chosen?.originalRig : Boolean(selectedVersion?.rigPath);
  const rigBusy = recovering || status.rigLoading;
  const duration = chosen ? (selectedVersion?.frames ?? chosen.frames) / chosen.sampleRate : 0;
  const rangeValid = Number.isFinite(start) && Number.isFinite(end) && start >= 0 && end > start && end <= duration + 1e-6;
  const reviewRangeValid = reviewLoaded && !review.starting && !review.counting && Number.isFinite(review.a) && Number.isFinite(review.b) && review.a >= 0 && Math.min(duration, review.b) - review.a >= .05 && review.b <= duration + .001;
  const exportReport = status.takes?.lastExportReport;
  const recoveringRecording = Boolean(status.takes?.recoveringRecording);
  useLayoutEffect(() => { setRecoveryChecked(false); setExternalRecoveryReview(false); }, [selected, chosen?.incomplete]);
  useLayoutEffect(() => { setVersionName(selectedVersion?.name || ''); }, [selected, version, selectedVersion?.name]);
  useLayoutEffect(() => { setNotes(chosen?.notes || ''); }, [selected, chosen?.notes]);
  useLayoutEffect(() => { setStart(0); setEnd(duration); }, [selected, version, duration]);
  useEffect(() => {
    let active = true;
    if (available) invoke('getTakes').then(list => {
      if (!active) return;
      setTakes(Array.isArray(list) ? list : []);
      setLibraryLoaded(true);
      setSelected(id => list?.some?.(t => t.id === id) ? id : list?.[0]?.id ?? '');
    }).catch(() => { if (active) onError({title: 'Takes', text: 'Couldn’t read your take library.'}); });
    return () => { active = false; };
  }, [available, status.takes?.revision]);
  useLayoutEffect(() => {
    setName(chosen?.name ?? ''); setFavorite(!!chosen?.favorite); setVersion('processed');
    setIncludeBacking(Boolean(chosen?.hasBacking)); setGuitarDb(0); setBackingDb(0);
    setFadeMs(10);
  }, [selected]);
  useEffect(() => {
    if (!available || recording || !libraryLoaded || !selectionRequest || appliedRequest.current === selectionRequest) return;
    const take = takes.find(row => row.id === selectionRequest.takeId);
    const found = take && (selectionRequest.version === 'processed' || selectionRequest.version === 'dry' || take.versions?.some(row => row.id === selectionRequest.version));
    if (!found) {
      appliedRequest.current = selectionRequest;
      onError({ title: 'Practice recording', text: 'That take or version is unavailable. Restore a personal backup and import its linked history, or import the recording folder and create a new link.' });
    } else if (selected !== take.id) setSelected(take.id);
    else { setVersion(selectionRequest.version); setQuery(''); setFavorites(false); appliedRequest.current = selectionRequest; }
  }, [available, recording, libraryLoaded, selectionRequest, takes, selected]);
  const action = async (fn, ...args) => {
    try { const error = await invoke(fn, ...args); if (typeof error === 'string' && error) onError({title: 'Takes', text: error}); }
    catch { onError({title: 'Takes', text: 'Audio engine connection interrupted. Please try again.'}); }
  };
  const reviewAction = (command, amount = 0) => {
    if (reviewLoaded) action('takeReviewControl', selected, version, command, amount);
  };
  const recoverRig = async () => {
    if (!available || recording || exporting || rigBusy || recoveryPending.current || !hasSnapshot || chosen?.incomplete) return;
    recoveryPending.current = true; setRecovering(true);
    try { await action('restoreTakeRig', chosen.id, version); }
    finally { recoveryPending.current = false; setRecovering(false); }
  };
  const newest = (a,b) => (Date.parse(b.created) || 0) - (Date.parse(a.created) || 0);
  const filtered = takes.filter(t => (!favorites || t.favorite) && `${t.name} ${t.created} ${t.notes || ''}`.toLowerCase().includes(query.toLowerCase()))
    .sort((a,b) => (sort === 'name' ? a.name.localeCompare(b.name) : (sort === 'favorites' ? Number(Boolean(b.favorite)) - Number(Boolean(a.favorite)) : 0) || newest(a,b)) || a.id.localeCompare(b.id));
  return <div className="take-browser">
    <details className="take-notes"><summary>Recover interrupted recording</summary>
      <p className="practice-note">Choose the original Cassian take folder. Recovery makes a separate copy of the common readable dry/processed audio, keeping your originals intact. It may omit an unusable backing stem. Damaged headers and audio after the last readable checkpoint cannot be recovered here.</p>
      <button disabled={!available || recording || exporting || status.backup?.busy} onClick={() => action('recoverRecording')}>Choose interrupted take folder</button>
    </details>
    {recoveringRecording && <div className="take-export" role="status" aria-label="Recording recovery progress"><span>Preparing recording recovery…</span><progress aria-label="Recording recovery progress" value={status.takes?.progress || 0} max={1}/><button onClick={() => action('cancelReamp')}>Cancel recovery</button></div>}
    {status.takes?.lastRecoverySummary && <div className="practice-note" role="status" aria-label="Recording recovery result"><p>{status.takes.lastRecoverySummary}</p>{status.takes.lastRecoveryId && <button onClick={() => setSelected(status.takes.lastRecoveryId)}>Show recovered take</button>}{status.takes.lastRecoveryPath && <button disabled={!available || recoveringRecording} onClick={() => action('revealRecordingRecovery')}>Open recovered files</button>}</div>}
    <div className="take-filters"><input aria-label="Search takes" placeholder="Search your takes…" value={query} onChange={e => setQuery(e.target.value)}/><label><input type="checkbox" checked={favorites} onChange={e => setFavorites(e.target.checked)}/> Favorites</label><select aria-label="Take sort" value={sort} onChange={e => setSort(e.target.value)}><option value="newest">Newest first</option><option value="name">Name</option><option value="favorites">Favorites first</option></select><button disabled={!available || recording} onClick={() => action('importTake')}>Import take folder</button></div>
    {!available && <p className="practice-note">Open standalone to record, review and reamp your guitar takes.</p>}
    <div className="take-columns"><div className="take-list" aria-label="Recorded takes">
      {filtered.map(t => <button key={t.id} className={`take-row${selected === t.id ? ' selected' : ''}`} aria-pressed={selected === t.id} onClick={() => setSelected(t.id)}><strong>{t.favorite ? '★ ' : ''}{t.name}</strong><small>{clock(t.frames / t.sampleRate)} · {(t.sampleRate / 1000).toFixed(1)} kHz{t.incomplete ? ' · Incomplete' : ''}</small></button>)}
      {!filtered.length && <p className="practice-note">{takes.length ? 'No takes match your search.' : 'Finished recordings appear here automatically. You can also import an earlier Cassian take folder.'}</p>}
    </div>{chosen && <div className="take-detail">
      <details className="take-notes"><summary>Take notes{chosen.notes ? ' · Saved' : ''}</summary><form onSubmit={e => { e.preventDefault(); action('saveTakeNotes', chosen.id, notes); }}><label>Notes<textarea aria-label="Take notes" value={notes} maxLength={2000} rows={3} disabled={!available} onChange={e => setNotes(e.target.value)}/></label><button disabled={!available || notes === (chosen.notes || '')}>Save notes</button><p className="practice-note">Track tuning, tempo, song and what to improve. Notes are searchable and saved in your take library; the recordings stay intact.</p></form></details>
      <form className="take-name" onSubmit={e => { e.preventDefault(); action('editTake', chosen.id, name, favorite); }}><input aria-label="Take name" maxLength={80} value={name} onChange={e => setName(e.target.value)}/><label><input type="checkbox" aria-label="Favorite take" checked={favorite} onChange={e => setFavorite(e.target.checked)}/> ★</label><button disabled={!name.trim()}>Save</button></form>
      <div className="take-review"><select aria-label="Take version" value={version} onChange={e => setVersion(e.target.value)}><option value="processed">Original processed</option><option value="dry">Dry DI</option>{(chosen.versions ?? []).map(v => <option key={v.id} value={v.id}>{v.name}</option>)}</select><button disabled={!available || recording || status.takes?.reviewLoading} onClick={() => action('previewTake', chosen.id, version)}>Listen</button><button disabled={!reviewLoaded} onClick={() => reviewAction(review.playing ? 'pause' : 'play')}>{review.playing ? 'Pause review' : 'Resume review'}</button><button onClick={() => action('reviewControl', 'stop', 0)}>Stop review</button></div>
      {selectedVersion && <form className="take-name" onSubmit={e => { e.preventDefault(); if (versionName.trim()) action('renameTakeVersion',chosen.id,version,versionName.trim()); }}><input aria-label="Reamp version name" value={versionName} maxLength={80} disabled={recording || exporting} onChange={e => setVersionName(e.target.value)}/><button disabled={recording || exporting || !versionName.trim()}>Rename version</button></form>}
      <div className="take-export"><button disabled={!available || recording || exporting || rigBusy || chosen.incomplete || !hasSnapshot} onClick={recoverRig}>{recovering ? 'Loading saved rig…' : originalVersion ? 'Load recorded rig' : 'Load reamp rig'}</button><span className="practice-note">{hasSnapshot ? 'Loads this version’s tone into your current rig. Input calibration and listening levels stay as they are. Save your current edits first.' : 'This version has no saved rig snapshot.'}</span></div>
      <p className="practice-note" role="status" aria-label="Take review status">{status.takes?.reviewLoading ? `Preparing take review… ${Math.round((review.loadProgress || 0) * 100)}%` : reviewingTake ? `${review.buffering ? 'Buffering' : review.playing ? 'Playing' : 'Loaded'}: ${reviewingTake.name} · ${reviewingName || 'Reamp'}` : 'Select a version and press Listen to audition it.'}{reviewLoaded && review.streaming ? ' · Streaming WAV at normal speed.' : ''}{reviewLoaded && review.buffering ? ' Playback waits here until audio is ready.' : ''}{reviewingTake && !reviewLoaded && !recording && !status.takes?.reviewLoading ? ' Press Listen to load your selected version.' : ''}</p>
      {review.error && <p className="practice-error" role="alert">{review.error}</p>}
      {reviewLoaded && review.streaming && review.loop && review.loopPrefetchReady === false && <p className="practice-note" role="status">Preparing loop start… Playback will wait at the boundary if more audio is needed.</p>}
      <div className="practice-timeline"><span>{clock(reviewLoaded ? review.position : 0)}</span><input type="range" aria-label="Take review position" min={0} max={reviewLoaded ? review.duration || 1 : 1} step={.01} value={reviewLoaded ? Math.min(review.position || 0, review.duration || 0) : 0} disabled={!reviewLoaded || !review.duration} onChange={e => reviewAction('seek', Number(e.target.value))}/><span>{clock(reviewLoaded ? review.duration : duration)}</span></div>
      <div className="take-export"><button disabled={!reviewLoaded} onClick={() => reviewAction('a', review.position || 0)}>Set review A here</button><button disabled={!reviewLoaded} onClick={() => reviewAction('b', review.position || 0)}>Set review B here</button><label><input type="checkbox" aria-label="Loop take review" checked={reviewLoaded && Boolean(review.loop)} disabled={!reviewLoaded || review.loopAvailable === false || (review.b || 0) - (review.a || 0) < .05} onChange={e => reviewAction('loop', Number(e.target.checked))}/> Loop A–B</label><button disabled={!reviewLoaded} onClick={() => reviewAction('seek', review.a || 0)}>Go to A</button></div>
      <p className="practice-note">{reviewLoaded ? `A ${(review.a || 0).toFixed(2)}s · B ${(review.b || 0).toFixed(2)}s. ` : ''}{reviewLoaded && review.loopAvailable === false ? 'Long-take looping is not available yet. A–B ranges and saved sections still work for seeking and exports.' : 'Review loops use 5 ms edge fades and affect listening only. Set B at least 0.05 seconds after A; Listen reloads the full version and resets its loop.'}</p>
      {reviewLoaded && review.reviewUnderruns > 0 && <p className="practice-note">Playback buffer waits: {review.reviewUnderruns} (including uncached seeks). If waits continue during normal playback, copy the take to a local drive and close disk-heavy apps.</p>}
      <PracticeSections key={`${selected}:${version}:${reviewLoaded ? review.waveRevision : 'unloaded'}`} p={reviewLoaded ? review : {}} available={reviewLoaded} disabled={!reviewLoaded} takeId={selected} takeVersion={version} onError={onError}/>
      <label className="practice-volume">Review volume<input aria-label="Review volume" type="range" min={-60} max={0} step={1} value={review.level ?? -12} onChange={e => action('reviewControl', 'level', Number(e.target.value))}/><output>{review.level ?? -12} dB</output></label>
      <PracticeWaveform p={reviewLoaded ? review : {}} available={reviewLoaded} disabled={!reviewLoaded} takeId={selected} takeVersion={version} label="Take waveform position" emptyText="Listen to this version to see its waveform" onSeek={seconds => reviewAction('seek', seconds)} onError={onError}/>
      <div className="take-export"><label>Effect tail <select aria-label="Reamp effect tail" value={tail} disabled={exporting || recording || rigBusy} onChange={e => setTail(Number(e.target.value))}>{[0, 1, 2, 5, 10].map(seconds => <option key={seconds} value={seconds}>{seconds ? `${seconds} seconds` : 'No extra tail'}</option>)}</select></label><button disabled={exporting || recording || rigBusy || chosen.incomplete} onClick={() => action('reampTake', chosen.id, tail)}>Reamp with current rig</button><button onClick={() => action('revealTake', chosen.id)}>Open take folder</button>{exporting && <><progress aria-label="Audio export progress" value={status.takes?.progress || 0} max={1}/><button onClick={() => action('cancelReamp')}>Cancel export</button></>}</div>
      <div className="take-export"><button disabled={!available || exporting || recording || rigBusy || chosen.incomplete || !(duration > 0)} onClick={() => action('exportVideoAudio',chosen.id,version,false,0,0,0,duration,.01)}>Export guitar WAV</button><span className="practice-note">Full selected version, guitar only · 48 kHz / 24-bit stereo · 10 ms edge fades. Uses peak protection; no backing or clicks.</span></div>
      <details className="video-export"><summary>Video soundtrack</summary>
        <p className="practice-note">Export the selected take version as a 48 kHz / 24-bit stereo WAV for your video editor. The export keeps the balance below and reduces peaks only when needed for −1 dBFS headroom.</p>
        <button disabled={!reviewRangeValid || exporting || recording} onClick={() => { setStart(review.a); setEnd(Math.min(duration, review.b)); }}>Use review A–B</button>
        <p className="practice-note">Recall a saved section or set review A–B, then copy that range here. Later loop edits won’t change the export selection.</p>
        <div className="take-range"><label>Start (seconds)<input aria-label="Export start seconds" type="number" min={0} max={duration} step="any" value={start} onChange={e => setStart(e.target.value === '' ? '' : Number(e.target.value))}/></label><label>End (seconds)<input aria-label="Export end seconds" type="number" min={0} max={duration} step="any" value={end} onChange={e => setEnd(e.target.value === '' ? '' : Number(e.target.value))}/></label><button onClick={() => { setStart(0); setEnd(duration); }}>Full take</button></div>
        <label className="practice-volume">Edge fades<input aria-label="Export fade milliseconds" type="range" min={0} max={100} step={1} value={fadeMs} onChange={e => setFadeMs(Number(e.target.value))}/><output>{fadeMs} ms</output></label>
        <p className={rangeValid ? 'practice-note' : 'practice-error'}>{rangeValid ? `${(end - start).toFixed(2)} seconds selected of ${duration.toFixed(2)} seconds. Trimming and fades affect only this export.` : 'Choose an end after the start, within the selected version.'}</p>
        <label className="practice-volume">Guitar balance<input type="range" aria-label="Video guitar balance" min={-60} max={12} step={1} value={guitarDb} onChange={e => setGuitarDb(Number(e.target.value))}/><output>{guitarDb} dB</output></label>
        <label><input type="checkbox" aria-label="Include recorded backing" checked={includeBacking && Boolean(chosen.hasBacking)} disabled={!chosen.hasBacking} onChange={e => setIncludeBacking(e.target.checked)}/> Include recorded backing</label>
        {includeBacking && chosen.hasBacking && <label className="practice-volume">Backing balance<input type="range" aria-label="Video backing balance" min={-60} max={12} step={1} value={backingDb} onChange={e => setBackingDb(Number(e.target.value))}/><output>{backingDb} dB</output></label>}
        <p className="practice-note">New takes include a synchronized backing stem from tracks loaded in Practice. Audio playing in a browser is not recorded. The count-in and metronome stay out of the soundtrack.</p>
        <div className="take-export"><button disabled={!available || exporting || recording || rigBusy || chosen.incomplete || !rangeValid} onClick={() => action('exportVideoAudio', chosen.id, version, includeBacking && Boolean(chosen.hasBacking), guitarDb, backingDb, start, end, fadeMs / 1000)}>Export for video</button><button disabled={!available || exporting || recording || rigBusy || chosen.incomplete || !rangeValid} onClick={() => action('exportVideoAudio', chosen.id, version, false, 0, 0, start, end, fadeMs / 1000)}>Export range as guitar WAV</button></div>
        <p className="practice-note">Guitar WAV uses this range and fade with neutral export gain, excluding backing. Export for video uses the balance above. Both preserve original recordings.</p>
      </details>
      {status.takes?.lastExportPath && <div className="practice-note" role="status"><p>Last export saved{exportReport ? `: ${exportReport.source} · ${Number(exportReport.duration).toFixed(2)} seconds · ${exportReport.backing ? 'guitar + backing' : 'guitar only'} · 48 kHz / 24-bit stereo` : '.'}</p>{exportReport && <p>{exportReport.attenuationDb > 0 ? `Peak protection reduced this export by ${Number(exportReport.attenuationDb).toFixed(2)} dB.` : 'No peak attenuation was needed.'} Original audio and live output settings are unchanged.</p>}<button onClick={() => action('revealVideoExport')}>Show exported audio</button></div>}
      {chosen.incomplete && <p className="practice-error">{chosen.recovered ? 'This recovered copy needs your review before export or reamping.' : 'This recording was interrupted. Use Recover interrupted recording to create a reviewable copy.'}</p>}
      {chosen.recovered && chosen.incomplete && <div className="take-notes"><label><input type="checkbox" aria-label="I listened to the recovered processed audio" checked={recoveryChecked} disabled={!available || recording || exporting} onChange={e => setRecoveryChecked(e.target.checked)}/> I listened to the recovered processed audio and want to use this copy.</label><button disabled={!available || recording || exporting || !recoveryChecked || (!externalRecoveryReview && (status.takes?.reviewLoading || status.takes?.reviewId !== selected || status.takes?.reviewVersion !== 'processed'))} onClick={() => action('confirmTakeRecovery',chosen.id,true,externalRecoveryReview)}>Confirm recovered take</button><p className="practice-note">Press Listen on Original processed first. Confirmation verifies all recovered audio again and enables exports/reamping. Your interrupted original stays intact.</p><details><summary>Review in another player</summary><p className="practice-note">If this file or device rate is unsupported, use Open take folder and listen to Guitar processed.wav in another audio player.</p><label><input type="checkbox" aria-label="I reviewed Guitar processed.wav in another player" checked={externalRecoveryReview} disabled={!available || recording || exporting} onChange={e => setExternalRecoveryReview(e.target.checked)}/> I reviewed Guitar processed.wav in another player.</label></details></div>}
      <p className="practice-note">Review mutes live guitar and pauses the backing track. Reamping creates a new stereo float WAV and rig snapshot, preserving the original files. {chosen.originalRig ? 'Original rig snapshot saved with this take.' : 'No original rig snapshot in this older take.'}</p>
    </div>}</div>
    {status.takes?.error && <p className="practice-error" role="alert">{status.takes.error}</p>}
  </div>;
}
