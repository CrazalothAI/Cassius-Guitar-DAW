import { useRef } from 'react';
import Knob from './Knob.jsx';
import Switch from './Switch.jsx';
import { setParameter, useParameters, useToggle } from '../parameterState.js';

// The rig in signal order. Each stage is a tab; its controls open in the panel below.
export const pages = ['Input', 'Pedal', 'Amp', 'Cab', 'EQ', 'Effects'];

const level = value => Math.max(0, Math.min(1, value > 0 ? (20 * Math.log10(value) + 60) / 60 : 0));

export function Meter({ label, value }) {
  const db = Math.max(-60, Math.min(0, value > 0 ? 20 * Math.log10(value) : -60));
  return <div className={`meter${db > -1 ? ' hot' : ''}`}>
    <span>{label}</span>
    <div className="meter-track" role="meter" aria-label={`${label} level`} aria-valuemin={-60} aria-valuemax={0} aria-valuenow={Math.round(db)}>
      <i style={{ width: `${(db + 60) / 60 * 100}%` }} />
    </div>
  </div>;
}

function Group({ title, status, children }) {
  return <div className="control-group" role="group" aria-label={title}>
    <div className="group-head"><h3>{title}</h3>{status}</div>
    <div className="group-knobs">{children}</div>
  </div>;
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

function Choice({id, label, options, disabled = false}) {
  const values = useParameters([id]);
  return <label className="slot-choice">{label}<select aria-label={label} disabled={disabled} value={Math.round(values[id])} onChange={e => setParameter(id, Number(e.target.value))}>
    {options.map((text, i) => <option key={text} value={i}>{text}</option>)}
  </select></label>;
}

function Input({ native, status }) {
  const gateOn = useToggle('GATE_ON');
  // Sets input gain so the raw interface peak lands near -12 dBFS.
  const autoTrim = () => {
    if (!native || !(status.input > 0.0001)) return;
    setParameter('INPUT_GAIN', Math.max(-12, Math.min(12, -12 - 20 * Math.log10(status.input))));
  };
  const hum = !native ? 'Removes mains hum on its own' : status.humCancelling ? `Removing ${status.mainsHz || 60} Hz hum` : 'Listening · engages if hum is heard between notes';
  return <div className="control-groups">
    <Group title="Level">
      <Knob id="INPUT_GAIN" small />
      <div className="trim">
        <Meter label="IN" value={status.input} />
        <button className="text-button" disabled={!native || !(status.input > 0.0001)} onClick={autoTrim}
          title="Play your hardest, then press: sets Input so peaks land near -12 dBFS">Auto trim</button>
      </div>
    </Group>
    <Group title="Noise gate" status={gateOn && native && <span className={`gate-state${status.gate > .1 ? ' open' : ''}`}>{status.gate > .1 ? 'Open' : 'Closed'}</span>}>
      <Knob id="GATE_THRESH" enable="GATE_ON" small /><Knob id="GATE_RELEASE" small muted={!gateOn} /><Knob id="CHUG_ATTACK" small />
    </Group>
    <Group title="Hum filter" status={<span className={`hum-state${status.humCancelling ? ' on' : ''}`}><i aria-hidden="true" />{hum}</span>} />
  </div>;
}

function Pedal({ clean, native, status, load, remove }) {
  const source = useParameters(['AMP_SOURCE']).AMP_SOURCE;
  const bypassed = clean && source < .5;
  const rate = resampled(status.pedalResampled, status.pedalExpectedRate, status);
  return <div className="rig-list">
    <RigRow label="PEDAL" file={status.pedal} empty="No pedal capture" note={[bypassed ? 'Bypassed by the current channel routing' : 'Before the selected amp', rate].filter(Boolean).join(' · ')}>
      <Switch id="PEDAL_ON" name="Pedal enabled" disabled={bypassed || !status.pedal} forcedOff={bypassed} />
      {remove('pedal', status.pedal, 'pedal capture')}{load('pedal', status.pedal ? 'Change pedal' : 'Load pedal NAM')}
    </RigRow>
    <Group title="Pedal gain"><Knob id="PEDAL_INPUT" small muted={!status.pedal || bypassed} /><Knob id="PEDAL_OUTPUT" small muted={!status.pedal || bypassed} /></Group>
  </div>;
}

function Amp({ clean, status, load, remove }) {
  const source = useParameters(['AMP_SOURCE']).AMP_SOURCE;
  const rate = resampled(status.ampResampled, status.ampExpectedRate, status);
  const signed = db => `${db > 0 ? '+' : ''}${db.toFixed(1)} dB`;
  const note = [status.model && status.ampLevelled ? `Level matched ${signed(status.ampLevelDb)}` : '', rate].filter(Boolean).join(' · ');
  return <div className="stage-split">
    <div className="rig-list">
      <Choice id="AMP_SOURCE" label="Amp source" options={['Current rig', 'Lumen · built-in clean', 'Ferrum · built-in high gain', 'NAM capture', 'Natural DI · acoustic / nylon']} />
      <RigRow label="AMP" file={status.model} empty={status.fallbackAmp ? 'Built-in high-gain amp' : 'No capture loaded'}
        note={source === 4 ? 'Neutral DI. No electric pickup simulation.' : source === 1 || (source === 0 && clean) ? 'Lumen is active · loaded capture is retained' : source === 2 ? 'Ferrum is active · loaded capture is retained' : status.model ? note : 'Select a built-in amp or load a capture'}>
        {remove('amp', status.model, 'amp capture')}{load('amp', status.model ? 'Change amp' : 'Load amp model')}
      </RigRow>
      {source === 3 && <Choice id="CAPTURE_KIND" label="Capture type" options={['Auto · metadata', 'Amp-only', 'Preamp-only', 'Full rig · includes cabinet']} />}
      {source === 3 && <Switch id="CAPTURE_MATCH" name="Capture level matching" />}
      {source === 3 && <small className="slot-note">Captures hold fixed amp settings. Drive and EQ shape the signal; they do not recreate every original knob. Preamp-only captures need a suitable power-amp stage.</small>}
    </div>
    <div className="control-groups">
      <Group title="Voice"><Knob id="AMP_OUT" small /><Knob id={clean ? 'CLEAN_COMP' : 'TIGHT'} small /><Knob id="PRESENCE" small /><Knob id="HIGH_CUT" small /></Group>
      {clean && <Group title="Compressor"><Knob id="COMP_THRESH" small /><Knob id="COMP_RATIO" small /><Knob id="COMP_ATTACK" small /><Knob id="COMP_RELEASE" small /><Knob id="COMP_MAKEUP" small /></Group>}
    </div>
  </div>;
}

function Cab({ clean, status, load, remove }) {
  const values = useParameters(['AMP_SOURCE', 'CAB_MODE', 'CAPTURE_KIND']);
  const legacy = values.AMP_SOURCE === 0;
  const fullRig = values.AMP_SOURCE === 3 && (values.CAPTURE_KIND === 3 || (values.CAPTURE_KIND === 0 && status.ampHasCab));
  return <div className="rig-list">
    <Choice id="CAB_MODE" label="Cabinet mode" disabled={legacy} options={['Auto', 'External IR · intentional override', '4×12 · built-in', 'Off']} />
    <RigRow label="CAB" file={status.ir} empty={status.speakerSim ? 'Built-in 4×12 speaker' : 'Off · optional for full-rig captures'}
      note={legacy ? 'Current rig keeps its original routing. Choose an amp source on the Amp page to change cabinet mode.' : values.CAB_MODE === 0 && fullRig ? 'Separate cabinet bypassed · full-rig capture' : values.AMP_SOURCE === 4 && values.CAB_MODE === 0 ? 'Natural DI has no guitar cabinet · choose External IR for an optional body IR' : values.CAB_MODE === 3 ? 'Cabinet bypassed' : values.CAB_MODE === 1 && !status.ir ? 'Load an external IR for this mode' : ''}>
      {remove('cab', status.ir, 'cabinet IR')}{load('cab', status.ir ? 'Change cabinet' : 'Load cabinet IR')}
    </RigRow>
  </div>;
}

function EQ() {
  const enabled = useToggle('EQ_ON');
  const setTone = values => Object.entries(values).forEach(([id, value]) => setParameter(id, value));
  return <div className="eq-pedal">
    <Group title="EQ pedal" status={<Switch id="EQ_ON" name="EQ enabled" />}>
      <Knob id="EQ_BODY" small muted={!enabled} /><Knob id="EQ_MUD" small muted={!enabled} />
      <Knob id="EQ_FOCUS" small muted={!enabled} /><Knob id="EQ_FIZZ" small muted={!enabled} />
    </Group>
    <div className="eq-actions">
      <button className="text-button" onClick={() => setTone({EQ_BODY: -2, EQ_MUD: -3, EQ_FOCUS: 1, EQ_FIZZ: -4, EQ_ON: 1})}>Smooth distortion</button>
      <button className="text-button quiet" onClick={() => setTone({EQ_BODY: 0, EQ_MUD: 0, EQ_FOCUS: 0, EQ_FIZZ: 0})}>Flat EQ</button>
      <small>After the cabinet · Body 120 Hz · Mud 350 Hz · Focus 1.2 kHz · Fizz 4.8 kHz</small>
    </div>
  </div>;
}

function Effects({ status }) {
  const resonance = useToggle('DYN_RES_ON');
  const sync = useToggle('DELAY_SYNC');
  return <div className="control-groups">
    <Group title="Chorus"><Knob id="CHORUS_MIX" small /><Knob id="CHORUS_RATE" small /><Knob id="CHORUS_DEPTH" small /></Group>
    <Group title="Delay" status={<Switch id="DELAY_SYNC" name="Delay tempo sync" />}><Knob id="DELAY_TIME" small muted={sync} /><Knob id="DELAY_MIX" small /><Knob id="DELAY_WIDTH" small /><Knob id="DELAY_FEEDBACK" small />
      {sync && <Choice id="DELAY_DIVISION" label="Delay division" options={['Quarter', 'Eighth', 'Dotted eighth', 'Sixteenth', 'Half', 'Whole']} />}
    </Group>
    <Group title="Reverb"><Choice id="REVERB_STYLE" label="Reverb voice" options={['Room', 'Chamber', 'Hall']} /><Knob id="REVERB_SIZE" small /><Knob id="REVERB_DAMP" small /><Knob id="REVERB_PREDELAY" small /><Knob id="MICRO_DELAY" small /></Group>
    <Group title="Character" status={resonance && status.dynResCut < -0.5 && <span className="telemetry-tag">{status.dynResCut.toFixed(1)} dB</span>}>
      <Knob id="DYN_RES_AMOUNT" enable="DYN_RES_ON" small /><Knob id="THICKEN_MIX" enable="THICKEN_ON" small /><Knob id="PIEZO_BLEND" enable="PIEZO_ON" small />
    </Group>
  </div>;
}

const views = { Input, Pedal, Amp, Cab, EQ, Effects };
const shortAmp = (clean, status) => clean ? 'Lumen clean'
  : status.model ? status.model.replace('APP-5153-Ivory-', '5150III ').replace(/\.nam$/i, '').replace(/-/g, ' ')
  : status.fallbackAmp ? 'Ferrum built-in' : 'No capture';
const stripExtension = name => name.replace(/\.(nam|wav)$/i, '');

// Signal chain: stages in order, each with its live level, then the selected stage's controls.
export default function Stages({ page, onPage, clean, native, status, onLoad, onRemove }) {
  const tabs = useRef([]);
  const fx = useParameters(['DELAY_MIX', 'REVERB_MIX', 'CHORUS_MIX', 'PEDAL_ON', 'GATE_ON', 'EQ_ON', 'AMP_SOURCE', 'CAB_MODE', 'CAPTURE_KIND']);
  const View = views[page] ?? Amp;
  const pedalOn = fx.PEDAL_ON >= .5 && Boolean(status.pedal) && (!clean || fx.AMP_SOURCE > 0);
  const gate = fx.GATE_ON < .5 ? 'Gate off' : !native ? 'Gate on' : status.gate > .1 ? 'Gate open' : 'Gate closed';
  const effects = [fx.CHORUS_MIX > 0 && `Chorus ${Math.round(fx.CHORUS_MIX)}%`, fx.DELAY_MIX > 0 && `Delay ${Math.round(fx.DELAY_MIX)}%`, fx.REVERB_MIX > 0 && `Space ${Math.round(fx.REVERB_MIX)}%`].filter(Boolean).join(' · ') || 'Dry';
  const fullRig = fx.AMP_SOURCE === 3 && fx.CAB_MODE === 0 && (fx.CAPTURE_KIND === 3 || (fx.CAPTURE_KIND === 0 && status.ampHasCab));
  const cabOff = fx.AMP_SOURCE > 0 && (fx.CAB_MODE === 3 || fullRig || (fx.AMP_SOURCE === 4 && fx.CAB_MODE === 0));
  const nodes = {
    Input: { detail: gate, value: status.input, lit: fx.GATE_ON >= .5 && native && status.gate > .1 },
    Pedal: { detail: status.pedal ? stripExtension(status.pedal) : 'Empty', value: status.postPedal, lit: pedalOn, off: !pedalOn },
    Amp: { detail: fx.AMP_SOURCE === 4 ? 'Natural DI' : fx.AMP_SOURCE === 2 ? 'Ferrum built-in' : shortAmp(clean, status), value: status.postAmp, lit: native },
    Cab: { detail: fullRig ? 'Included in capture' : cabOff ? 'Off' : clean && fx.AMP_SOURCE === 0 ? 'Clean rolloff' : fx.AMP_SOURCE > 0 && fx.CAB_MODE === 2 ? 'Built-in 4×12' : status.ir ? stripExtension(status.ir) : status.speakerSim ? 'Built-in 4×12' : 'Off', value: status.postCab, lit: native && !cabOff && !(clean && fx.AMP_SOURCE === 0) && (Boolean(status.ir) || status.speakerSim || fx.CAB_MODE === 2) },
    EQ: { detail: fx.EQ_ON >= .5 ? 'Tone shaping' : 'Bypassed', value: status.postEq, lit: fx.EQ_ON >= .5, off: fx.EQ_ON < .5 },
    Effects: { detail: effects, value: status.output, lit: effects !== 'Dry' },
  };
  const keyDown = e => {
    const i = pages.indexOf(page);
    const target = { ArrowRight: i + 1, ArrowLeft: i - 1, Home: 0, End: pages.length - 1 }[e.key];
    if (target === undefined) return;
    e.preventDefault();
    const next = (target + pages.length) % pages.length;
    onPage(pages[next]);
    tabs.current[next]?.focus();
  };
  const load = (type, text) => <button className="text-button" disabled={!native} onClick={() => onLoad(type)}>{text}<span aria-hidden="true"> ↗</span></button>;
  // Unloading returns the stage to its built-in (or bypassed) state.
  const remove = (type, file, name) => file && native && <button className="text-button quiet" aria-label={`Remove ${name}`} onClick={() => onRemove(type)}>Remove</button>;
  return <section className="rig" aria-label="Signal chain">
    <div className="chain" role="tablist" aria-label="Signal chain" onKeyDown={keyDown}>
      <span className="chain-end" aria-hidden="true">GUITAR</span>
      {pages.map((name, i) => {
        const node = nodes[name];
        return <button key={name} ref={el => { tabs.current[i] = el; }} role="tab" id={`tab-${name}`} aria-label={name}
          aria-selected={page === name} aria-controls="stage-panel" tabIndex={page === name ? 0 : -1} onClick={() => onPage(name)}
          className={`chain-node${node.lit ? ' lit' : ''}${node.off ? ' off' : ''}`}>
          <span className="node-head"><i className="node-led" aria-hidden="true" />{name}</span>
          <span className="node-detail">{node.detail}</span>
          <span className="node-level" aria-hidden="true"><i style={{ width: `${level(node.value) * 100}%` }} /></span>
        </button>;
      })}
      <span className="chain-end" aria-hidden="true">OUT</span>
    </div>
    <div className="stage-panel" role="tabpanel" id="stage-panel" aria-labelledby={`tab-${page}`}>
      <View clean={clean} native={native} status={status} load={load} remove={remove} />
    </div>
  </section>;
}
