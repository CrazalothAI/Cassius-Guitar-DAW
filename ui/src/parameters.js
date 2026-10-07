// Mirrors Source/params/ParameterIDs.h (ids, ranges, defaults and order); parameters.test.js enforces it.
// `step` is the finest UI resolution; `centre` (0 = linear) is the value at mid-travel, as in
// JUCE's setSkewForCentre. Labels are the panel names and may differ from the host names.
export const parameters = [
  ['INPUT_GAIN', 'Input', -24, 24, 0, 'dB', 0.1],
  ['GATE_THRESH', 'Threshold', -80, 0, -60, 'dB', 0.1],
  ['DRIVE_GAIN', 'Drive', 0, 24, 0, 'dB', 0.1],
  ['AMP_BASS', 'Bass', -12, 12, 0, 'dB', 0.1],
  ['AMP_MID', 'Middle', -12, 12, 0, 'dB', 0.1],
  ['AMP_TREBLE', 'Treble', -12, 12, 0, 'dB', 0.1],
  ['AMP_OUT', 'Output', -24, 12, 0, 'dB', 0.1],
  ['DELAY_TIME', 'Time', 40, 1000, 320, 'ms', 1, 300],
  ['DELAY_MIX', 'Mix', 0, 100, 0, '%', 1],
  ['REVERB_MIX', 'Space', 0, 100, 12, '%', 1],
  ['MASTER_VOL', 'Master', -60, 6, -12, 'dB', 0.1],
  ['AMP_CLEAN', 'Clean channel', 0, 1, 0, '', 1],
  ['TIGHT', 'Tight', 20, 180, 20, 'Hz', 1, 70],
  ['PRESENCE', 'Presence', -6, 6, 0, 'dB', 0.1],
  ['CLEAN_COMP', 'Compression', 0, 100, 35, '%', 1],
  ['HIGH_CUT', 'High cut', 3000, 20000, 20000, 'Hz', 10, 8000],
  ['DELAY_WIDTH', 'Width', 0, 100, 0, '%', 1],
  ['REVERB_SIZE', 'Room', 0, 100, 60, '%', 1],
  ['GATE_ON', 'Gate enabled', 0, 1, 1, '', 1],
  ['GATE_RELEASE', 'Release', 40, 500, 140, 'ms', 1, 150],
  ['PEDAL_ON', 'Pedal enabled', 0, 1, 0, '', 1],
  ['DYN_RES_ON', 'Dynamic resonance', 0, 1, 0, '', 1],
  ['DYN_RES_AMOUNT', 'Chug cut', 0, 100, 50, '%', 1],
  ['CHUG_ATTACK', 'Pick attack', 0, 100, 0, '%', 1],
  ['THICKEN_ON', 'Sub-synthesis', 0, 1, 0, '', 1],
  ['THICKEN_MIX', 'Sub', 0, 100, 30, '%', 1],
  ['PIEZO_ON', 'Piezo resonator', 0, 1, 0, '', 1],
  ['PIEZO_BLEND', 'Piezo', 0, 100, 40, '%', 1],
  ['MICRO_DELAY', 'Micro-delay', 0, 1.0, 0.0, 'ms', 0.01],
  ['METRO_ON', 'Metronome', 0, 1, 0, '', 1],
  ['METRO_BPM', 'Tempo', 40, 240, 120, 'BPM', 1],
  ['METRO_BEATS', 'Beats', 1, 12, 4, 'beats', 1],
  ['METRO_LEVEL', 'Click', -40, 0, -18, 'dB', 0.1],
  ['EQ_ON', 'EQ enabled', 0, 1, 0, '', 1],
  ['EQ_BODY', 'Body', -12, 12, 0, 'dB', 0.1],
  ['EQ_MUD', 'Mud', -12, 12, 0, 'dB', 0.1],
  ['EQ_FOCUS', 'Focus', -12, 12, 0, 'dB', 0.1],
  ['EQ_FIZZ', 'Fizz', -12, 12, 0, 'dB', 0.1],
  ['AMP_SOURCE', 'Amp source', 0, 4, 0, '', 1],
  ['CAPTURE_KIND', 'Capture type', 0, 3, 0, '', 1],
  ['CAB_MODE', 'Cabinet mode', 0, 3, 0, '', 1],
  ['PEDAL_INPUT', 'Input', -24, 24, 0, 'dB', 0.1],
  ['PEDAL_OUTPUT', 'Output', -24, 12, 0, 'dB', 0.1],
  ['COMP_THRESH', 'Threshold', -40, 0, -20, 'dB', 0.1],
  ['COMP_RATIO', 'Ratio', 1, 10, 2.5, ':1', 0.1],
  ['COMP_ATTACK', 'Attack', 1, 100, 15, 'ms', 1],
  ['COMP_RELEASE', 'Release', 20, 500, 140, 'ms', 1, 150],
  ['COMP_MAKEUP', 'Makeup', 0, 12, 3, 'dB', 0.1],
  ['CHORUS_MIX', 'Mix', 0, 100, 0, '%', 1],
  ['CHORUS_RATE', 'Rate', 0.1, 5, 0.8, 'Hz', 0.01],
  ['CHORUS_DEPTH', 'Depth', 0, 100, 35, '%', 1],
  ['DELAY_FEEDBACK', 'Feedback', 0, 85, 35, '%', 1],
  ['DELAY_SYNC', 'Tempo sync', 0, 1, 0, '', 1],
  ['DELAY_DIVISION', 'Division', 0, 5, 2, '', 1],
  ['REVERB_STYLE', 'Voice', 0, 2, 0, '', 1],
  ['REVERB_DAMP', 'Damping', 0, 100, 55, '%', 1],
  ['REVERB_PREDELAY', 'Pre-delay', 0, 150, 0, 'ms', 1],
  ['CAPTURE_MATCH', 'Level match', 0, 1, 1, '', 1],
  ['COMP_MODE', 'Compressor routing', 0, 3, 0, '', 1],
  ['OD_ON', 'Overdrive enabled', 0, 1, 0, '', 1],
  ['OD_DRIVE', 'Drive', 0, 100, 15, '%', 1],
  ['OD_TONE', 'Tone', 0, 100, 50, '%', 1],
  ['OD_LEVEL', 'Level', -24, 12, 0, 'dB', 0.1],
  ['OD_TIGHT', 'Low cut', 20, 200, 80, 'Hz', 1, 80],
  ['CAB_B_ON', 'Second cabinet enabled', 0, 1, 0, '', 1],
  ['CAB_BLEND', 'B blend', 0, 100, 50, '%', 1],
  ['CAB_A_LEVEL', 'A level', -24, 12, 0, 'dB', 0.1],
  ['CAB_B_LEVEL', 'B level', -24, 12, 0, 'dB', 0.1],
  ['CAB_A_PAN', 'A pan', -100, 100, 0, '%', 1],
  ['CAB_B_PAN', 'B pan', -100, 100, 0, '%', 1],
  ['CAB_A_INVERT', 'Cabinet A polarity', 0, 1, 0, '', 1],
  ['CAB_B_INVERT', 'Cabinet B polarity', 0, 1, 0, '', 1],
  ['CAB_A_DELAY', 'A alignment', 0, 10, 0, 'ms', 0.01],
  ['CAB_B_DELAY', 'B alignment', 0, 10, 0, 'ms', 0.01],
  ['CAB_LOW_CUT', 'Low cut', 20, 500, 20, 'Hz', 1, 100],
  ['CAB_HIGH_CUT', 'High cut', 2000, 20000, 20000, 'Hz', 1, 8000],
  ['MOD_ON', 'Modulation enabled', 0, 1, 0, '', 1],
  ['MOD_TYPE', 'Modulation voice', 0, 2, 0, '', 1],
  ['MOD_RATE', 'Speed', 0.05, 10, 0.8, 'Hz', 0.01],
  ['MOD_DEPTH', 'Motion', 0, 100, 50, '%', 1],
  ['MOD_MIX', 'Blend', 0, 100, 50, '%', 1],
  ['MOD_FEEDBACK', 'Regeneration', 0, 70, 20, '%', 1],
  ['MOD_STEREO', 'Spread', 0, 100, 0, '%', 1],
  ['MOD_SYNC', 'Modulation sync', 0, 1, 0, '', 1],
  ['MOD_DIVISION', 'Modulation division', 0, 4, 2, '', 1],
  ['GUITAR_MIX_LEVEL', 'Guitar balance', -12, 12, 0, 'dB', 0.1],
  ['GUITAR_MIX_FOCUS', 'Mix focus', 0, 100, 0, '%', 1],

].map(([id, label, min, max, initial, unit, step, centre = 0]) =>
  ({ id, label, min, max, initial, unit, step, centre, skew: centre ? Math.log(0.5) / Math.log((centre - min) / (max - min)) : 1 }));
