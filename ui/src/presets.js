import { byId } from './parameters.js';
import { setParameter } from './parameterState.js';
// Keep input calibration, master level and loaded files when choosing a starting point.
export const presets = {
  'Thall chug': {AMP_CLEAN: 0, PEDAL_ON: 1, GATE_ON: 1, GATE_RELEASE: 60, GATE_THRESH: -46, DRIVE_GAIN: 0, TIGHT: 90, AMP_BASS: -1, AMP_MID: 2, AMP_TREBLE: 0, AMP_OUT: 0, PRESENCE: 1, HIGH_CUT: 6800, CLEAN_COMP: 35, DELAY_TIME: 320, DELAY_MIX: 0, DELAY_WIDTH: 0, REVERB_MIX: 2, REVERB_SIZE: 30, DYN_RES_ON: 1, DYN_RES_AMOUNT: 75, CHUG_ATTACK: 65, THICKEN_ON: 1, THICKEN_MIX: 35, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0.35},
  'Playing God nylon': {AMP_CLEAN: 1, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 220, GATE_THRESH: -70, DRIVE_GAIN: 1, TIGHT: 20, AMP_BASS: 2, AMP_MID: 0, AMP_TREBLE: 3, AMP_OUT: 0, PRESENCE: 2, HIGH_CUT: 18000, CLEAN_COMP: 45, DELAY_TIME: 380, DELAY_MIX: 15, DELAY_WIDTH: 50, REVERB_MIX: 25, REVERB_SIZE: 70, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 1, PIEZO_BLEND: 65, MICRO_DELAY: 0},
  'Drop-Z djent': {AMP_CLEAN: 0, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 50, GATE_THRESH: -44, DRIVE_GAIN: 0, TIGHT: 110, AMP_BASS: -3, AMP_MID: 2.5, AMP_TREBLE: -1, AMP_OUT: 0, PRESENCE: 1.5, HIGH_CUT: 6200, CLEAN_COMP: 35, DELAY_TIME: 280, DELAY_MIX: 0, DELAY_WIDTH: 0, REVERB_MIX: 0, REVERB_SIZE: 25, DYN_RES_ON: 1, DYN_RES_AMOUNT: 85, CHUG_ATTACK: 80, THICKEN_ON: 1, THICKEN_MIX: 45, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0.5},
  '80s rock': {AMP_CLEAN: 0, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 180, GATE_THRESH: -57, DRIVE_GAIN: 0, TIGHT: 50, AMP_BASS: 0, AMP_MID: 2, AMP_TREBLE: 0, AMP_OUT: -3, PRESENCE: 1, HIGH_CUT: 8500, CLEAN_COMP: 35, DELAY_TIME: 300, DELAY_MIX: 10, DELAY_WIDTH: 25, REVERB_MIX: 10, REVERB_SIZE: 45, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Modern metalcore': {AMP_CLEAN: 0, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 140, GATE_THRESH: -48, DRIVE_GAIN: 0, TIGHT: 60, AMP_BASS: 0, AMP_MID: 1.5, AMP_TREBLE: -1, AMP_OUT: 0, PRESENCE: 0, HIGH_CUT: 6500, CLEAN_COMP: 35, DELAY_TIME: 320, DELAY_MIX: 0, DELAY_WIDTH: 0, REVERB_MIX: 0, REVERB_SIZE: 30, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Tight metal': {AMP_CLEAN: 0, PEDAL_ON: 1, GATE_ON: 1, GATE_RELEASE: 80, GATE_THRESH: -48, DRIVE_GAIN: 0, TIGHT: 85, AMP_BASS: -2, AMP_MID: 1, AMP_TREBLE: 0, AMP_OUT: 0, PRESENCE: 1, HIGH_CUT: 9000, CLEAN_COMP: 35, DELAY_TIME: 320, DELAY_MIX: 0, DELAY_WIDTH: 0, REVERB_MIX: 4, REVERB_SIZE: 35, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Singing lead': {AMP_CLEAN: 0, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 230, GATE_THRESH: -58, DRIVE_GAIN: 0, TIGHT: 60, AMP_BASS: -2, AMP_MID: 2.5, AMP_TREBLE: -1, AMP_OUT: 0, PRESENCE: .5, HIGH_CUT: 7500, CLEAN_COMP: 35, DELAY_TIME: 340, DELAY_MIX: 16, DELAY_WIDTH: 35, REVERB_MIX: 11, REVERB_SIZE: 55, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Glass clean': {AMP_CLEAN: 1, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 250, GATE_THRESH: -75, DRIVE_GAIN: 0, TIGHT: 20, AMP_BASS: 1, AMP_MID: -1, AMP_TREBLE: 2, AMP_OUT: 0, PRESENCE: 1, HIGH_CUT: 16000, CLEAN_COMP: 40, DELAY_TIME: 360, DELAY_MIX: 8, DELAY_WIDTH: 45, REVERB_MIX: 20, REVERB_SIZE: 60, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Warm clean': {AMP_CLEAN: 1, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 250, GATE_THRESH: -76, DRIVE_GAIN: 2, TIGHT: 20, AMP_BASS: 1.5, AMP_MID: 1, AMP_TREBLE: -1.5, AMP_OUT: 0, PRESENCE: -1, HIGH_CUT: 11000, CLEAN_COMP: 55, DELAY_TIME: 280, DELAY_MIX: 5, DELAY_WIDTH: 20, REVERB_MIX: 14, REVERB_SIZE: 45, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Ambient clean': {AMP_CLEAN: 1, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 320, GATE_THRESH: -78, DRIVE_GAIN: 1, TIGHT: 20, AMP_BASS: 0, AMP_MID: -1, AMP_TREBLE: 1, AMP_OUT: 0, PRESENCE: .5, HIGH_CUT: 14000, CLEAN_COMP: 50, DELAY_TIME: 480, DELAY_MIX: 28, DELAY_WIDTH: 80, REVERB_MIX: 38, REVERB_SIZE: 85, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
};
export const notes = {
  'Glass clean': 'Clear attack. Room for every note.',
  'Warm clean': 'Rounded highs and extra compression for soft dynamics.',
  'Ambient clean': 'Clean notes, wide repeats, longer trails.',
  'Playing God nylon': 'Acoustic body resonance & glassy piezo sparkle for electric pickups.',
  '80s rock': 'Mid-forward drive with a little room to breathe.',
  'Singing lead': 'Sustain and definition for expressive runs.',
  'Modern metalcore': 'Controlled lows. Fast, deliberate stops.',
  'Tight metal': 'Pedal-pushed rhythm with a firm low cut.',
  'Thall chug': 'Heavy low-end articulation, dynamic resonance cut & chug attack.',
  'Drop-Z djent': 'Deeper resonance cut, sub layer and micro-delay width for extended range.',
};
// Main-panel tone families. Each button applies its first preset; the others belong to the same family.
export const voices = [
  {label: 'Clean', presets: ['Glass clean', 'Warm clean']},
  {label: 'Ambient', presets: ['Ambient clean']},
  {label: 'Piezo', presets: ['Playing God nylon']},
  {label: 'Rock', presets: ['80s rock']},
  {label: 'Lead', presets: ['Singing lead']},
  {label: 'Metal', presets: ['Modern metalcore', 'Tight metal']},
  {label: 'Thall', presets: ['Thall chug', 'Drop-Z djent']},
];
export const familyOf = name => voices.find(v => v.presets.includes(name))?.label ?? null;
export const presetParameterIds = Object.keys(Object.values(presets)[0]);
export function applyPreset(name) { Object.entries(presets[name] ?? {}).forEach(([id, value]) => setParameter(id, value)); }
// The preset whose every value the current parameters still hold (host values are rounded to 0.01).
export const matchPreset = values => Object.keys(presets).find(name =>
  Object.entries(presets[name]).every(([id, value]) => Math.abs(values[id] - value) <= byId[id].step / 2)) ?? null;
