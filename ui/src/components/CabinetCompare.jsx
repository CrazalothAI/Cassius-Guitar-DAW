import { setParameter, useParameters } from '../parameterState.js';

export function cabinetRoute(values, status) {
  const source = Math.round(values.AMP_SOURCE), mode = Math.round(values.CAB_MODE);
  const fullRig = source === 3 && (values.CAPTURE_KIND === 3 || (values.CAPTURE_KIND === 0 && status.ampHasCab));
  const a = Boolean(status.ir), b = Boolean(status.irB) && values.CAB_B_ON >= .5;
  if (source === 0) return {title: 'Current rig routing', detail: 'Choose an explicit amp source to edit cabinet routing.', external: false};
  if (mode === 3) return {title: 'Cabinet bypassed', detail: 'Loaded IRs are retained for later use.', external: false};
  if (mode === 2) return {title: 'Built-in 4×12', detail: 'Cabinet cuts shape the built-in speaker. External IR blend controls are inactive.', external: false};
  if (mode === 0 && fullRig) return {title: 'Cabinet included in the capture', detail: 'Auto bypasses separate cabinets. External IR mode is an intentional override.', external: false};
  if (mode === 0 && source === 4) return {title: 'Natural DI · no cabinet', detail: 'Choose External IR for an optional acoustic body response.', external: false};
  if (a || b) return {title: a && b ? 'Parallel cabinets A + B' : a ? 'Cabinet A only' : 'Cabinet B only', detail: a && b ? 'Compare the two responses, then set your preferred blend.' : 'One active IR keeps its level independently of the blend setting.', external: true, dual: a && b};
  if (mode === 1) return {title: 'External IR not loaded', detail: 'Load cabinet A or load and enable cabinet B.', external: false};
  if (source === 2 || status.speakerSim) return {title: 'Built-in 4×12', detail: 'Auto uses the built-in speaker. Load an IR to try an external cabinet.', external: false};
  return {title: 'No separate cabinet active', detail: 'Load an external IR or choose the built-in speaker if your tone needs one.', external: false};
}

export default function CabinetCompare({status}) {
  const values = useParameters(['AMP_SOURCE', 'CAB_MODE', 'CAPTURE_KIND', 'CAB_B_ON', 'CAB_BLEND', 'CAB_A_DELAY', 'CAB_B_DELAY', 'CAB_A_INVERT', 'CAB_B_INVERT']);
  const route = cabinetRoute(values, status);
  const busy = Boolean(status.rigLoading || status.practice?.recordMode > 0 || status.review?.playing);
  const opposite = (values.CAB_A_INVERT >= .5) !== (values.CAB_B_INVERT >= .5);
  const offset = values.CAB_B_DELAY - values.CAB_A_DELAY;
  const reset = () => ['CAB_A_DELAY', 'CAB_B_DELAY', 'CAB_A_INVERT', 'CAB_B_INVERT'].forEach(id => setParameter(id, 0));
  return <section className="cabinet-compare" aria-label="Cabinet comparison">
    <div><h3>{route.title}</h3><p>{route.detail}</p></div>
    <div className="cabinet-compare-actions" role="group" aria-label="Set cabinet blend">
      {[[0, 'A only'], [50, '50/50 blend'], [100, 'B only']].map(([blend, label]) => <button key={blend} disabled={!route.dual || busy} aria-pressed={route.dual && values.CAB_BLEND === blend} onClick={() => setParameter('CAB_BLEND', blend)}>{label}</button>)}
      <button disabled={!route.external || busy} onClick={reset}>Reset alignment &amp; polarity</button>
    </div>
    {route.external && <p className="cabinet-alignment-note">A {values.CAB_A_DELAY.toFixed(2)} ms · B {values.CAB_B_DELAY.toFixed(2)} ms{route.dual && <> · {offset === 0 ? 'No relative delay' : `${offset > 0 ? 'B' : 'A'} delayed ${Math.abs(offset).toFixed(2)} ms relative to ${offset > 0 ? 'A' : 'B'}`} · {opposite ? 'Opposite polarity may cancel similar responses; compare each cabinet alone.' : 'Same polarity'}</>}</p>}
    <small>{busy ? 'Stop recording or take playback before using comparison shortcuts.' : 'Shortcuts edit your tone and cabinet settings. They affect processed recordings; Input, Master and Play Along levels stay unchanged.'}</small>
  </section>;
}
