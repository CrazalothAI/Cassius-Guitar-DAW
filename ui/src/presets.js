import { byId, parameters } from './parameters.js';
import { setParameter } from './parameterState.js';
// Keep input calibration, master level and loaded files when choosing a starting point.
export const presets = {
  'Low-tuned chug': {AMP_CLEAN: 0, PEDAL_ON: 1, GATE_ON: 1, GATE_RELEASE: 60, GATE_THRESH: -46, DRIVE_GAIN: 0, TIGHT: 90, AMP_BASS: -1, AMP_MID: 2, AMP_TREBLE: 0, AMP_OUT: 0, PRESENCE: 1, HIGH_CUT: 6800, CLEAN_COMP: 35, DELAY_TIME: 320, DELAY_MIX: 0, DELAY_WIDTH: 0, REVERB_MIX: 2, REVERB_SIZE: 30, DYN_RES_ON: 1, DYN_RES_AMOUNT: 75, CHUG_ATTACK: 65, THICKEN_ON: 1, THICKEN_MIX: 35, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0.35},
  'Piezo shimmer': {AMP_CLEAN: 1, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 220, GATE_THRESH: -70, DRIVE_GAIN: 1, TIGHT: 20, AMP_BASS: 2, AMP_MID: 0, AMP_TREBLE: 3, AMP_OUT: 0, PRESENCE: 2, HIGH_CUT: 18000, CLEAN_COMP: 45, DELAY_TIME: 380, DELAY_MIX: 15, DELAY_WIDTH: 50, REVERB_MIX: 25, REVERB_SIZE: 70, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 1, PIEZO_BLEND: 65, MICRO_DELAY: 0},
  'Drop-Z djent': {AMP_CLEAN: 0, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 50, GATE_THRESH: -44, DRIVE_GAIN: 0, TIGHT: 110, AMP_BASS: -3, AMP_MID: 2.5, AMP_TREBLE: -1, AMP_OUT: 0, PRESENCE: 1.5, HIGH_CUT: 6200, CLEAN_COMP: 35, DELAY_TIME: 280, DELAY_MIX: 0, DELAY_WIDTH: 0, REVERB_MIX: 0, REVERB_SIZE: 25, DYN_RES_ON: 1, DYN_RES_AMOUNT: 85, CHUG_ATTACK: 80, THICKEN_ON: 1, THICKEN_MIX: 45, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0.5},
  '80s rock': {AMP_CLEAN: 0, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 180, GATE_THRESH: -57, DRIVE_GAIN: 0, TIGHT: 50, AMP_BASS: 0, AMP_MID: 2, AMP_TREBLE: 0, AMP_OUT: -3, PRESENCE: 1, HIGH_CUT: 8500, CLEAN_COMP: 35, DELAY_TIME: 300, DELAY_MIX: 10, DELAY_WIDTH: 25, REVERB_MIX: 10, REVERB_SIZE: 45, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Modern metalcore': {AMP_CLEAN: 0, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 140, GATE_THRESH: -48, DRIVE_GAIN: 0, TIGHT: 60, AMP_BASS: 0, AMP_MID: 1.5, AMP_TREBLE: -1, AMP_OUT: 0, PRESENCE: 0, HIGH_CUT: 6500, CLEAN_COMP: 35, DELAY_TIME: 320, DELAY_MIX: 0, DELAY_WIDTH: 0, REVERB_MIX: 0, REVERB_SIZE: 30, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Tight metal': {AMP_CLEAN: 0, PEDAL_ON: 1, GATE_ON: 1, GATE_RELEASE: 80, GATE_THRESH: -48, DRIVE_GAIN: 0, TIGHT: 85, AMP_BASS: -2, AMP_MID: 1, AMP_TREBLE: 0, AMP_OUT: 0, PRESENCE: 1, HIGH_CUT: 9000, CLEAN_COMP: 35, DELAY_TIME: 320, DELAY_MIX: 0, DELAY_WIDTH: 0, REVERB_MIX: 4, REVERB_SIZE: 35, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Singing lead': {AMP_CLEAN: 0, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 230, GATE_THRESH: -58, DRIVE_GAIN: 0, TIGHT: 60, AMP_BASS: -2, AMP_MID: 2.5, AMP_TREBLE: -1, AMP_OUT: 0, PRESENCE: .5, HIGH_CUT: 7500, CLEAN_COMP: 35, DELAY_TIME: 340, DELAY_MIX: 16, DELAY_WIDTH: 35, REVERB_MIX: 11, REVERB_SIZE: 55, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Glass clean': {AMP_CLEAN: 1, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 250, GATE_THRESH: -75, DRIVE_GAIN: 0, TIGHT: 20, AMP_BASS: 1, AMP_MID: -1, AMP_TREBLE: 2, AMP_OUT: 0, PRESENCE: 1, HIGH_CUT: 16000, CLEAN_COMP: 40, DELAY_TIME: 360, DELAY_MIX: 8, DELAY_WIDTH: 45, REVERB_MIX: 20, REVERB_SIZE: 60, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Warm clean': {AMP_CLEAN: 1, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 250, GATE_THRESH: -76, DRIVE_GAIN: 2, TIGHT: 20, AMP_BASS: 1.5, AMP_MID: 1, AMP_TREBLE: -1.5, AMP_OUT: 0, PRESENCE: -1, HIGH_CUT: 11000, CLEAN_COMP: 55, DELAY_TIME: 280, DELAY_MIX: 5, DELAY_WIDTH: 20, REVERB_MIX: 14, REVERB_SIZE: 45, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Ambient clean': {AMP_CLEAN: 1, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 320, GATE_THRESH: -78, DRIVE_GAIN: 1, TIGHT: 20, AMP_BASS: 0, AMP_MID: -1, AMP_TREBLE: 1, AMP_OUT: 0, PRESENCE: .5, HIGH_CUT: 14000, CLEAN_COMP: 50, DELAY_TIME: 480, DELAY_MIX: 28, DELAY_WIDTH: 80, REVERB_MIX: 38, REVERB_SIZE: 85, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0},
  'Neoclassical lead': {AMP_CLEAN: 0, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 180, GATE_THRESH: -54, DRIVE_GAIN: 2, TIGHT: 72, AMP_BASS: -2, AMP_MID: 3.5, AMP_TREBLE: 1, AMP_OUT: 0, PRESENCE: 1, HIGH_CUT: 8200, CLEAN_COMP: 25, DELAY_TIME: 300, DELAY_MIX: 7, DELAY_WIDTH: 22, REVERB_MIX: 7, REVERB_SIZE: 36, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 20, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0.18},
  'Progressive clean': {AMP_CLEAN: 1, PEDAL_ON: 0, GATE_ON: 1, GATE_RELEASE: 280, GATE_THRESH: -74, DRIVE_GAIN: 1, TIGHT: 20, AMP_BASS: 1, AMP_MID: 0, AMP_TREBLE: 2.5, AMP_OUT: 0, PRESENCE: 1, HIGH_CUT: 17000, CLEAN_COMP: 42, DELAY_TIME: 410, DELAY_MIX: 17, DELAY_WIDTH: 62, REVERB_MIX: 24, REVERB_SIZE: 68, DYN_RES_ON: 0, DYN_RES_AMOUNT: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, THICKEN_MIX: 0, PIEZO_ON: 1, PIEZO_BLEND: 35, MICRO_DELAY: 0.22},
  'Wide low-tuned metal': {AMP_CLEAN: 0, PEDAL_ON: 1, GATE_ON: 1, GATE_RELEASE: 70, GATE_THRESH: -45, DRIVE_GAIN: 0, TIGHT: 100, AMP_BASS: -2, AMP_MID: 2, AMP_TREBLE: -0.5, AMP_OUT: 0, PRESENCE: 1, HIGH_CUT: 7000, CLEAN_COMP: 30, DELAY_TIME: 320, DELAY_MIX: 0, DELAY_WIDTH: 0, REVERB_MIX: 3, REVERB_SIZE: 28, DYN_RES_ON: 1, DYN_RES_AMOUNT: 80, CHUG_ATTACK: 72, THICKEN_ON: 1, THICKEN_MIX: 30, PIEZO_ON: 0, PIEZO_BLEND: 0, MICRO_DELAY: 0.62},
};
// Appended controls default to the legacy/bypassed sound for every starting point.
for (const preset of Object.values(presets))
  for (const p of parameters.slice(41)) preset[p.id] = p.initial;
// EQ is part of each tone and A/B snapshot; clean voices stay flat and bypassed.
const flatEq = {EQ_ON: 0, EQ_BODY: 0, EQ_MUD: 0, EQ_FOCUS: 0, EQ_FIZZ: 0};
for (const [name, preset] of Object.entries(presets)) {
  const lead = name === 'Singing lead' || name === 'Neoclassical lead';
  const rock = name === '80s rock';
  Object.assign(preset, flatEq, preset.AMP_CLEAN ? {} : {
    EQ_ON: 1, EQ_BODY: rock ? -1 : -2, EQ_MUD: lead || rock ? -1.5 : -3,
    EQ_FOCUS: 1, EQ_FIZZ: rock ? -1.5 : lead ? -2.5 : -4,
  });
}
presets['Natural Nylon'] = Object.fromEntries(Object.keys(Object.values(presets)[0]).map(id => [id, byId[id].initial]));
Object.assign(presets['Natural Nylon'], {AMP_CLEAN: 1, GATE_ON: 0, PEDAL_ON: 0, DRIVE_GAIN: 0, TIGHT: 20, CLEAN_COMP: 0,
  HIGH_CUT: 20000, REVERB_MIX: 6, REVERB_SIZE: 25, DYN_RES_ON: 0, CHUG_ATTACK: 0, THICKEN_ON: 0, PIEZO_ON: 0, EQ_ON: 0});
for (const [name, tone] of Object.entries(presets)) Object.assign(tone, {AMP_SOURCE: name === 'Natural Nylon' ? 4 : tone.AMP_CLEAN ? 1 : 0, CAPTURE_KIND: 0, CAB_MODE: name === 'Natural Nylon' ? 3 : 0});
presets['Articulate lead'] = {...presets['Neoclassical lead'], AMP_SOURCE: 2, CAB_MODE: 2,
  OD_ON: 1, OD_DRIVE: 12, OD_TONE: 45, OD_LEVEL: -3, OD_TIGHT: 100,
  COMP_MODE: 1, CLEAN_COMP: 20, COMP_THRESH: -24, COMP_RATIO: 2, COMP_MAKEUP: 0};
presets['Studio clean'] = {...presets['Glass clean'], COMP_MODE: 2, CLEAN_COMP: 55,
  COMP_THRESH: -24, COMP_RATIO: 3, COMP_MAKEUP: 2, COMP_RELEASE: 180};
export const notes = {
  'Articulate lead': 'Built-in overdrive into Ferrum, with light pre-amp compression and short repeats.',
  'Studio clean': 'Lumen with gentle post-cab compression; clear attack and consistent arpeggio levels.',
  'Natural Nylon': 'Neutral DI for a real nylon/piezo input. Optional body IR and gentle room; no electric pickup simulation.',
  'Glass clean': 'Clear attack. Room for every note.',
  'Warm clean': 'Rounded highs and extra compression for soft dynamics.',
  'Ambient clean': 'Clean notes, wide repeats, longer trails.',
  'Piezo shimmer': 'Acoustic body resonance & glassy piezo sparkle for electric pickups.',
  '80s rock': 'Mid-forward drive with a little room to breathe.',
  'Singing lead': 'Sustain and definition for expressive runs.',
  'Modern metalcore': 'Controlled lows. Fast, deliberate stops.',
  'Tight metal': 'Pedal-pushed rhythm with a firm low cut.',
  'Low-tuned chug': 'Heavy low-end articulation, dynamic resonance cut & chug attack.',
  'Drop-Z djent': 'Deeper resonance cut, sub layer and micro-delay width for extended range.',
  'Neoclassical lead': 'Pushed mids and a firm low end with short repeats for fast runs.',
  'Progressive clean': 'Bright clean with piezo sparkle and wide repeats for arpeggios.',
  'Wide low-tuned metal': 'Pedal-pushed chug with resonance cut, sub layer and wider micro-delay.',
};
// Main-panel tone families. Each button applies its first preset; the others belong to the same family.
export const voices = [
  {label: 'Acoustic', presets: ['Natural Nylon']},
  {label: 'Clean', presets: ['Glass clean', 'Warm clean', 'Progressive clean', 'Studio clean']},
  {label: 'Ambient', presets: ['Ambient clean']},
  {label: 'Piezo', presets: ['Piezo shimmer']},
  {label: 'Rock', presets: ['80s rock']},
  {label: 'Lead', presets: ['Singing lead', 'Neoclassical lead', 'Articulate lead']},
  {label: 'Metal', presets: ['Modern metalcore', 'Tight metal']},
  {label: 'Extended range', presets: ['Low-tuned chug', 'Drop-Z djent', 'Wide low-tuned metal']},
];
export const familyOf = name => voices.find(v => v.presets.includes(name))?.label ?? null;
export const presetParameterIds = Object.keys(Object.values(presets)[0]);
export function applyPreset(name) { Object.entries(presets[name] ?? {}).forEach(([id, value]) => setParameter(id, value)); }
// The preset whose every value the current parameters still hold (host values are rounded to 0.01).
export const matchPreset = values => Object.keys(presets).find(name =>
  Object.entries(presets[name]).every(([id, value]) => Math.abs(values[id] - value) <= byId[id].step / 2)) ?? null;
