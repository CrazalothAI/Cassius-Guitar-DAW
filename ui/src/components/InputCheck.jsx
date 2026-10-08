import { useEffect, useRef, useState } from 'react';
import { native } from '../juce/bridge.js';
import { setParameter, useParameter } from '../parameterState.js';
import { CHECK_MS, inputRoute, checkUnavailable, beginCheck, sampleCheck, finishCheck } from '../inputCheck.js';
import { Meter } from './Stages.jsx';

const signed = db => `${db > 0 ? '+' : ''}${db.toFixed(1)} dB`;
export default function InputCheck({status}) {
  const currentTrim = useParameter('INPUT_GAIN');
  const check = useRef(null), latest = useRef(status), lastReading = useRef(Date.now());
  const [remaining, setRemaining] = useState(null), [result, setResult] = useState(null), [message, setMessage] = useState('');
  latest.current = status;
  const route = inputRoute(status), unavailable = checkUnavailable(status, native);
  useEffect(() => {
    lastReading.current = Date.now();
    if (check.current) sampleCheck(check.current, status, Date.now());
  }, [status]);
  useEffect(() => {
    if (check.current && (check.current.route !== route || unavailable)) {
      check.current = null; setRemaining(null); setResult(null);
      setMessage(unavailable || 'Audio settings changed. Run a new check for this input.');
    }
    if (result && (result.route !== route || unavailable)) {
      setResult(null); setMessage(unavailable || 'Audio settings changed. Run a new check for this input.');
    }
  }, [route, unavailable]);
  useEffect(() => {
    const timer = setInterval(() => {
      const active = check.current;
      if (!active) return;
      const now = Date.now(), left = Math.max(0, Math.ceil((CHECK_MS - (now - active.started)) / 1000));
      setRemaining(left);
      if (!left) { check.current = null; setRemaining(null); setResult(finishCheck(active, now)); }
    }, 200);
    return () => { clearInterval(timer); check.current = null; };
  }, []);
  const start = () => {
    if (check.current || checkUnavailable(latest.current, native)) return;
    const next = beginCheck(latest.current, Date.now()); sampleCheck(next, latest.current, Date.now());
    check.current = next; setResult(null); setMessage(''); setRemaining(CHECK_MS / 1000);
  };
  const validResult = result && result.route === route;
  const apply = () => {
    if (!validResult || result.kind !== 'measured' || checkUnavailable(latest.current, native) || inputRoute(latest.current) !== result.route) return;
    if (Date.now() - lastReading.current > 1500) { setResult(null); setMessage('Live input readings stopped. Run a new check after reconnecting audio.'); return; }
    setParameter('INPUT_GAIN', result.trim);
    setMessage(`Input trim set to ${signed(result.trim)}. Listen to your rig and adjust to taste.`);
  };
  return <section className="input-check" aria-label="Guided input check">
    <h3>Check your guitar input</h3>
    <p>Play your hardest chords and notes for eight seconds. Keep guitar volume where you normally play. Start with a comfortable headphone level.</p>
    <div className="input-check-controls"><Meter label="RAW INPUT" value={Number.isFinite(status.input) ? status.input : 0}/><span>Current Input trim: {signed(currentTrim)}</span>
      <button disabled={!!unavailable || remaining !== null} onClick={start}>{remaining !== null ? `Listening · ${remaining}s` : 'Check input for 8 seconds'}</button>
      {remaining !== null && <button onClick={() => { check.current = null; setRemaining(null); setMessage('Input check cancelled. Settings were not changed.'); }}>Cancel input check</button>}
    </div>
    {unavailable && <p className="practice-note">{unavailable}</p>}
    {validResult && <div className="input-check-result" role="status"><p>{result.text}</p>
      {result.peak > 0 && <p>Highest sampled raw peak: {(20 * Math.log10(result.peak)).toFixed(1)} dBFS</p>}
      {result.kind === 'measured' && <><p>Suggested Input trim: {signed(result.trim)} · aiming near −12 dBFS, limited to ±12 dB.</p><button disabled={!!unavailable} onClick={apply}>Apply suggested input trim</button></>}
    </div>}
    {result && !validResult && <p className="practice-note">Audio settings changed. Run a new check for this input.</p>}
    {message && <p role="status">{message}</p>}
    <p className="practice-note">Readings are sampled from the raw input meter; short transients can be missed. Only Apply changes Input trim. This does not calibrate a capture’s dBu level or adjust Master, gate, pedals or amp gain.</p>
  </section>;
}
