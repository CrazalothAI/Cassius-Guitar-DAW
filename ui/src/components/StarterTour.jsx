import { useRef, useState } from 'react';
import catalog from '../startingRigs.json';

const guide = [
  ['factory.prism-clean', 'Expressive clean', 'Play open chords, arpeggios and soft-to-hard picking. Listen for clarity and how the compressor responds.'],
  ['factory.copper-blues', 'Warm crunch', 'Try double stops and sustained bends; turn the guitar volume down to explore the edge of breakup.'],
  ['factory.iron-rhythm', 'Tight rhythm', 'Try palm-muted low notes and chords. Listen for a clear pick attack and a controlled stop between phrases.'],
  ['factory.velvet-lead', 'Singing lead', 'Try legato, alternate picking and held bends. Listen for note separation and the delay/reverb behind the phrase.'],
  ['factory.midnight-space', 'Ambient clean', 'Leave space between chord changes to hear the clean note and the decay of the effects.'],
  ['factory.natural-nylon', 'Natural nylon / DI', 'Start with a piezo or acoustic pickup and fingerpicked passages. This is a DI starting point; it does not turn an electric guitar into a nylon guitar.'],
].map(([id, label, listen]) => ({...catalog.rigs.find(r => r.id === id), label, listen})).filter(r => r.id && !r.assets);

export default function StarterTour({status, onChooseRig, onError}) {
  const [selected, setSelected] = useState(guide[0]?.id), [busy, setBusy] = useState(false), loading = useRef(false);
  const rig = guide.find(r => r.id === selected);
  if (!rig) return null;
  const blocked = !onChooseRig || busy || status.rigLoading || status.practice?.recordMode > 0 || status.review?.playing;
  const load = async () => {
    if (blocked || loading.current) return;
    loading.current = true; setBusy(true);
    try { await onChooseRig(rig.id); }
    catch (error) { onError?.({title: 'Starter tones', text: error.message || 'Could not load this starter rig.'}); }
    finally { loading.current = false; setBusy(false); }
  };
  return <details className="starter-tour"><summary>Audition original starter tones</summary>
    <p>Six contrasting complete rigs to start with. All 22 Cassian built-in rigs include their effects and cabinet choices and work without extra files. Save your edits first: loading a starter replaces the current tone and scenes.</p>
    <label>Sound to try <select aria-label="Starter sound to try" value={selected} disabled={busy} onChange={e => setSelected(e.target.value)}>{guide.map(r => <option key={r.id} value={r.id}>{r.label} · {r.name}</option>)}</select></label>
    <p><strong>{rig.name}</strong> · {rig.amp}<br/>{rig.listen}</p>
    <button disabled={!!blocked} onClick={load}>{busy ? 'Loading starter…' : `Load ${rig.name} and open Tone`}</button>
    {(status.practice?.recordMode > 0 || status.review?.playing) && <p className="practice-note">Stop recording or take playback before auditioning a different rig.</p>}
    <p className="practice-note">Repeat the same phrase with each sound. Input trim, Master and Play Along mix stay where they are. Use the rig bar’s A/B and optional Match level for listening comparisons; matching is approximate. The preset menu contains the rest of the collection. Capture recipes are separate and need their listed sound files.</p>
  </details>;
}
