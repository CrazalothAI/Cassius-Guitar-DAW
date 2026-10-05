import Knob from './Knob.jsx';
import Tuner from './Tuner.jsx';
import { setParameter, useParameter } from '../parameterState.js';
import cassianLogo from '../assets/cassian-logo-192.png'; // shown at 34-46 px; the full-size original stays in assets
import { ampIdentity } from '../ampIdentity.js';

const controls = ['DRIVE_GAIN', 'AMP_BASS', 'AMP_MID', 'AMP_TREBLE', 'REVERB_MIX', 'MASTER_VOL'];

// The amp: a boutique-style head. Decorative hardware never intercepts the controls.
export default function AmpHead({ clean, tunerOpen, status }) {
  const source = Math.round(useParameter('AMP_SOURCE'));
  return <section className="amp" aria-label="Amplifier">
    <div className="amp-handle" aria-hidden="true" />
    <div className="amp-cab">
      <i className="cab-corner tl" /><i className="cab-corner tr" /><i className="cab-corner bl" /><i className="cab-corner br" />
      <div className="grille">
        <div className="tube-glow" aria-hidden="true">{[0, 1, 2, 3].map(i => <span key={i} />)}</div>
        <div className="badge">
          <img src={cassianLogo} alt="" />
          <span className="badge-word">Cassian</span>
          <span className="badge-model" title={ampIdentity(source, clean, status)}>{ampIdentity(source, clean, status)}</span>
        </div>
        {tunerOpen && <Tuner status={status} />}
      </div>
      <div className="faceplate">
        <div className="plate-side">
          <span className="jack" aria-hidden="true"><i /></span>
          <span className="plate-caption" aria-hidden="true">INPUT</span>
        </div>
        <button className="channel" aria-label="Channel" aria-pressed={!clean} disabled={source === 3 || source === 4} onClick={() => { setParameter('AMP_SOURCE', source > 0 ? clean ? 2 : 1 : 0); setParameter('AMP_CLEAN', clean ? 0 : 1); }}
          title="Switch between the clean and high-gain channels">
          <span className={clean ? 'lit' : ''}><i />CLEAN</span>
          <span className="lever" aria-hidden="true" />
          <span className={clean ? '' : 'lit'}><i />LEAD</span>
        </button>
        <div className="amp-controls">{controls.map(id => <Knob key={id} id={id} />)}</div>
        <div className="plate-side power" aria-hidden="true">
          <span className="jewel" />
          <span className="plate-caption">POWER</span>
        </div>
      </div>
    </div>
    <div className="amp-feet" aria-hidden="true"><i /><i /></div>
  </section>;
}
