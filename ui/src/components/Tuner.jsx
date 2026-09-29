// Shown over the amp grille, so opening the tuner never moves the controls.
const ticks = [-50, -40, -30, -20, -10, 0, 10, 20, 30, 40, 50];
export default function Tuner({ status }) {
  const active = Boolean(status.tunerActive) && status.tunerHz > 0;
  const cents = active ? Math.max(-50, Math.min(50, status.tunerCents || 0)) : 0;
  const inTune = active && Math.abs(cents) <= 3;
  const state = !active ? '' : inTune ? ' in-tune' : cents < 0 ? ' flat' : ' sharp';
  return <div className={`tuner${active ? ' active' : ''}${state}`} role="group" aria-label="Chromatic tuner">
    <div className="tuner-readout">
      <span className="tuner-arrow flat-arrow" aria-hidden="true">♭</span>
      <span className="tuner-note">{active ? status.tunerNote : '—'}</span>
      <span className="tuner-arrow sharp-arrow" aria-hidden="true">♯</span>
    </div>
    <div className="tuner-scale" aria-hidden="true">
      {ticks.map(t => <i key={t} className={t === 0 ? 'centre' : ''} style={{ left: `${50 + t}%` }} />)}
      {active && <b className="tuner-cursor" style={{ left: `${50 + cents}%` }} />}
    </div>
    <p className="tuner-detail">{active
      ? <>{inTune ? 'In tune' : `${cents > 0 ? '+' : ''}${Math.round(cents)} cents`} · {status.tunerHz.toFixed(1)} Hz</>
      : 'Play a single open string'}</p>
  </div>;
}
