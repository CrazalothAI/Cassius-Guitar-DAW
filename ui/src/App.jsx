import { useEffect, useState } from 'react';
import Knob from './components/Knob.jsx';
import { invoke, native } from './juce/bridge.js';
import { applyPreset, presets, restoreSnapshot, snapshotParameters, setParameter, useParameter } from './parameterState.js';
import cassianLogo from './assets/cassian-logo.png';

const voices = [
  {label:'Clean', preset:'Glass clean', note:'Clear attack. Room for every note.'},
  {label:'Ambient', preset:'Ambient clean', note:'Clean notes, wide repeats, longer trails.'},
  {label:'Piezo', preset:'Playing God nylon', note:'Acoustic body resonance & glassy piezo sparkle for electric pickups.'},
  {label:'Rock', preset:'80s rock', note:'Mid-forward drive with a little room to breathe.'},
  {label:'Lead', preset:'Singing lead', note:'Sustain and definition for expressive runs.'},
  {label:'Metal', preset:'Modern metalcore', note:'Controlled lows. Fast, deliberate stops.'},
  {label:'Thall', preset:'Thall chug', note:'Heavy low-end articulation, dynamic resonance cut & chug attack.'},
];

function Meter({label,value}) {
  const db = Math.max(-60, Math.min(0, value > 0 ? 20*Math.log10(value) : -60));
  return <div className="meter"><span>{label}</span><div className="meter-track" role="meter" aria-label={`${label} level`} aria-valuemin={-60} aria-valuemax={0} aria-valuenow={db}><i style={{width:`${(db+60)/60*100}%`}}/></div></div>;
}

