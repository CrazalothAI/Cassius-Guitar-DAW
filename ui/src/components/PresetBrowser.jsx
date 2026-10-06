import { familyOf, notes, voices } from '../presets.js';

const order = voices.flatMap(v => v.presets);

// One display replaces the tone-family buttons and the "More tones" menu: arrows step
// through every preset, and clicking the name opens the full list grouped by family.
export default function PresetBrowser({ current, edited, onChoose, onRevert, compare, compareSide, onCompare, showCompare = true, rigs = [], currentRig, loading = false }) {
  const step = direction => {
    const choices = current && !currentRig ? order : rigs.filter(r => !r.unavailable && !r.previewUnavailable).map(r => r.id);
    if (!choices.length) return;
    const i = choices.indexOf(currentRig?.id || current);
    onChoose(choices[i < 0 ? (direction > 0 ? 0 : choices.length - 1) : (i + direction + choices.length) % choices.length]);
  };
  return <div className="preset-browser">
    <button className="preset-step" aria-label="Previous preset" disabled={loading} onClick={() => step(-1)}>‹</button>
    <div className="preset-display" title={currentRig?.notes || (current ? notes[current] : undefined)}>
      <span className="preset-family">{currentRig ? `Complete rig · ${currentRig.amp}` : `Starting point · ${current ? familyOf(current) : 'Custom'}`}{edited && <em>Edited</em>}</span>
      <span className="preset-name">{currentRig?.name || current || 'Choose a rig'}</span>
      <select aria-label="Preset" disabled={loading} value={currentRig?.id || current || ''} onChange={e => onChoose(e.target.value)}>
        {!current && !currentRig && <option value="">Choose a rig</option>}
        <optgroup label="Complete capture rigs">{rigs.filter(r => r.assets).map(r => <option key={r.id} value={r.id} disabled={r.unavailable || r.previewUnavailable}>{r.name}{r.unavailable ? ' · Import sounds first' : ''}</option>)}</optgroup>
        <optgroup label="Cassian built-in rigs">{rigs.filter(r => !r.assets).map(r => <option key={r.id} value={r.id}>{r.name} · {r.amp}</option>)}</optgroup>
        {voices.map(v => <optgroup key={v.label} label={`Legacy controls · ${v.label}`}>{v.presets.map(p => <option key={p} value={p}>{p}</option>)}</optgroup>)}
      </select>
    </div>
    <button className="preset-step" aria-label="Next preset" disabled={loading} onClick={() => step(1)}>›</button>
    <div className="preset-actions">
      {edited && <button className="chip" disabled={loading} aria-label={`Revert to ${currentRig?.name || current}`} onClick={onRevert}>Revert</button>}
      {showCompare && <button className={`chip${compare ? ' active' : ''}`} aria-label="A/B compare" onClick={onCompare}>{compare ? `A/B · ${compareSide}` : 'A/B'}</button>}
    </div>
  </div>;
}
