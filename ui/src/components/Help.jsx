import { useState } from 'react';
import { invoke, native } from '../juce/bridge.js';
import { releaseLabel } from '../releaseLabel.js';
import InputCheck from './InputCheck.jsx';
import StarterTour from './StarterTour.jsx';

const repository = 'https://github.com/CrazalothAI/Cassius-Guitar-DAW';
const links = [['guide', 'User guide', `${repository}/blob/main/docs/USER-GUIDE.md`], ['support', 'Report an issue', `${repository}/issues`], ['source', 'Source code', repository], ['license', 'License', `${repository}/blob/main/LICENSE.txt`]];
export function supportReport(status) {
  const device = status.audioDevice;
  // Explicit whitelist: no paths, recordings, imported-file names or MIDI IDs.
  return [
    `Cassian ${releaseLabel(status,native)}`,
    `Mode: ${!native ? 'Browser preview' : status.deviceSettingsAvailable ? 'Standalone' : 'DAW plugin'}`,
    `Driver: ${device?.driver || 'Controlled by host / unavailable'}`,
    `Input device: ${device?.inputDevice || 'Controlled by host / unavailable'}`,
    `Output device: ${device?.outputDevice || 'Controlled by host / unavailable'}`,
    `Input channel: ${Number.isInteger(status.selectedInput) && status.selectedInput >= 0 ? status.selectedInput + 1 : 'Controlled by host / unavailable'}`,
    `Monitoring: ${device ? device.monitoring ? 'On' : 'Off' : 'Controlled by host / unavailable'}`,
    `Audio callback: ${status.audioProcessing === true ? 'Recently active' : status.audioProcessing === false ? 'Stopped / inactive' : 'Unavailable'}`,
    `Sample rate: ${status.sampleRate || 'Unavailable'} Hz`, `Buffer: ${status.bufferSize || 'Unavailable'} samples`,
    `DSP load: ${Math.round(status.cpu || 0)}%`, `Processing overruns: ${status.overruns || 0}`,
    `Device dropouts: ${status.dropouts >= 0 ? status.dropouts : 'Not reported by driver'}`,
    `Take review: ${status.review ? status.review.streaming ? 'Streaming WAV' : 'Decoded / idle' : 'Unavailable'}; buffering ${status.review?.buffering ? 'Yes' : 'No'}; buffer waits ${status.review?.reviewUnderruns || 0} (includes uncached seeks)`,
    `Review loop: ${status.review?.loop ? 'On' : 'Off'}; start prefetch ${status.review?.loopPrefetchReady === false ? 'Preparing' : 'Ready / not needed'}; cache ${status.review?.reviewCacheBytes || 0} bytes`,
    `Board: ${status.board?.serial ? 'Serial' : 'Compatibility'}; ${status.board?.blocks?.length || 0} serial pedals`,
    `Input clipping: ${status.inputClipped ? 'Detected' : 'Not detected'}`,
    'Add your OS, interface driver version, steps to reproduce, and expected/actual result before submitting.'
  ].join('\n');
}
export default function Help({status, onError, onChooseRig}) {
  const [busy, setBusy] = useState(false), [copied, setCopied] = useState(false);
  const action = async (method, ...args) => {
    if (busy) return false; setBusy(true); setCopied(false);
    try { const failure = await invoke(method, ...args); if (failure) throw new Error(String(failure)); return true; }
    catch (error) { onError({title: 'Help & setup', text: error.message || 'Could not complete this action.'}); return false; }
    finally { setBusy(false); }
  };
  const device = status.audioDevice, report = supportReport(status), recording = status.practice?.recordMode > 0;
  return <section className="help-panel" aria-label="Help and setup">
    <p className="practice-note">Cassian {releaseLabel(status,native)} · Guitar workstation · Crazaloth</p>
    <h3>Get your first sound</h3>
    <ol>
      <li>Connect your guitar and headphones to the audio interface.</li>
      <li>{status.deviceSettingsAvailable ? 'Choose the interface, input and playback outputs in Audio settings.' : native ? 'Select your interface and guitar input in your DAW, then enable monitoring on its Cassian track.' : 'Open the installed Cassian app to connect audio. This browser is a preview.'}</li>
      <li>Choose Prism Clean, Velvet Lead or Iron Rhythm under Cassian built-in rigs. All 22 built-in rigs work without extra files.</li>
      <li>Keep the interface input below clipping, raise Master gradually, and use playback monitoring to avoid doubling the dry guitar.</li>
      <li>Use Save as to keep your complete rig. Open Practice to record, then Takes to review and export audio.</li>
    </ol>
    {native && status.deviceSettingsAvailable && <div className="help-audio">
      <button disabled={busy || recording} onClick={() => action('showAudioSettings')}>Open audio settings</button>
      {status.inputChannels?.length > 0 && <label>Guitar input <select aria-label="Setup guitar input" disabled={busy || recording} value={status.selectedInput ?? -1} onChange={e => action('setInputChannel', Number(e.target.value))}><option value={-1} disabled>No input</option>{status.inputChannels.map((name, index) => <option key={index} value={index}>{index + 1} · {name}</option>)}</select></label>}
      <label><input type="checkbox" aria-label="Monitor guitar input" checked={!!device?.monitoring} disabled={busy || !device || recording} onChange={e => action('setInputMonitoring', e.target.checked)}/> Monitor guitar input</label>
      {device && <p className="practice-note">{device.inputDevice || 'No input device'} → {device.outputDevice || 'No output device'} · {device.monitoring ? 'Monitoring on' : 'Input muted'}</p>}
    </div>}
    <InputCheck status={status}/>
    <StarterTour status={status} onChooseRig={onChooseRig} onError={onError}/>
    <h3>Playing and recording</h3>
    <p>Mix helps your guitar sit over backing tracks. Browser/YouTube audio stays outside Cassian recordings. Load a track in Practice to record its backing stem and export a mixed soundtrack from Takes. Exports are 48 kHz / 24-bit stereo WAVs.</p>
    <p>Board supports independent pedals before and after the amp, duplication, reorder and Undo/Redo. Edit scenes to organize variations; Performance connects MIDI controls.</p>
    <p>Practice sets organize clean, rhythm and lead exercises with time and BPM targets. Start a local timer, pause breaks and finish with notes. History shows finished-session progress and links exact recording versions; opening a link does not start playback. Export sets/history for another PC and use personal recovery’s linked-history copy for recovered recordings. No practice data is uploaded.</p>
    <p>Library → Backup &amp; recovery creates verified personal rig/take archives and restores copies while preserving existing work. The current archive limit is 32 GiB. Save your DAW projects and MIDI/device preferences separately.</p>
    <details><summary>No sound or crackles?</summary><p>Check the selected input, monitoring, interface output and Master. If the input clips, lower the interface gain. For reported dropouts, try a larger buffer in Audio settings or your DAW. Capture-based recipes need their listed files; the built-in rigs always work without captures.</p></details>
    <details><summary>Support report</summary><p>Review this report before copying it. It includes device names and audio settings. Recordings, file paths and imported sound names are excluded. Nothing is submitted automatically.</p><textarea aria-label="Support report" readOnly value={report} rows={10}/><button disabled={!native || busy} onClick={async () => { if (await action('copySupportReport', report)) setCopied(true); }}>Copy support report</button>{copied && <p role="status">Copied. Paste it into your issue with steps to reproduce.</p>}</details>
    <nav aria-label="Help links">{links.map(([key,label,url]) => <a key={key} href={url} target="_blank" rel="noreferrer" onClick={e => { if(native){e.preventDefault(); action('openHelpLink',key);} }}>{label}</a>)}</nav>
    <p className="practice-note">Open-source under AGPL-3.0-or-later. Paid installers and support do not limit your license rights. Third-party components keep their licenses; extra sound files need their own permissions.</p>
  </section>;
}
