import { setParameter, useToggle } from '../parameterState.js';
// On/off for a boolean parameter. The accessible name stays fixed; the LED and state text follow the value.
export default function Switch({ id, name, disabled = false, forcedOff = false }) {
  const enabled = useToggle(id), on = enabled && !forcedOff;
  return <button className="switch" aria-label={name} aria-pressed={on} disabled={disabled} onClick={() => setParameter(id, enabled ? 0 : 1)}>
    <i aria-hidden="true" />{on ? 'On' : 'Off'}
  </button>;
}