export const byId = Object.fromEntries(parameters.map(p => [p.id, p]));

export const boardTypes = ['compressor', 'overdrive', 'neural-pedal', 'eq', 'modulation', 'chorus', 'delay', 'reverb', 'ambience', 'wah', 'distortion'];
export const boardControls = [
  ['CLEAN_COMP', 'COMP_THRESH', 'COMP_RATIO', 'COMP_ATTACK', 'COMP_RELEASE', 'COMP_MAKEUP'],
  ['OD_ON', 'OD_DRIVE', 'OD_TONE', 'OD_LEVEL', 'OD_TIGHT'],
  ['PEDAL_ON', 'PEDAL_INPUT', 'PEDAL_OUTPUT'], ['EQ_ON', 'EQ_BODY', 'EQ_MUD', 'EQ_FOCUS', 'EQ_FIZZ'],
  ['MOD_ON', 'MOD_TYPE', 'MOD_RATE', 'MOD_DEPTH', 'MOD_MIX', 'MOD_FEEDBACK', 'MOD_STEREO', 'MOD_SYNC', 'MOD_DIVISION'],
  ['CHORUS_MIX', 'CHORUS_RATE', 'CHORUS_DEPTH'], ['DELAY_TIME', 'DELAY_MIX', 'DELAY_WIDTH', 'DELAY_FEEDBACK', 'DELAY_SYNC', 'DELAY_DIVISION'],
  ['REVERB_MIX', 'REVERB_SIZE', 'REVERB_STYLE', 'REVERB_DAMP', 'REVERB_PREDELAY'], ['AMBIENCE_MIX'],
  ['WAH_MODE', 'WAH_POSITION', 'WAH_SENSITIVITY', 'WAH_RESONANCE', 'WAH_MIX'],
  ['DIST_MODE', 'DIST_DRIVE', 'DIST_TONE', 'DIST_TIGHT', 'DIST_MIX'],
];
export const boardPrefix = (kind, slot) => `BOARD_${boardTypes[kind].replaceAll('-', '_').toUpperCase()}_${slot}_`;
export const boardId = (kind, slot, id) => slot === 0 && kind < 8 ? id : boardPrefix(kind, slot) + id;
export const boardOnId = (kind, slot) => boardControls[kind][0].endsWith('_ON') ? boardId(kind, slot, boardControls[kind][0]) : boardPrefix(kind, slot) + 'ON';
export const boardParameters = boardTypes.flatMap((type, kind) => [0, 1].flatMap(slot => {
  const rows = [];
  if (!boardControls[kind][0].endsWith('_ON')) rows.push({id: boardOnId(kind, slot), label: 'Enabled', min: 0, max: 1, initial: 1, unit: '', step: 1, centre: 0, skew: 1});
  rows.push({id: boardPrefix(kind, slot) + 'TRIM', label: 'Output trim', min: -24, max: 12, initial: 0, unit: 'dB', step: .1, centre: 0, skew: 1});
  if (kind === 8) rows.push({id: boardId(kind, slot, 'AMBIENCE_MIX'), label: 'Blend', min: 0, max: 100, initial: 25, unit: '%', step: 1, centre: 0, skew: 1});
  if (kind === 9) rows.push(...[
    ['WAH_MODE', 'Mode', 0, 1, 0, ''], ['WAH_POSITION', 'Position', 0, 100, 50, '%'],
    ['WAH_SENSITIVITY', 'Sensitivity', -24, 24, 0, 'dB'], ['WAH_RESONANCE', 'Resonance', 0, 100, 45, '%'], ['WAH_MIX', 'Blend', 0, 100, 100, '%'],
  ].map(([id, label, min, max, initial, unit]) => ({id: boardId(kind, slot, id), label, min, max, initial, unit, step: unit ? .1 : 1, centre: 0, skew: 1})));
  if (kind === 10) rows.push(...[
    ['DIST_MODE', 'Mode', 0, 2, 0, '', 1, 0], ['DIST_DRIVE', 'Drive', 0, 100, 45, '%', 1, 0],
    ['DIST_TONE', 'Tone', 0, 100, 50, '%', 1, 0], ['DIST_TIGHT', 'Low cut', 20, 250, 80, 'Hz', 1, 80], ['DIST_MIX', 'Blend', 0, 100, 100, '%', 1, 0],
  ].map(([id,label,min,max,initial,unit,step,centre]) => ({id: boardId(kind,slot,id),label,min,max,initial,unit,step,centre,skew: centre ? Math.log(.5)/Math.log((centre-min)/(max-min)) : 1})));
  if (slot === 1 && kind < 8) rows.push(...boardControls[kind].map(id => ({...byId[id], id: boardId(kind, slot, id)})));
  return rows;
}));
boardParameters.forEach(p => { byId[p.id] = p; });
export const allParameters = [...parameters, ...boardParameters];

// Range ends that mean "off": the DSP bypasses Tight at 20 Hz and High cut at 20 kHz, and
// the gate and the character effects switch off when their knob is fully down.
const offAt = { TIGHT: 'min', HIGH_CUT: 'max', CAB_LOW_CUT: 'min', CAB_HIGH_CUT: 'max', GATE_THRESH: 'min', DYN_RES_AMOUNT: 'min', THICKEN_MIX: 'min', PIEZO_BLEND: 'min' };

export const clamp = (p, value) => Math.min(p.max, Math.max(p.min, value));
// Knob travel (and the host's normalised value) for a value, and back.
export const toFraction = (p, value) => ((clamp(p, value) - p.min) / (p.max - p.min)) ** p.skew;
export const fromFraction = (p, fraction) => p.min + Math.min(1, Math.max(0, fraction)) ** (1 / p.skew) * (p.max - p.min);
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
