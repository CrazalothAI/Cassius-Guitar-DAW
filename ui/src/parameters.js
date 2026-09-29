// Mirrors Source/params/ParameterIDs.h (ids, ranges, defaults and order); parameters.test.js enforces it.
// `step` is the finest UI resolution. Labels are the panel names and may differ from the host names.
export const parameters = [
  ['INPUT_GAIN', 'Input', -24, 24, 0, 'dB', 0.1],
  ['GATE_THRESH', 'Threshold', -80, 0, -60, 'dB', 0.1],
  ['DRIVE_GAIN', 'Drive', 0, 24, 0, 'dB', 0.1],
  ['AMP_BASS', 'Bass', -12, 12, 0, 'dB', 0.1],
  ['AMP_MID', 'Middle', -12, 12, 0, 'dB', 0.1],
  ['AMP_TREBLE', 'Treble', -12, 12, 0, 'dB', 0.1],
  ['AMP_OUT', 'Output', -24, 12, 0, 'dB', 0.1],
  ['DELAY_TIME', 'Time', 40, 1000, 320, 'ms', 1],
  ['DELAY_MIX', 'Mix', 0, 100, 0, '%', 1],
  ['REVERB_MIX', 'Space', 0, 100, 12, '%', 1],
  ['MASTER_VOL', 'Master', -60, 6, -12, 'dB', 0.1],
  ['AMP_CLEAN', 'Clean channel', 0, 1, 0, '', 1],
  ['TIGHT', 'Tight', 20, 180, 20, 'Hz', 1],
  ['PRESENCE', 'Presence', -6, 6, 0, 'dB', 0.1],
  ['CLEAN_COMP', 'Compression', 0, 100, 35, '%', 1],
  ['HIGH_CUT', 'High cut', 3000, 20000, 20000, 'Hz', 10],
  ['DELAY_WIDTH', 'Width', 0, 100, 0, '%', 1],
  ['REVERB_SIZE', 'Room', 0, 100, 60, '%', 1],
  ['GATE_ON', 'Gate enabled', 0, 1, 1, '', 1],
  ['GATE_RELEASE', 'Release', 40, 500, 140, 'ms', 1],
  ['PEDAL_ON', 'Pedal enabled', 0, 1, 0, '', 1],
  ['DYN_RES_ON', 'Dynamic resonance', 0, 1, 0, '', 1],
  ['DYN_RES_AMOUNT', 'Chug cut', 0, 100, 50, '%', 1],
  ['CHUG_ATTACK', 'Pick attack', 0, 100, 0, '%', 1],
  ['THICKEN_ON', 'Sub-synthesis', 0, 1, 0, '', 1],
  ['THICKEN_MIX', 'Sub mix', 0, 100, 30, '%', 1],
  ['PIEZO_ON', 'Piezo resonator', 0, 1, 0, '', 1],
  ['PIEZO_BLEND', 'Piezo sparkle', 0, 100, 40, '%', 1],
  ['MICRO_DELAY', 'Micro-delay', 0, 1.0, 0.0, 'ms', 0.01],
].map(([id, label, min, max, initial, unit, step]) => ({ id, label, min, max, initial, unit, step }));
export const byId = Object.fromEntries(parameters.map(p => [p.id, p]));

// The DSP treats these range ends as bypass: Tight blends in above 20 Hz, High cut fades out at 20 kHz.
const offAt = { TIGHT: 'min', HIGH_CUT: 'max' };

export const clamp = (p, value) => Math.min(p.max, Math.max(p.min, value));
export const toFraction = (p, value) => (clamp(p, value) - p.min) / (p.max - p.min);
export const fromFraction = (p, fraction) => p.min + Math.min(1, Math.max(0, fraction)) * (p.max - p.min);
const decimals = p => Math.max(0, -Math.floor(Math.log10(p.step) + 1e-9));
export const snap = (p, value) => Number(clamp(p, p.min + Math.round((value - p.min) / p.step) * p.step).toFixed(decimals(p)));
// Arc origin: centre-detented controls (EQ, trims) fill outward from 0 dB instead of from the minimum.
export const origin = p => (p.min < 0 && p.max > 0 && p.initial === 0 ? 0 : p.min);

export function formatValue(p, value) {
  const off = offAt[p.id];
  if ((off === 'min' && value <= p.min + p.step / 2) || (off === 'max' && value >= p.max - p.step / 2)) return { text: 'Off', unit: '' };
  if (p.unit === 'Hz' && value >= 1000) return { text: (value / 1000).toFixed(1), unit: 'kHz' };
  const shown = Math.abs(value) < p.step / 2 ? 0 : value; // never "-0.0"
  const text = shown.toFixed(decimals(p));
  return { text: p.unit === 'dB' && p.min < 0 && shown > 0 ? `+${text}` : text, unit: p.unit };
}
