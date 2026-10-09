import { useState } from 'react';

export default function PracticeRecordingLinks({ session, takes, blocked, recording, onAction, onOpenTake }) {
  const [takeId, setTakeId] = useState(''), [version, setVersion] = useState('processed');
  const chosen = takes.find(take => take.id === takeId), links = session.recordings || [];
  const versionName = (take, id) => id === 'processed' ? 'Original processed' : id === 'dry' ? 'Dry DI' : take?.versions?.find(row => row.id === id)?.name;
  return <details className="journal-recordings"><summary>Recordings · {links.length} / 8</summary>
    <ul>{links.map(link => {
      const take = takes.find(row => row.id === link.takeId), name = versionName(take, link.version), found = take && name;
      return <li key={`${link.takeId}:${link.version}`}><span>{found ? `${take.name} · ${name}` : `Unavailable recording · ${link.takeId} · ${link.version}`}{take?.incomplete ? ' · Incomplete' : ''}</span><button disabled={!found || recording || !onOpenTake} onClick={() => onOpenTake(link)}>Open in Takes</button><button disabled={blocked} onClick={() => onAction('removeRecording', { sessionId: session.id, ...link })}>Unlink recording</button></li>;
    })}</ul>
    <div className="journal-link-form"><label>Take<select aria-label={`Recording for ${session.title} (${session.id})`} value={takeId} disabled={blocked || recording} onChange={e => { setTakeId(e.target.value); setVersion('processed'); }}><option value="">Choose a recording</option>{takes.map(take => <option key={take.id} value={take.id}>{take.name}{take.incomplete ? ' · Incomplete' : ''}</option>)}</select></label><label>Version<select aria-label={`Recording version for ${session.title} (${session.id})`} value={version} disabled={blocked || recording || !chosen} onChange={e => setVersion(e.target.value)}><option value="processed">Original processed</option><option value="dry">Dry DI</option>{chosen?.versions?.map(row => <option key={row.id} value={row.id}>{row.name}</option>)}</select></label><button disabled={blocked || recording || !chosen || !versionName(chosen, version) || links.length >= 8 || links.some(link => link.takeId === takeId && link.version === version)} onClick={() => onAction('addRecording', { sessionId: session.id, takeId, version })}>Link recording</button></div>
    <p className="practice-note">Links open the selected version without starting playback. Unlinking keeps the audio. History JSON carries identities only. Personal recovery provides a linked-history copy for its recovered takes; separately imported folders need new links.</p>
  </details>;
}
