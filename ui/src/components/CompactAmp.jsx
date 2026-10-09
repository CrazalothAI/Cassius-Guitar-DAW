import Knob from './Knob.jsx';
import Tuner from './Tuner.jsx';
import { useParameter } from '../parameterState.js';
import { ampIdentity } from '../ampIdentity.js';
import logo from '../assets/cassian-logo-192.png';
export default function CompactAmp({clean, status, tunerOpen, head, driveControl = 'DRIVE_GAIN'}) {
  const source = Math.round(useParameter('AMP_SOURCE'));
  return <section className={`compact-amp head-${head.id}`} aria-label="Compact amplifier">
    <img className="compact-head-art" src={head.art} alt=""/><img className="compact-logo" src={logo} alt=""/><div className="compact-amp-name"><span>CASSIAN · {head.name.toUpperCase()}</span><strong title={ampIdentity(source, clean, status)}>{ampIdentity(source, clean, status)}</strong></div>
    <Knob key={driveControl} id={driveControl} small/><Knob id="MASTER_VOL" small/>
    {tunerOpen && <Tuner status={status}/>}
  </section>;
}
