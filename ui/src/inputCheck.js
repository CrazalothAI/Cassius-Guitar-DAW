// Uses the UI's sampled raw-input peaks, before Input gain and the tone chain.
// This is a setup aid, not capture dBu calibration or a continuous peak recorder.
export const CHECK_MS = 8000;
export const inputRoute = status => JSON.stringify([
  status.deviceSettingsAvailable, status.selectedInput, status.sampleRate, status.bufferSize,
  status.audioDevice?.driver, status.audioDevice?.inputDevice, status.audioDevice?.outputDevice,
  status.audioDevice?.monitoring, status.audioDevice?.activeInputs, status.audioDevice?.activeOutputs,
]);
export function checkUnavailable(status, native) {
  if (!native) return 'Open the installed app to check your guitar input.';
  if (status.practice?.recordMode > 0) return 'Stop recording before checking or changing input trim.';
  if (status.review?.playing) return 'Stop take playback before checking your live guitar.';
  if (status.rigLoading) return 'Wait for the rig to finish loading.';
  if (status.audioProcessing !== true || !(status.sampleRate > 0)) return 'Audio is stopped. Open audio settings or start audio in your DAW.';
  if (status.message === 'Audio engine connection interrupted') return 'The audio engine connection is interrupted. Reopen the editor.';
  if (status.deviceSettingsAvailable) {
    if (!status.audioDevice || !(status.audioDevice.activeInputs > 0) || !(status.audioDevice.activeOutputs > 0)) return 'Choose a guitar input and playback outputs in Audio settings.';
    if (!status.audioDevice.monitoring) return 'Turn on Monitor guitar input to check your signal.';
  }
  return '';
}
export function beginCheck(status, now) {
  return {route: inputRoute(status), started: now, lastSeen: now, peak: 0, samples: 0, clipped: false, invalid: false, interrupted: false, overruns: status.overruns || 0, dropouts: status.dropouts ?? -1};
}
export function sampleCheck(check, status, now) {
  check.lastSeen = now; check.samples++;
  if (!Number.isFinite(status.input) || status.input < 0) check.invalid = true;
  else check.peak = Math.max(check.peak, status.input);
  check.clipped ||= !!status.inputClipped || status.input >= .995;
  check.interrupted ||= !!status.overrunRecent || !!status.dropoutRecent || (status.overruns || 0) !== check.overruns || (check.dropouts >= 0 && (status.dropouts ?? -1) !== check.dropouts);
}
export function finishCheck(check, now) {
  if (check.samples < 8 || now - check.lastSeen > 1500 || check.invalid) return {...check, kind: 'unavailable', text: 'Not enough fresh input readings. Check the audio connection and try again.'};
  if (check.clipped) return {...check, kind: 'clipped', text: 'Clipping was detected. Lower the gain on your interface, then run the check again. Software trim cannot repair clipped input.'};
  if (check.interrupted) return {...check, kind: 'interrupted', text: 'An audio interruption was reported during the check. Try a larger buffer, then check again.'};
  if (check.peak < .0001) return {...check, kind: 'silent', text: 'No useful signal was sampled. Check your cable, selected input and interface instrument setting, then play during the check.'};
  const db = 20 * Math.log10(check.peak);
  if (db < -40) return {...check, db, kind: 'weak', text: 'The sampled signal is very low. Check the interface gain and guitar volume, then play your hardest notes again.'};
  const trim = Math.round(Math.max(-12, Math.min(12, -12 - db)) * 10) / 10;
  return {...check, db, trim, kind: 'measured', text: 'Input sampled. Listen after applying the suggestion and repeat the check if you change the interface gain.'};
}
