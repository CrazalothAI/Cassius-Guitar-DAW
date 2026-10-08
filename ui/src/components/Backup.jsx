import {useEffect, useRef, useState} from 'react';
import {invoke, native} from '../juce/bridge.js';

export default function Backup({onRestored = () => {}}) {
  const [state, setState] = useState({available:native, busy:false}), [error, setError] = useState(''), [pending, setPending] = useState(false), [confirm, setConfirm] = useState(false);
  const [includeTakes, setIncludeTakes] = useState(true);
  const completedRestore = useRef(''), callback = useRef(onRestored), request = useRef(false);
  callback.current = onRestored;
  useEffect(() => {
    if (!native) return;
    let active = true, timer;
    const poll = async () => {
      try {
        const next = await invoke('getBackupStatus');
        if (active && next?.available !== undefined) {
          setState(next);
          if (!next.busy && !next.error && next.operation === 'restore' && next.path && completedRestore.current !== next.path) {
            completedRestore.current = next.path;
            callback.current();
          }
        }
      } catch { if (active) setError('Could not read backup status.'); }
      if (active) timer = setTimeout(poll, 500);
    };
    poll(); return () => {active = false; clearTimeout(timer);};
  }, []);
  const run = async (name, ...args) => {
    if (request.current) return;
    request.current = true; setPending(true); setError('');
    try { const result = await invoke(name, ...args); if (typeof result === 'string' && result) throw new Error(result); }
    catch (e) { setError(e.message || 'Could not complete backup action.'); }
    finally {request.current = false; setPending(false);}
  };
  const disabled = !native || !state.available || state.busy || pending;
  return <details className="library-backup">
    <summary>Backup &amp; recovery</summary>
    <p>Save rigs, boards, scenes, sound files, recorded takes, reamps and review sections in one verified personal archive. Save to another drive for protection against disk failure.</p>
    <p className="library-note">Save your DAW project separately. Finish recording, exports and edits first. Backups currently support up to 2 GiB. Personal backups can contain private or licensed sounds; they are not public sound packs.</p>
    {!native && <p className="library-note">Open Cassian to back up your audio library. Browser preview has no access to its files.</p>}
    <label><input type="checkbox" checked={includeTakes} disabled={disabled} onChange={e => setIncludeTakes(e.target.checked)}/> Include recorded takes and reamps in new backups</label>
    {!includeTakes && <p className="library-note">Tone-library backup: rigs, sounds, scenes and practice sections only. Recorded audio, take metadata and take review sections are excluded. The 2 GiB limit still applies.</p>}
    <div className="backup-actions">
      <button className="text-button" disabled={disabled} onClick={() => includeTakes ? run('createBackup') : run('createBackup', false)}>Create backup</button>
      <button className="text-button" disabled={disabled} onClick={() => setConfirm(true)}>Restore backup</button>
      {state.path && !state.busy && <button className="text-button" disabled={pending} onClick={() => run('revealBackup')}>Show saved files</button>}
    </div>
    {confirm && !state.busy && <div className="backup-confirm" role="group" aria-label="Restore backup confirmation">
      <p>Restore adds recovered copies of saved rigs and takes. Existing work and the current playing tone stay intact. Matching sound files are reused; existing practice/review sections take priority.</p>
      <button className="text-button" disabled={disabled} onClick={() => {setConfirm(false); run('restoreBackup');}}>Choose backup to restore</button>
      <button className="text-button quiet" onClick={() => setConfirm(false)}>Cancel restore</button>
    </div>}
    {state.busy && <div className="backup-progress"><label>{state.operation === 'restore' ? 'Recovering library' : 'Creating and verifying backup'}<progress max="1" value={Math.max(0,Math.min(1,Number(state.progress) || 0))}/></label><button className="text-button" disabled={pending} onClick={() => run('cancelBackup')}>Cancel operation</button></div>}
    {state.summary && !state.busy && <p aria-live="polite">{state.summary}</p>}
    {(error || state.error) && <p className="library-error" role="alert">{error || state.error}</p>}
  </details>;
}
