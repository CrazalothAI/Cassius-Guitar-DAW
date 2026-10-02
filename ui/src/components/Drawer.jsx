import { useRef } from 'react';
import Knob from './Knob.jsx';
import Switch from './Switch.jsx';
import { setParameter, useToggle } from '../parameterState.js';

export const pages = ['Amp', 'Effects', 'Rig'];

function Group({ title, status, children }) {
  return <div className="control-group" role="group" aria-label={title}>
    <div className="group-head"><h3>{title}</h3>{status}</div>
    <div className="group-knobs">{children}</div>
  </div>;
}

// Stages switch off by turning their main knob fully down (shown as Off); a stage's
// other knobs dim while it is off but stay adjustable.
function Amp({ clean, native, status }) {
  const gateOn = useToggle('GATE_ON');
  return <div className="control-groups">
    <Group title="Levels"><Knob id="INPUT_GAIN" small /><Knob id="AMP_OUT" small /></Group>
    <Group title="Noise gate" status={gateOn && native && <span className={`gate-state${status.gate > .1 ? ' open' : ''}`}>{status.gate > .1 ? 'Open' : 'Closed'}</span>}>
      <Knob id="GATE_THRESH" enable="GATE_ON" small /><Knob id="GATE_RELEASE" small muted={!gateOn} />
    </Group>
    <Group title="Tone"><Knob id={clean ? 'CLEAN_COMP' : 'TIGHT'} small /><Knob id="PRESENCE" small /><Knob id="HIGH_CUT" small /></Group>
  </div>;
}

function Effects({ status }) {
  const resonance = useToggle('DYN_RES_ON');
  return <div className="control-groups">
    <Group title="Delay"><Knob id="DELAY_TIME" small /><Knob id="DELAY_MIX" small /><Knob id="DELAY_WIDTH" small /></Group>
    <Group title="Room"><Knob id="REVERB_SIZE" small /><Knob id="MICRO_DELAY" small /></Group>
    <Group title="Character" status={resonance && status.dynResCut < -0.5 && <span className="telemetry-tag">{status.dynResCut.toFixed(1)} dB</span>}>
      <Knob id="DYN_RES_AMOUNT" enable="DYN_RES_ON" small /><Knob id="CHUG_ATTACK" small />
      <Knob id="THICKEN_MIX" enable="THICKEN_ON" small /><Knob id="PIEZO_BLEND" enable="PIEZO_ON" small />
    </Group>
  </div>;
}

function RigRow({ label, file, empty, note, children }) {
  return <div className="rig-row">
    <span className="rig-label">{label}</span>
    <p><span className="rig-file" title={file || undefined}>{file || empty}</span>{note && <small>{note}</small>}</p>
    {children}
  </div>;
}

function Meter({ label, value }) {
  const db = Math.max(-60, Math.min(0, value > 0 ? 20 * Math.log10(value) : -60));
  return <div className={`meter${db > -1 ? ' hot' : ''}`}>
    <span>{label}</span>
    <div className="meter-track" role="meter" aria-label={`${label} level`} aria-valuemin={-60} aria-valuemax={0} aria-valuenow={Math.round(db)}>
      <i style={{ width: `${(db + 60) / 60 * 100}%` }} />
    </div>
  </div>;
}
export { Meter };

// Captures recorded at another rate are resampled to the host rate by the engine.
const kHz = hz => `${(hz / 1000).toFixed(1)} kHz`;
const resampled = (on, expected, status) => on && expected > 0 && status.sampleRate > 0 ? `${kHz(expected)} capture · resampled to ${kHz(status.sampleRate)}` : '';

