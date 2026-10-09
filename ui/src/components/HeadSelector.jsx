import { ampHeads } from '../ampHeads.js';

export default function HeadSelector({ head, busy, onChoose }) {
  return <section className="head-selector" aria-label="Amplifier collection">
    <div className="head-selector-heading"><span>THE COLLECTION</span><small>Choose a head &amp; starter rig<br/>Save edits before switching</small></div>
    <div className="head-options">{ampHeads.map(item => <button key={item.id} className={`head-option head-${item.id}`} aria-label={`Load ${item.name} ${item.voice.toLowerCase()} rig`} aria-pressed={head.id === item.id} disabled={busy} onClick={() => onChoose(item.rig)} title={`Load a complete ${item.voice.toLowerCase()} starting rig · ${item.finish}. Replaces the current tone and scenes; save edits first.`}>
      <img src={item.art} alt="" width="112" height="38"/>
      <span><strong>{item.name}</strong><small>{item.voice}</small></span>
      <i aria-hidden="true"/>
    </button>)}</div>
  </section>;
}
