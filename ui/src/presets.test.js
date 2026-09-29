import { describe, expect, it } from 'vitest';
import { byId, parameters, snap } from './parameters.js';
import { familyOf, matchPreset, notes, presets } from './presets.js';
describe('presets', () => {
  it('set every tone parameter, leave calibration alone and stay on the control grid', () => {
    const expected = parameters.map(p => p.id).filter(id => id !== 'INPUT_GAIN' && id !== 'MASTER_VOL').sort();
    for (const [name, preset] of Object.entries(presets)) {
      expect(Object.keys(preset).sort(), name).toEqual(expected);
      for (const [id, value] of Object.entries(preset)) expect(snap(byId[id], value), `${name} ${id}`).toBe(value);
      expect(notes[name], name).toBeTruthy();
      expect(familyOf(name), name).toBeTruthy();
    }
  });
  it('are recognised from parameter values, including host rounding, until edited', () => {
    for (const [name, preset] of Object.entries(presets)) {
      expect(matchPreset({ ...preset, INPUT_GAIN: 5, MASTER_VOL: -20 })).toBe(name);
      expect(matchPreset({ ...preset, AMP_TREBLE: preset.AMP_TREBLE + 0.5 })).toBeNull();
    }
    expect(matchPreset({ ...presets['Drop-Z djent'], MICRO_DELAY: 0.4999999, HIGH_CUT: 6200.004 })).toBe('Drop-Z djent');
  });
});
