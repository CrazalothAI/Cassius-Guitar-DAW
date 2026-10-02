// @vitest-environment node
import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { byId, formatValue, fromFraction, parameters, snap, toFraction } from './parameters.js';
const text = (id, value) => { const { text, unit } = formatValue(byId[id], value); return unit ? `${text} ${unit}` : text; };
describe('parameter table', () => {
  it('matches the native definitions in order, range, default and unit', () => {
    const header = readFileSync(new URL('../../Source/params/ParameterIDs.h', import.meta.url), 'utf8');
    const native = [...header.matchAll(/Definition \{ "(\w+)", "[^"]*", (-?[\d.]+)f?, (-?[\d.]+)f?, (-?[\d.]+)f?, "([^"]*)"(?:, (-?[\d.]+)f?)? \}/g)]
      .map(([, id, min, max, initial, unit, centre]) => ({ id, min: Number(min), max: Number(max), initial: Number(initial), unit, centre: Number(centre ?? 0) }));
    expect(native).toHaveLength(29);
    expect(parameters.map(({ id, min, max, initial, unit, centre }) => ({ id, min, max, initial, unit, centre }))).toEqual(native);
  });
  it('formats values the way the DSP treats them', () => {
    expect(text('TIGHT', 20)).toBe('Off');
    expect(text('TIGHT', 85)).toBe('85 Hz');
    expect(text('HIGH_CUT', 20000)).toBe('Off');
    expect(text('HIGH_CUT', 6800)).toBe('6.8 kHz');
    expect(text('MICRO_DELAY', 0.35)).toBe('0.35 ms');
    expect(text('AMP_BASS', 2)).toBe('+2.0 dB');
    expect(text('AMP_BASS', -0.004)).toBe('0.0 dB');
    expect(text('MASTER_VOL', -12)).toBe('-12.0 dB');
    expect(text('DRIVE_GAIN', 6)).toBe('6.0 dB');
    expect(text('REVERB_MIX', 12.4)).toBe('12 %');
  });
  it('maps skewed ranges like JUCE setSkewForCentre and round-trips', () => {
    for (const p of parameters) {
      expect(fromFraction(p, 0.5)).toBeCloseTo(p.centre || (p.min + p.max) / 2, 6);
      for (const f of [0, 0.13, 0.5, 0.87, 1]) expect(toFraction(p, fromFraction(p, f))).toBeCloseTo(f, 9);
    }
  });
  it('snaps to each control’s resolution within range', () => {
    expect(snap(byId.HIGH_CUT, 6804)).toBe(6800);
    expect(snap(byId.MICRO_DELAY, 0.347)).toBe(0.35);
    expect(snap(byId.AMP_MID, 2.46)).toBe(2.5);
    expect(snap(byId.MASTER_VOL, 9)).toBe(6);
  });
});
