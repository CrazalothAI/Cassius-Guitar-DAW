import { useState } from 'react';
import { invoke, native } from '../juce/bridge.js';
import { snapshotParameters } from '../parameterState.js';
import { previewRigs, writePreviewRigs } from './Library.jsx';
import UtilityDialog from './UtilityDialog.jsx';
export default function RigBar({status, previewActive, previewEdited, onPreviewRig, onError, onCompare, compare, compareSide, matchCompare, onMatch, amp}) {
  const [saving, setSaving] = useState(false), [name, setName] = useState(''), [busy, setBusy] = useState(false), [error, setError] = useState('');
  const activeName = native ? status.activeRigName : previewActive?.name;
  const saved = native ? status.activeRigSaved : !!previewActive;
  const edited = native ? status.activeRigEdited : previewEdited;
  const save = async asNew => {
    if (busy) return;
    setBusy(true); setError('');
    try {
      if (native) {
        const error = await invoke(asNew ? 'saveRig' : 'updateActiveRig', ...(asNew ? [name.trim()] : []));
        if (error) throw new Error(error);
      } else {
        const row = {id: asNew ? `preview-${Date.now()}-${Math.random()}` : previewActive.id, name: asNew ? name.trim() : activeName, parameters: snapshotParameters()};
        const list = previewRigs(); writePreviewRigs([...list.filter(x => x.id !== row.id), row]); onPreviewRig(row);
      }
      setSaving(false);
    } catch (error) { const text = error.message || 'Could not save this rig. Please try again.'; if (saving) setError(text); else onError({title: 'Save rig', text}); }
    finally { setBusy(false); }
  };
  return <>
    <section className="rig-bar" aria-label="Current complete rig">
      <div className="rig-title"><span>{activeName ? saved ? 'SAVED RIG' : 'UNSAVED RIG' : 'UNSAVED TONE'}{edited && <em>Edited</em>}</span><strong>{activeName || 'Untitled rig'}</strong><small>{amp}</small></div>
      <div className="rig-actions"><button className="text-button" aria-label="Save rig" disabled={busy || status.rigLoading} onClick={() => saved ? save(false) : (setError(''), setName(activeName || ''), setSaving(true))}>Save</button><button className="text-button" aria-label="Save rig as" disabled={busy || status.rigLoading} onClick={() => { setError(''); setName(activeName ? `${activeName} copy` : ''); setSaving(true); }}>Save as</button><button className={`chip${compare ? ' active' : ''}`} aria-label="A/B compare" onClick={onCompare}>{compare ? `A/B · ${compareSide}` : 'A/B'}</button><label className="compare-match" title="Approximate matching from similar recent playing"><input type="checkbox" aria-label="Match A/B loudness" disabled={!native} checked={matchCompare} onChange={e => onMatch(e.target.checked)}/> Match level</label></div>
    </section>
    {saving && <UtilityDialog title="Save complete rig" onClose={() => !busy && setSaving(false)}><form className="rig-save-form" onSubmit={e => { e.preventDefault(); if (name.trim()) save(true); }}><label>Rig name<input aria-label="Rig name" maxLength={80} value={name} onChange={e => setName(e.target.value)} disabled={busy}/></label><button disabled={busy || !name.trim()} type="submit">Save complete rig</button></form>{error && <p className="practice-error" role="alert">{error}</p>}<p className="practice-note">{native ? 'Saves the current amp, pedal, cabinets, effects and scenes.' : 'Browser preview saves control settings only. Use the native app for complete rigs.'}</p></UtilityDialog>}
  </>;
}