export default function App() {
  const clean = useParameter('AMP_CLEAN') >= .5;
  const gateEnabled = useParameter('GATE_ON') >= .5;
  const pedalEnabled = useParameter('PEDAL_ON') >= .5;
  const dynResEnabled = useParameter('DYN_RES_ON') >= .5;
  const thickenEnabled = useParameter('THICKEN_ON') >= .5;
  const piezoEnabled = useParameter('PIEZO_ON') >= .5;

  const [selected, setSelected] = useState('');
  const [expanded, setExpanded] = useState(false);
  const [tunerOpen, setTunerOpen] = useState(false);
  const [page, setPage] = useState('Shape');
  const [compare, setCompare] = useState(null);
  const [compareSide, setCompareSide] = useState('A');
  const [status, setStatus] = useState({
    model:'', ir:'', pedal:'', input:0, prePedal:0, postPedal:0, postAmp:0, postCab:0, output:0,
    tunerActive:false, tunerNote:'—', tunerCents:0, tunerHz:0, dynResCut:0,
    message:native?'Connecting to audio engine…':'Browser preview · Open Cassian to play.'
  });

  useEffect(() => {
    if (!native) return;
    let active=true,timer;
    const poll=async()=>{
      try {const next=await invoke('getStatus');if(active&&next)setStatus(next);}
      catch {if(active)setStatus(s=>({...s,message:'Audio engine connection interrupted'}));}
      if(active)timer=setTimeout(poll,100);
    };
    poll();return()=>{active=false;clearTimeout(timer);};
  },[]);

  const chooseTone = async name => {
    applyPreset(name); setSelected(name);
    if(native && !presets[name]?.AMP_CLEAN) {
      try {await invoke('selectAmpVoice',name==='80s rock'?'Blue-I':'Red-I');}
      catch {setStatus(s=>({...s,message:'Could not switch amp voice. Your current capture is still active.'}));}
    }
  };

  const toggleCompare = () => {
    const current = snapshotParameters();
    if (!compare) {
      setCompare(current);
      setCompareSide('A');
      return;
    }
    restoreSnapshot(compare);
    setCompare(current);
    setCompareSide(side => side === 'A' ? 'B' : 'A');
  };

  const load=async type=>{
    try {await invoke(type==='amp'?'loadModel':type==='pedal'?'loadPedal':'loadIR');}
    catch {setStatus(s=>({...s,message:'Could not open the file picker. Please try again.'}));}
  };

  const activeVoice = selected ? voices.find(v=>v.preset===selected)?.label || (clean?'Clean':'Metal') : '';
  const description = voices.find(v=>v.label===activeVoice)?.note || 'Choose a starting point. Make it yours.';
  const ampName = clean ? 'Lumen · built-in clean' : status.model ? status.model.replace('APP-5153-Ivory-','EVH 5150III · ').replace(/\.nam$/i,'').replace(/-/g,' ') : 'Load your amp capture';

  return <div className={`app-shell streamlined ${clean?'clean':'metal'}`}>
    <header>
      <div className="brand"><img className="brand-logo" src={cassianLogo} alt=""/><h1>CASSIAN</h1></div>
      <div style={{display:'flex',alignItems:'center',gap:'8px'}}>
        <button className="compare-toggle" aria-label="A/B compare" onClick={toggleCompare}>{compare ? `A/B · ${compareSide}` : 'A/B'}</button>
        <button className="tuner-toggle" aria-pressed={tunerOpen} onClick={()=>setTunerOpen(!tunerOpen)}>
          {tunerOpen ? 'TUNER ON' : 'TUNER'}
        </button>
        <label className="preset">
          <select aria-label="Tone starting point" value="" onChange={e=>chooseTone(e.target.value)}>
            <option value="">More tones…</option>
            {Object.keys(presets).map(name=><option key={name}>{name}</option>)}
          </select>
        </label>
      </div>
      <span className="connection"><i/>{native?'LIVE':'PREVIEW'}</span>
    </header>
    <main>
      {status.inputClipped&&<div className="load-error" role="alert"><strong>Input is clipping</strong><span>Turn down the input gain on your interface.</span></div>}
      {status.message.startsWith('Load failed:')&&<div className="load-error" role="alert"><strong>Couldn’t load the file</strong><span>{status.message.slice(12).trim()}</span></div>}

      {tunerOpen && (
        <section className="tuner-container" aria-label="Chromatic Guitar Tuner">
          <div className="tuner-note-box">
            <span className="tuner-note">{status.tunerActive ? status.tunerNote : '—'}</span>
            <span className="tuner-freq">{status.tunerActive && status.tunerHz > 0 ? `${status.tunerHz.toFixed(1)} Hz` : 'Play a string'}</span>
          </div>
          <div className="tuner-gauge">
            <span className="tuner-sign">♭</span>
            <div className="tuner-track">
              <div className="tuner-center-mark" />
              <div
                className={`tuner-cursor ${Math.abs(status.tunerCents || 0) <= 3 ? 'in-tune' : ''}`}
                style={{ left: `${Math.max(0, Math.min(100, 50 + (status.tunerCents || 0)))}%` }}
              />
            </div>
            <span className="tuner-sign">♯</span>
            <span className="tuner-cents-val">
              {status.tunerActive ? `${status.tunerCents > 0 ? '+' : ''}${Math.round(status.tunerCents)}¢` : '0¢'}
            </span>
          </div>
        </section>
      )}

      <section className="voice-section"><nav className="tone-types" aria-label="Tone families">{voices.map(v=><button key={v.label} aria-pressed={activeVoice===v.label} onClick={()=>chooseTone(v.preset)}>{v.label}</button>)}</nav><p>{description}</p></section>
      <section className="amp-stage" aria-label="Amplifier"><div className="amp-handle"/><div className="amp-head"><div className="grille"><span className="corner tl"/><span className="corner tr"/><div className="tube-bank" aria-hidden="true">{[0,1,2,3,4,5].map(i=><span className="glass-tube" key={i}><i/></span>)}</div><img className="amp-logo" src={cassianLogo} alt="Cassian"/><div className="amp-series">{clean?'L U M E N':'F E R R U M'}<small>{clean?'CLEAN':'CAPTURE'}</small></div></div><div className="faceplate"><div className="input-jack"><i/><span>INPUT</span></div><div className="amp-controls">{['DRIVE_GAIN','AMP_BASS','AMP_MID','AMP_TREBLE','REVERB_MIX','MASTER_VOL'].map(id=><Knob key={id} id={id}/>)}</div><div className="power"><i/><span>ON</span></div></div><div className="amp-lower"/></div><div className="amp-feet"><i/><i/></div></section>
      <section className="capture-strip" aria-label="Amp source"><div><span className="source-dot"/><span className="source-name" title={status.model}>{ampName}</span></div><div className="live-meters"><Meter label="IN" value={status.input}/><Meter label="PRE" value={status.prePedal}/><Meter label="AMP" value={status.postAmp}/><Meter label="CAB" value={status.postCab}/><Meter label="OUT" value={status.output}/><span className="gate-summary">{gateEnabled?'Gate on':'Gate off'}</span></div></section>
      <section className="effects"><button className="drawer-toggle" aria-expanded={expanded} aria-controls="effects-panel" onClick={()=>setExpanded(!expanded)}><span>RIG & TONE</span><span>{expanded?'−':'+'}</span></button>
      {expanded&&<div id="effects-panel" className="detail-drawer"><nav className="detail-tabs" aria-label="Detailed controls">{['Shape','Thall','Piezo','Space','Rig'].map(name=><button key={name} aria-pressed={page===name} onClick={()=>setPage(name)}>{name}</button>)}</nav>
        {page==='Shape'&&<><div className="detail-knobs">{['INPUT_GAIN','GATE_THRESH','GATE_RELEASE',clean?'CLEAN_COMP':'TIGHT','HIGH_CUT','PRESENCE','AMP_OUT'].map(id=><Knob key={id} id={id} small/>)}</div><div className="detail-note"><button className="pedal-switch" aria-label="Noise gate enabled" aria-pressed={gateEnabled} onClick={()=>setParameter('GATE_ON',gateEnabled?0:1)}>{gateEnabled?'Gate on':'Gate off'}</button><span>{gateEnabled&&native?(status.gate>.1?'Open · ':'Closed · '):''}Longer release preserves sustained notes.</span></div></>}
        {page==='Thall'&&<><div className="detail-knobs thall-knobs"><Knob id="DYN_RES_AMOUNT" small /><Knob id="CHUG_ATTACK" small /><Knob id="THICKEN_MIX" small /></div><div className="detail-note"><button className="pedal-switch" aria-label="Dynamic resonance" aria-pressed={dynResEnabled} onClick={()=>setParameter('DYN_RES_ON',dynResEnabled?0:1)}>{dynResEnabled?'Resonance cut on':'Resonance cut off'}</button><button className="pedal-switch" aria-label="Sub-synthesis" aria-pressed={thickenEnabled} onClick={()=>setParameter('THICKEN_ON',thickenEnabled?0:1)}>{thickenEnabled?'Thicken sub on':'Thicken sub off'}</button>{dynResEnabled&&status.dynResCut<-0.5&&<span className="telemetry-tag">Dynamic Cut: {status.dynResCut.toFixed(1)} dB</span>}<span>Dynamic 200–400 Hz chug suppression & sub-octave synthesis for Drop E/Z tunings.</span></div></>}
        {page==='Piezo'&&<><div className="detail-knobs piezo-knobs"><Knob id="PIEZO_BLEND" small /></div><div className="detail-note"><button className="pedal-switch" aria-label="Piezo body simulation" aria-pressed={piezoEnabled} onClick={()=>setParameter('PIEZO_ON',piezoEnabled?0:1)}>{piezoEnabled?'Piezo on':'Piezo off'}</button><span>Acoustic body modal resonance and exciter sparkle for magnetic pickups (Tim Henson style).</span></div></>}
        {page==='Space'&&<><div className="detail-knobs space-knobs">{['DELAY_TIME','DELAY_MIX','DELAY_WIDTH','REVERB_SIZE','MICRO_DELAY'].map(id=><Knob key={id} id={id} small/>)}</div><p className="detail-note">Micro-delay (0–1.0 ms) adds sub-sample fractional phase separation for wide double-tracked guitars.</p></>}
        {page==='Rig'&&<div className="rig-list"><div><span>AMP</span><p>{status.model||'No capture loaded'}{clean&&<small>Bypassed · Lumen clean is active</small>}</p><button className="text-button" disabled={!native} onClick={()=>load('amp')}>{status.model?'Change amp':'Load amp model'} ↗</button></div><div><span>PEDAL</span><p>{status.pedal||'No pedal capture'}<small>Before the amp · bypassed on cleans</small></p><button className="pedal-switch" aria-label="Pedal enabled" disabled={clean||!status.pedal} aria-pressed={pedalEnabled&&!clean} onClick={()=>setParameter('PEDAL_ON',pedalEnabled?0:1)}>{pedalEnabled&&!clean?'On':'Off'}</button><button className="text-button" disabled={!native} onClick={()=>load('pedal')}>{status.pedal?'Change pedal':'Load pedal NAM'} ↗</button></div><div><span>CAB</span><p>{status.ir||'Off · optional for full-rig captures'}{clean&&<small>Bypassed · clean uses its own speaker rolloff</small>}</p><button className="text-button" disabled={!native||clean} onClick={()=>load('cab')}>{status.ir?'Change cabinet':'Load cabinet IR'} ↗</button></div></div>}
      </div>}
      </section>
    </main>
    <footer><span role="status">{!native?status.message:status.message.startsWith('Load failed:')||status.message.includes('bypassed:')?status.message:clean?'Clean ready':status.model?'Rig ready':status.message}</span><span>{status.sampleRate?`${(status.sampleRate/1000).toFixed(1)} kHz · ${status.bufferSize||'—'} samples${expanded?` · DSP ${Math.round(status.cpu||0)}% · ${status.overruns||0} overruns`:''}`:'NO AUDIO IN PREVIEW'}</span></footer>
  </div>;
}
