import { useEffect, useState } from 'react';
import { invoke } from '../juce/bridge.js';
import { time } from '../practiceTime.js';
import { setParameter } from '../parameterState.js';

const newTask = () => ({ title: 'Clean dynamics', minutes: 10, bpm: 80 });
const date = value => { const d = new Date(value); return Number.isNaN(d.getTime()) ? value : d.toLocaleString(); };

export default function PracticeJournal({ journal: j, recording, available, onError }) {
  const [doc, setDoc] = useState({ sets: [], sessions: [] }), [open, setOpen] = useState(false);
  const [selected, setSelected] = useState(''), [taskIndex, setTaskIndex] = useState(0), [name, setName] = useState('My practice set');
  const [tasks, setTasks] = useState([newTask()]), [notes, setNotes] = useState(''), [filter, setFilter] = useState(''), [pending, setPending] = useState(false), [confirm, setConfirm] = useState('');
  const [awaitingSet, setAwaitingSet] = useState(null);
  useEffect(() => {
    let current = true;
    if (available && j?.writable) invoke('getPracticeJournal').then(value => { if (current && value?.sets && value?.sessions) setDoc(value); }).catch(() => { if (current) onError({ title: 'Practice history', text: 'Could not load practice history. Reopen Practice to retry.' }); });
    return () => { current = false; };
  }, [available, j?.revision, j?.writable]);
  useEffect(() => { setNotes(''); }, [j?.active?.id]);
  useEffect(() => {
    if (awaitingSet) {
      const saved = doc.sets.find(row => !awaitingSet.existingIds.includes(row.id) && row.name === awaitingSet.name && JSON.stringify(row.tasks) === JSON.stringify(awaitingSet.tasks));
      if (saved) { setSelected(saved.id); setAwaitingSet(null); }
    }
    if (selected && !doc.sets.some(row => row.id === selected)) { setSelected(''); setName('My practice set'); setTasks([newTask()]); setTaskIndex(0); }
  }, [doc, awaitingSet]);
  if (!j?.available) return null;
  const blocked = !available || !j.writable || j.busy || pending, active = j.active;
  const task = tasks[taskIndex] || tasks[0], valid = name.trim() && tasks.length && tasks.every(t => t.title.trim() && Number.isInteger(t.minutes) && t.minutes >= 1 && t.minutes <= 120 && Number.isInteger(t.bpm) && t.bpm >= 40 && t.bpm <= 240);
  const action = async (command, args) => {
    if (blocked) return; setPending(true); setConfirm('');
    try { const failure = await invoke('practiceJournalCommand', command, args); if (failure) throw new Error(String(failure)); if (command === 'saveSet' && !args.id) setAwaitingSet({ ...args, existingIds: doc.sets.map(row => row.id) }); }
    catch (e) { onError({ title: 'Practice history', text: e.message || 'Could not update practice history.' }); }
    finally { setPending(false); }
  };
  const transfer = async save => {
    if (blocked || recording) return; setPending(true);
    try { const failure = await invoke('transferPracticeJournal', save); if (failure) throw new Error(String(failure)); }
    catch (e) { onError({ title: 'Practice history', text: e.message || 'Could not transfer practice history.' }); }
    finally { setPending(false); }
  };
  const choose = id => {
    setSelected(id); setTaskIndex(0); setConfirm('');
    const set = doc.sets.find(row => row.id === id); setName(set?.name || 'My practice set'); setTasks(set ? set.tasks.map(row => ({ ...row })) : [newTask()]);
  };
  const updateTask = (i, key, value) => setTasks(rows => rows.map((row, index) => index === i ? { ...row, [key]: value } : row));
  const remove = (kind, id) => { const key = `${kind}:${id}`; if (confirm === key) action(kind, id); else setConfirm(key); };
  const rows = doc.sessions.filter(row => `${row.title} ${row.setName} ${row.notes}`.toLowerCase().includes(filter.toLowerCase()));
  const completedSeconds = doc.sessions.filter(row => row.state === 'finished').reduce((sum, row) => sum + row.seconds, 0);
  return <section className="practice-journal" aria-label="Practice sets and history">
    {active && <div className="journal-timer" role="status">
      <div><strong>{active.title}</strong><span>{time(active.seconds)} / {active.minutes} min · {active.bpm} BPM target · {active.state === 'paused' ? 'Paused' : 'Timer running'}{active.seconds >= active.minutes * 60 ? ' · Time target reached' : ''}</span></div>
      <button disabled={blocked} onClick={() => action(active.state === 'paused' ? 'resume' : 'pause', null)}>{active.state === 'paused' ? 'Resume timer' : 'Pause timer'}</button>
      <label>Session notes<textarea aria-label="Session notes" rows={2} value={notes} maxLength={1000} onChange={e => setNotes(e.target.value)}/></label>
      <button disabled={blocked} onClick={() => action('finish', notes)}>Finish session</button>
    </div>}
    <button type="button" className="text-button" aria-expanded={open} onClick={() => setOpen(!open)}>Practice sets &amp; history · {j.setCount || 0} sets · {j.sessionCount || 0} sessions</button>
    {open && <>
      <p className="practice-note">Plan a clean, rhythm or lead exercise and keep your progress locally. The timer measures elapsed practice time until paused; it does not detect played notes. Starting it leaves your tone, backing transport and metronome unchanged.</p>
      <form onSubmit={e => { e.preventDefault(); action('saveSet', { id: selected, name: name.trim(), tasks }); }}>
        <div className="journal-set-header">
          <label>Practice set<select aria-label="Practice set" value={selected} disabled={blocked} onChange={e => choose(e.target.value)}><option value="">New practice set</option>{doc.sets.map(row => <option key={row.id} value={row.id}>{row.name}</option>)}</select></label>
          <label>Set name<input aria-label="Practice set name" value={name} maxLength={48} disabled={blocked} onChange={e => setName(e.target.value)}/></label>
          <button disabled={blocked || !valid || (!selected && doc.sets.length >= 32)}>{selected ? 'Save set changes' : 'Save practice set'}</button>
          <button type="button" disabled={blocked || !selected} onClick={() => remove('removeSet', selected)}>{confirm === `removeSet:${selected}` ? 'Confirm delete set' : 'Delete set'}</button>
        </div>
        <ol className="journal-tasks">{tasks.map((row, i) => <li key={i}>
          <label>Exercise<input aria-label={`Exercise ${i + 1}`} value={row.title} maxLength={80} disabled={blocked} onChange={e => updateTask(i, 'title', e.target.value)}/></label>
          <label>Minutes<input aria-label={`Minutes ${i + 1}`} type="number" min={1} max={120} value={row.minutes} disabled={blocked} onChange={e => updateTask(i, 'minutes', Number(e.target.value))}/></label>
          <label>BPM target<input aria-label={`BPM target ${i + 1}`} type="number" min={40} max={240} value={row.bpm} disabled={blocked} onChange={e => updateTask(i, 'bpm', Number(e.target.value))}/></label>
          <button type="button" disabled={blocked || tasks.length <= 1} onClick={() => { setTasks(rows => rows.filter((_, index) => i !== index)); setTaskIndex(0); }}>Remove exercise {i + 1}</button>
        </li>)}</ol>
        <button type="button" disabled={blocked || tasks.length >= 8} onClick={() => setTasks(rows => [...rows, { title: 'New exercise', minutes: 10, bpm: 100 }])}>Add exercise</button>
      </form>
      <div className="journal-start">
        <label>Exercise to practice<select aria-label="Exercise to practice" value={taskIndex} disabled={blocked || !!active} onChange={e => setTaskIndex(Number(e.target.value))}>{tasks.map((row, i) => <option key={i} value={i}>{row.title} · {row.minutes} min · {row.bpm} BPM</option>)}</select></label>
        <button disabled={blocked || !!active || !valid || doc.sessions.length >= 256} onClick={() => action('start', { ...task, setName: name.trim() })}>Start practice timer</button>
        <button disabled={blocked || recording || !valid} onClick={() => setParameter('METRO_BPM', task.bpm)}>Use exercise tempo</button>
      </div>
      <h3>Session history</h3><p className="practice-note">{time(completedSeconds)} across {doc.sessions.filter(row => row.state === 'finished').length} finished sessions. Notes are saved when you finish. Interrupted sessions retain their last saved time and do not resume automatically.</p>
      <label>Find a session<input aria-label="Find a session" value={filter} onChange={e => setFilter(e.target.value)}/></label>
      <div className="journal-history">{rows.length ? rows.map(row => <article key={row.id}>
        <div><strong>{row.title}</strong><span>{row.setName} · {date(row.started)}</span><span>{time(row.seconds)} · {row.bpm} BPM target · {row.state}{row.seconds >= row.minutes * 60 ? ' · Time target reached' : ''}</span>{row.notes && <p>{row.notes}</p>}</div>
        <button disabled={blocked || row.id === active?.id} onClick={() => remove('removeSession', row.id)}>{confirm === `removeSession:${row.id}` ? 'Confirm remove session' : 'Remove session'}</button>
      </article>) : <p className="practice-note">No sessions yet. Start an exercise to build your history.</p>}</div>
      <div className="journal-transfer"><button disabled={blocked || recording} onClick={() => transfer(true)}>Export sets &amp; history</button><button disabled={blocked || recording || !!active} onClick={() => transfer(false)}>Import sets &amp; history</button></div>
      <p className="practice-note">Up to 32 sets, 8 exercises per set and 256 sessions. Export a JSON copy before removing older entries. Import adds missing identities and preserves existing entries; conflicting identities are rejected.</p>
    </>}
    {j.busy && <p className="practice-note" role="status">Saving practice history…</p>}
    {j.error && <p className="practice-error" role="alert">{j.error}</p>}
  </section>;
}
