import { familyOf, notes, voices } from '../presets.js';

const order = voices.flatMap(v => v.presets);

// One display replaces the tone-family buttons and the "More tones" menu: arrows step
// through every preset, and clicking the name opens the full list grouped by family.
export default function PresetBrowser({ current, edited, onChoose, onRevert, compare, compareSide, onCompare }) {
  const step = direction => {
    const i = order.indexOf(current);
    onChoose(order[i < 0 ? (direction > 0 ? 0 : order.length - 1) : (i + direction + order.length) % order.length]);
  };
  return <div className="preset-browser">
    <button className="preset-step" aria-label="Previous preset" onClick={() => step(-1)}>‹</button>
    <div className="preset-display" title={current ? notes[current] : undefined}>
      <span className="preset-family">{current ? familyOf(current) : 'Custom'}{edited && <em>Edited</em>}</span>
      <span className="preset-name">{current ?? 'Choose a tone'}</span>
      <select aria-label="Preset" value={current ?? ''} onChange={e => onChoose(e.target.value)}>
        {!current && <option value="">Choose a tone</option>}
        {voices.map(v => <optgroup key={v.label} label={v.label}>{v.presets.map(p => <option key={p} value={p}>{p}</option>)}</optgroup>)}
      </select>
    </div>
    <button className="preset-step" aria-label="Next preset" onClick={() => step(1)}>›</button>
    <div className="preset-actions">
      {edited && <button className="chip" aria-label={`Revert to ${current}`} onClick={onRevert}>Revert</button>}
      <button className={`chip${compare ? ' active' : ''}`} aria-label="A/B compare" onClick={onCompare}>{compare ? `A/B · ${compareSide}` : 'A/B'}</button>
    </div>
  </div>;
}
