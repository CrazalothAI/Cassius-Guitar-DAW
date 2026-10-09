export function routeGuidance(status, native) {
  if (!native) return {kind: 'preview', title: 'Open Cassian to connect your guitar', detail: 'This browser previews the editor. Audio routing and recording are available in the installed app.'};
  if (status.deviceSettingsAvailable === false) return {kind: 'host', title: 'Your DAW controls the audio route', detail: 'Choose your interface and guitar input in the host, then enable monitoring on the Cassian track.'};
  if (status.deviceSettingsAvailable !== true) return {kind: 'unknown', title: 'Waiting for audio route information', detail: 'The engine has not reported its routing controls yet.'};
  const device = status.audioDevice;
  if (!device) return {kind: 'unknown', title: 'Audio route is not reported yet', detail: 'Open Audio settings to inspect the interface, input, and playback outputs.'};
  const invalidInput = Number.isInteger(status.selectedInput) && (status.selectedInput < 0 || (status.inputChannels?.length > 0 && status.selectedInput >= status.inputChannels.length));
  if (!device.inputDevice || !device.outputDevice || invalidInput) return {kind: 'route', title: 'Choose your guitar input and playback outputs', detail: 'Select the connected interface and the physical channel your guitar uses in Audio settings.'};
  if (device.monitoring === false) return {kind: 'muted', title: 'Guitar input monitoring is off', detail: 'Enable Monitor guitar input below when you are ready to listen through Cassian.'};
  if (status.audioProcessing === false) return {kind: 'inactive', title: 'The audio callback is inactive', detail: 'Check the selected driver and playback outputs in Audio settings.'};
  if (status.inputClipped) return {kind: 'clipping', title: 'Lower the interface input gain', detail: 'Clipping is detected before the amp. Reduce hardware input gain, then run the input check.'};
  if (status.dropoutRecent || status.overrunRecent) return {kind: 'dropout', title: 'Try a larger audio buffer', detail: 'A recent audio deadline was missed. Raise the buffer in Audio settings and try the same phrase again.'};
  const input = Number.isInteger(status.selectedInput) && status.selectedInput >= 0 ? `Input ${status.selectedInput + 1}` : 'Input not reported';
  return {kind: 'route', title: `${input} · ${device.inputDevice} → ${device.outputDevice}`, detail: `${device.monitoring === true ? 'Monitoring on' : 'Monitoring not reported'} · ${status.audioProcessing === true ? 'Audio callback active' : 'Callback activity not reported'}. Play a phrase, check input level, and raise Master gradually.`};
}
export default function SetupRoute({status, native, onNavigate}) {
  const guidance = routeGuidance(status, native);
  return <section className={`setup-route ${guidance.kind}`} aria-label="Current audio route">
    <span className="setup-eyebrow">YOUR NEXT STEP</span><h4>{guidance.title}</h4><p>{guidance.detail}</p>
    {onNavigate && <nav aria-label="Setup workflow"><button onClick={() => onNavigate('Tone')}>Choose a head in Tone</button><button onClick={() => onNavigate('Practice')}>{status.practice?.recordMode > 0 ? 'Continue recording in Practice' : 'Record in Practice'}</button><button onClick={() => onNavigate('Takes')}>Review &amp; export in Takes</button></nav>}
  </section>;
}
