import Knob from './Knob.jsx';
import Tuner from './Tuner.jsx';
import { setParameter, useParameter } from '../parameterState.js';
import cassianLogo from '../assets/cassian-logo-192.png'; // shown at 34-46 px; the full-size original stays in assets
import { ampIdentity } from '../ampIdentity.js';

const toneControls = ['AMP_BASS', 'AMP_MID', 'AMP_TREBLE'];

// The amp: a boutique-style head. Decorative hardware never intercepts the controls.
export default function AmpHead({ clean, tunerOpen, status, head, driveControl = 'DRIVE_GAIN', spaceControl = 'REVERB_MIX' }) {
  const source = Math.round(useParameter('AMP_SOURCE'));
  return <section className={`amp head-${head.id}`} aria-label="Amplifier">
    <div className="amp-cab">
      <div className="grille">
        <img className="head-art" src={head.art} alt="" fetchPriority="high"/>
        <div className="badge">
          <span className="badge-maker">CASSIAN</span>
          <span className="badge-word">{head.name}</span>
          <span className="badge-model">{head.voice} collection</span>
        </div>
        {tunerOpen && <Tuner status={status} />}
      </div>
      <div className="faceplate">
        <div className="plate-brand"><img src={cassianLogo} alt=""/><span>CASSIAN<small>{head.name}</small></span></div>
        <button className="channel" aria-label="Channel" aria-pressed={!clean} disabled={source === 3 || source === 4} onClick={() => { setParameter('AMP_SOURCE', source > 0 ? clean ? 2 : 1 : 0); setParameter('AMP_CLEAN', clean ? 0 : 1); }}
          title="Switch between the clean and high-gain channels">
          <span className={clean ? 'lit' : ''}><i />CLEAN</span>
          <span className="lever" aria-hidden="true" />
          <span className={clean ? '' : 'lit'}><i />LEAD</span>
        </button>
        <div className="amp-controls">{[driveControl, ...toneControls, spaceControl, 'MASTER_VOL'].map(id => <Knob key={id} id={id} />)}</div>
        <div className="plate-side power" aria-hidden="true">
          <span className="jewel" />
          <span className="plate-caption">POWER</span>
        </div>
      </div>
    </div>
    <div className="amp-detail"><span>{head.finish}</span><strong title={ampIdentity(source, clean, status)}>{ampIdentity(source, clean, status)}</strong><span>AMPLIFIER STUDIO</span></div>
  </section>;
}