function Rig({ clean, native, status, onLoad, onRemove }) {
  const ampRate = resampled(status.ampResampled, status.ampExpectedRate, status);
  const signed = db => `${db > 0 ? '+' : ''}${db.toFixed(1)} dB`;
  const ampNote = [status.model && status.ampLevelled ? `Level matched ${signed(status.ampLevelDb)}` : '', ampRate].filter(Boolean).join(' · ');
  const pedalRate = resampled(status.pedalResampled, status.pedalExpectedRate, status);
  const load = (type, text) => <button className="text-button" disabled={!native} onClick={() => onLoad(type)}>{text}<span aria-hidden="true"> ↗</span></button>;
  // Unloading returns the stage to its built-in (or bypassed) state.
  const remove = (type, file, name) => file && native && <button className="text-button quiet" aria-label={`Remove ${name}`} onClick={() => onRemove(type)}>Remove</button>;
  // Sets input gain so the raw interface peak lands near -12 dBFS.
  const autoTrim = () => {
    if (!native || !(status.input > 0.0001)) return;
    setParameter('INPUT_GAIN', Math.max(-12, Math.min(12, -12 - 20 * Math.log10(status.input))));
  };
  return <div className="rig-list">
    <RigRow label="AMP" file={status.model} empty={status.fallbackAmp ? 'Built-in high-gain amp' : 'No capture loaded'}
      note={clean ? 'Bypassed · Lumen clean is active' : status.model ? ampNote : status.fallbackAmp && 'Load a capture to replace it'}>
      {remove('amp', status.model, 'amp capture')}{load('amp', status.model ? 'Change amp' : 'Load amp model')}
    </RigRow>
    <RigRow label="PEDAL" file={status.pedal} empty="No pedal capture" note={['Before the amp · bypassed on cleans', pedalRate].filter(Boolean).join(' · ')}>
      <Switch id="PEDAL_ON" name="Pedal enabled" disabled={clean || !status.pedal} forcedOff={clean} />
      {remove('pedal', status.pedal, 'pedal capture')}{load('pedal', status.pedal ? 'Change pedal' : 'Load pedal NAM')}
    </RigRow>
    <RigRow label="CAB" file={status.ir} empty={status.speakerSim ? 'Built-in 4×12 speaker' : 'Off · optional for full-rig captures'}
      note={clean ? 'Bypassed · clean uses its own speaker rolloff' : !status.ir && status.speakerSim && 'Used while no IR is loaded and the amp has no cabinet'}>
      {remove('cab', status.ir, 'cabinet IR')}{load('cab', status.ir ? 'Change cabinet' : 'Load cabinet IR')}
    </RigRow>
    <div className="rig-row signal-row">
      <span className="rig-label">SIGNAL</span>
      <div className="stage-meters">
        <Meter label="IN" value={status.input} /><Meter label="PEDAL" value={status.prePedal} /><Meter label="AMP" value={status.postAmp} />
        <Meter label="CAB" value={status.postCab} /><Meter label="OUT" value={status.output} />
      </div>
      <button className="text-button" disabled={!native || !(status.input > 0.0001)} onClick={autoTrim}
        title="Play your hardest, then press: sets Input so peaks land near -12 dBFS">Auto trim input</button>
    </div>
  </div>;
}

const views = { Amp, Effects, Rig };

export default function Drawer({ page, onPage, ...props }) {
  const tabs = useRef([]);
  const View = views[page] ?? Amp;
  const keyDown = e => {
    const i = pages.indexOf(page);
    const target = { ArrowRight: i + 1, ArrowLeft: i - 1, Home: 0, End: pages.length - 1 }[e.key];
    if (target === undefined) return;
    e.preventDefault();
    const next = (target + pages.length) % pages.length;
    onPage(pages[next]);
    tabs.current[next]?.focus();
  };
  return <div id="effects-panel" className="detail-drawer">
    <div className="detail-tabs" role="tablist" aria-label="Detailed controls" onKeyDown={keyDown}>
      {pages.map((name, i) => <button key={name} ref={el => { tabs.current[i] = el; }} role="tab" id={`tab-${name}`}
        aria-selected={page === name} aria-controls="detail-page" tabIndex={page === name ? 0 : -1} onClick={() => onPage(name)}>{name}</button>)}
    </div>
    <div className="detail-page" role="tabpanel" id="detail-page" aria-labelledby={`tab-${page}`}><View {...props} /></div>
  </div>;
}
