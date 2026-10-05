import Knob from './Knob.jsx';
import Tuner from './Tuner.jsx';
import { useParameter } from '../parameterState.js';
import { ampIdentity } from '../ampIdentity.js';
import logo from '../assets/cassian-logo-192.png';
export default function CompactAmp({clean, status, tunerOpen}) {
  const source = Math.round(useParameter('AMP_SOURCE'));
  return <section className="compact-amp" aria-label="Compact amplifier">
    <img src={logo} alt=""/><div className="compact-amp-name"><span>AMPLIFIER</span><strong title={ampIdentity(source, clean, status)}>{ampIdentity(source, clean, status)}</strong></div>
    <Knob id="DRIVE_GAIN" small/><Knob id="MASTER_VOL" small/>
    {tunerOpen && <Tuner status={status}/>}
  </section>;
}
