import {useEffect, useRef, useState} from 'react';
import {invoke, native} from '../juce/bridge.js';

export default function ToneRecovery({onRecovered = () => {}}) {
  const [status, setStatus] = useState({available:false,snapshots:[]}), [selected, setSelected] = useState(''), [pending, setPending] = useState(false), [error, setError] = useState(''), [summary, setSummary] = useState('');
  const request = useRef(false), refreshCallback = useRef(onRecovered); refreshCallback.current = onRecovered;
  useEffect(() => {
    if (!native) return;
    let active = true, timer;
    const poll = async () => {
      try {const result = await invoke('getToneRecovery'); if (active && result?.available !== undefined) setStatus(result);}
      catch {if (active) setError('Could not read tone recovery history.');}
      if (active) timer = setTimeout(poll, 1000);
    };
    poll(); return () => {active = false; clearTimeout(timer);};
  }, []);
  const run = async (name, ...args) => {
    if (request.current) return;
    request.current = true; setPending(true); setError(''); setSummary('');
    try {
      const result = await invoke(name, ...args); if (typeof result === 'string' && result) throw new Error(result);
      if (name === 'recoverTone') {await refreshCallback.current(); setSummary('Recovered copy added to Library Presets. Current tone preserved.');}
      else if (name === 'captureRecoveryTone') setSummary('Snapshot queued. Unchanged tones reuse the existing snapshot.');
    } catch (e) {setError(e.message || 'Could not complete tone recovery.');}
    finally {request.current = false; setPending(false);}
  };
  const disabled = !native || !status.available || status.busy || pending;
  const rows = status.snapshots || [], chosen = rows.some(row => row.id === selected) ? selected : '';
  return <details className="library-backup">
    <summary>Automatic tone recovery</summary>
    <p>Standalone keeps up to 64 changed tone snapshots, checking once a minute while recording, loading, exports and backups are idle. Recover a saved copy here, then load it from Library Presets.</p>
    <p className="library-note">Snapshots reference your existing sounds. They do not contain recordings or sound files, and cannot protect against disk failure. Use a personal backup on another drive for that. Save DAW projects separately.</p>
    {status.available ? <label><input type="checkbox" checked={Boolean(status.automatic)} disabled={disabled} onChange={e => run('setAutomaticRecovery', e.target.checked)}/> Automatic snapshots</label> : <p className="library-note">Available in the standalone Cassian app.</p>}
    <div className="backup-actions">
      <button className="text-button" disabled={disabled} onClick={() => run('captureRecoveryTone')}>Snapshot current tone</button>
      <select aria-label="Tone recovery snapshot" value={chosen} disabled={disabled || !rows.length} onChange={e => setSelected(e.target.value)}>
        <option value="">Choose a tone snapshot</option>
        {rows.map(row => <option key={row.id} value={row.id}>{row.name} · {new Date(row.created).toLocaleString()}</option>)}
      </select>
      <button className="text-button" disabled={disabled || !chosen} onClick={() => run('recoverTone', chosen)}>Add recovered preset</button>
    </div>
    {status.busy && <p role="status">Updating tone recovery history…</p>}
    {summary && <p role="status">{summary}</p>}
    {(error || status.error) && <p className="library-error" role="alert">{error || status.error}</p>}
  </details>;
}
