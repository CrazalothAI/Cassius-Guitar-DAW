import { useEffect, useState } from 'react';
import { invoke } from '../juce/bridge.js';
import { time } from '../practiceTime.js';
const preciseTime = seconds => `${time(seconds)}.${String(Math.floor(((seconds || 0) % 1) * 100 + 1e-7)).padStart(2, '0')}`;

export default function PracticeSections({p, available, disabled, onError, takeId, takeVersion}) {
  const rows = p.sections ?? [], [open, setOpen] = useState(false), [selected, setSelected] = useState(''), [name, setName] = useState('New section'), [busy, setBusy] = useState(false);
  useEffect(() => { if (selected && !rows.some(row => row.id === selected)) { setSelected(''); setName('New section'); } }, [p.sectionRevision]);
  const blocked = !available || disabled || busy || !p.duration || p.loading || p.starting || p.counting || p.recordMode > 0;
  const action = async (method, ...args) => {
    if (blocked) return; setBusy(true);
    try {
      const command = {savePracticeSection: 'save', recallPracticeSection: 'recall', removePracticeSection: 'remove'}[method];
      const error = takeId ? await invoke('takeReviewSection', takeId, takeVersion, command, command === 'save' ? args[0] : '', command === 'save' ? args[1] : args[0]) : await invoke(method, ...args);
      if (error) throw new Error(String(error));
    }
    catch (e) { onError({title: takeId ? 'Take sections' : 'Practice sections', text: e.message || 'Couldn’t update this section.'}); }
    finally { setBusy(false); }
  };
  return <section className="practice-sections" aria-label={takeId ? 'Saved take sections' : 'Saved practice sections'}>
    <button type="button" className="text-button" aria-expanded={open} onClick={() => setOpen(!open)}>Sections · {rows.length}</button>
    {open && <form onSubmit={e => { e.preventDefault(); action('savePracticeSection', name.trim(), selected); }}>
      <label>Section<select aria-label="Saved section" value={selected} disabled={blocked} onChange={e => { const id = e.target.value; setSelected(id); setName(rows.find(row => row.id === id)?.name || 'New section'); }}><option value="">New section</option>{rows.map(row => <option key={row.id} value={row.id}>{row.name} · {preciseTime(row.a)}–{preciseTime(row.b)}</option>)}</select></label>
      <label>Name<input aria-label="Section name" value={name} maxLength={48} disabled={blocked} onChange={e => setName(e.target.value)}/></label>
      <button type="submit" disabled={blocked || !name.trim() || (p.b || 0) - (p.a || 0) < .05 || (!selected && rows.length >= 32)}>{selected ? 'Replace section' : 'Save loop'}</button>
      <button type="button" disabled={blocked || !selected} onClick={() => action('recallPracticeSection', selected)}>Recall section</button>
      <button type="button" disabled={blocked || !selected} onClick={() => action('removePracticeSection', selected)}>Delete section</button>
      <p className="practice-note">Save the current A–B range. Recall pauses at A and enables looping; press {takeId ? 'Resume' : 'Play'} when ready. Sections follow the same file contents, including renamed copies, and stay separate from tone rigs.{takeId && ' Take sections are saved separately from backing-track sections.'}</p>
    </form>}
    {p.sectionError && <p className="practice-error" role="alert">{p.sectionError}</p>}
  </section>;
}
