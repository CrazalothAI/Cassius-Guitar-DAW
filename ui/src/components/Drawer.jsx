import { useRef } from 'react';
import Knob from './Knob.jsx';
import Switch from './Switch.jsx';
import { useToggle } from '../parameterState.js';

export const pages = ['Shape', 'Low-Tuned', 'Piezo', 'Space', 'Rig'];

// Knobs a switch bypasses stay adjustable but dim, so the stage can be set up before it is enabled.
function Group({ title, toggle, status, children }) {
  return <div className="control-group" role="group" aria-label={title}>
    <div className="group-head"><h3>{title}</h3>{toggle}{status}</div>
    <div className="group-knobs">{children}</div>
  </div>;
}

function Shape({ clean, native, status }) {
  const gateOn = useToggle('GATE_ON');
  return <>
    <div className="control-groups">
      <Group title="Levels"><Knob id="INPUT_GAIN" small /><Knob id="AMP_OUT" small /></Group>
      <Group title="Noise gate" toggle={<Switch id="GATE_ON" name="Noise gate enabled" />}
        status={gateOn && native && <span className={`gate-state${status.gate > .1 ? ' open' : ''}`}>{status.gate > .1 ? 'Open' : 'Closed'}</span>}>
        <Knob id="GATE_THRESH" small muted={!gateOn} /><Knob id="GATE_RELEASE" small muted={!gateOn} />
      </Group>
      <Group title="Tone"><Knob id={clean ? 'CLEAN_COMP' : 'TIGHT'} small /><Knob id="PRESENCE" small /><Knob id="HIGH_CUT" small /></Group>
    </div>
    <p className="detail-note">A longer gate release preserves sustained notes. Tight and High cut are off at their ends.</p>
  </>;
}

function LowTuned({ status }) {
  const resonance = useToggle('DYN_RES_ON'), thicken = useToggle('THICKEN_ON');
  return <>
    <div className="control-groups">
      <Group title="Resonance cut" toggle={<Switch id="DYN_RES_ON" name="Dynamic resonance" />}
        status={resonance && status.dynResCut < -0.5 && <span className="telemetry-tag">{status.dynResCut.toFixed(1)} dB</span>}>
        <Knob id="DYN_RES_AMOUNT" small muted={!resonance} />
      </Group>
      <Group title="Attack"><Knob id="CHUG_ATTACK" small /></Group>
      <Group title="Thicken" toggle={<Switch id="THICKEN_ON" name="Sub-synthesis" />}><Knob id="THICKEN_MIX" small muted={!thicken} /></Group>
    </div>
    <p className="detail-note">Dynamic 200–400 Hz chug suppression & sub-octave synthesis for Drop E/Z tunings.</p>
  </>;
}

function Piezo() {
  const piezo = useToggle('PIEZO_ON');
  return <>
    <div className="control-groups">
      <Group title="Piezo body" toggle={<Switch id="PIEZO_ON" name="Piezo body simulation" />}><Knob id="PIEZO_BLEND" small muted={!piezo} /></Group>
    </div>
    <p className="detail-note">Acoustic body modal resonance and exciter sparkle for magnetic pickups (Tim Henson style).</p>
  </>;
}

function Space() {
  return <>
    <div className="control-groups">
      <Group title="Delay"><Knob id="DELAY_TIME" small /><Knob id="DELAY_MIX" small /><Knob id="DELAY_WIDTH" small /></Group>
      <Group title="Reverb"><Knob id="REVERB_SIZE" small /></Group>
      <Group title="Stereo"><Knob id="MICRO_DELAY" small /></Group>
    </div>
    <p className="detail-note">Reverb mix is Space on the amp. Micro-delay (0–1.0 ms) adds sub-sample phase separation for wide double-tracked guitars.</p>
  </>;
}

function RigRow({ label, file, empty, note, children }) {
  return <div className="rig-row">
    <span className="rig-label">{label}</span>
    <p><span className="rig-file" title={file || undefined}>{file || empty}</span>{note && <small>{note}</small>}</p>
    {children}
  </div>;
}

// Captures recorded at another rate are resampled to the host rate by the engine.
const kHz = hz => `${(hz / 1000).toFixed(1)} kHz`;
const resampled = (on, expected, status) => on && expected > 0 && status.sampleRate > 0 ? `${kHz(expected)} capture · resampled to ${kHz(status.sampleRate)}` : '';

function Rig({ clean, native, status, onLoad }) {
  const ampRate = resampled(status.ampResampled, status.ampExpectedRate, status);
  const pedalRate = resampled(status.pedalResampled, status.pedalExpectedRate, status);
  const load = (type, text) => <button className="text-button" disabled={!native} onClick={() => onLoad(type)}>{text}<span aria-hidden="true"> ↗</span></button>;
  return <div className="rig-list">
    <RigRow label="AMP" file={status.model} empty="No capture loaded" note={clean ? 'Bypassed · Lumen clean is active' : ampRate}>
      {load('amp', status.model ? 'Change amp' : 'Load amp model')}
    </RigRow>
    <RigRow label="PEDAL" file={status.pedal} empty={status.pedalFallback ? 'Built-in asymmetric drive' : 'No pedal capture'} note={['Before the amp · bypassed on cleans', pedalRate].filter(Boolean).join(' · ')}>
      <Switch id="PEDAL_ON" name="Drive pedal" disabled={clean} forcedOff={clean} />
      {load('pedal', status.pedal ? 'Change drive pedal' : 'Load drive pedal')}
    </RigRow>
    <RigRow label="CAB" file={status.ir} empty="Off · optional for full-rig captures" note={clean && 'Bypassed · clean uses its own speaker rolloff'}>
      {load('cab', status.ir ? 'Change cabinet' : 'Load cabinet IR')}
    </RigRow>
  </div>;
}

const views = { Shape, 'Low-Tuned': LowTuned, Piezo, Space, Rig };

export default function Drawer({ page, onPage, ...props }) {
  const tabs = useRef([]);
  const View = views[page];
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
