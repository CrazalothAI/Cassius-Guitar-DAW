import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render, screen } from '@testing-library/react';
import CabinetCompare, { cabinetRoute } from './components/CabinetCompare.jsx';
import { applyPreset } from './presets.js';
import { readParameter, setParameter, snapshotParameters } from './parameterState.js';

beforeEach(() => { applyPreset('Glass clean'); setParameter('AMP_SOURCE', 2); setParameter('CAB_MODE', 1); setParameter('CAB_B_ON', 1); });
afterEach(cleanup);
const status = {ir: 'A.wav', irB: 'B.wav'};
describe('cabinet comparison', () => {
  it('sets real blend endpoints and the equal blend, preserving all other tone and listening settings', () => {
    setParameter('CAB_BLEND', 67); setParameter('MASTER_VOL', -23); setParameter('INPUT_GAIN', -4); setParameter('GUITAR_MIX_LEVEL', 5);
    const before = snapshotParameters(); render(<CabinetCompare status={status}/>);
    for (const [name, value] of [['A only', 0], ['B only', 100], ['50/50 blend', 50]]) {
      fireEvent.click(screen.getByRole('button', {name}));
      expect(readParameter('CAB_BLEND')).toBe(value);
      expect(screen.getByRole('button', {name}).getAttribute('aria-pressed')).toBe('true');
    }
    expect(snapshotParameters()).toEqual({...before, CAB_BLEND: 50});
    expect([readParameter('MASTER_VOL'), readParameter('INPUT_GAIN'), readParameter('GUITAR_MIX_LEVEL')]).toEqual([-23, -4, 5]);
  });
  it('resets only alignment and polarity, keeping the cabinet mix, cuts, pans and levels', () => {
    for (const [id, value] of Object.entries({CAB_A_DELAY: 1.25, CAB_B_DELAY: 2.5, CAB_A_INVERT: 1, CAB_B_INVERT: 0, CAB_BLEND: 67, CAB_A_LEVEL: -3, CAB_B_PAN: 21, CAB_LOW_CUT: 80})) setParameter(id, value);
    const before = snapshotParameters(); render(<CabinetCompare status={status}/>);
    expect(screen.getByText(/B delayed 1.25 ms relative to A/)).toBeTruthy();
    expect(screen.getByText(/Opposite polarity may cancel/)).toBeTruthy();
    fireEvent.click(screen.getByRole('button', {name: 'Reset alignment & polarity'}));
    expect(snapshotParameters()).toEqual({...before, CAB_A_DELAY: 0, CAB_B_DELAY: 0, CAB_A_INVERT: 0, CAB_B_INVERT: 0});
    expect(screen.getByText(/No relative delay/)).toBeTruthy();
  });
  it('does not pretend a loaded IR is active when routing bypasses it', () => {
    const values = {AMP_SOURCE: 3, CAB_MODE: 0, CAPTURE_KIND: 3, CAB_B_ON: 1};
    expect(cabinetRoute(values, status).title).toBe('Cabinet included in the capture');
    expect(cabinetRoute({...values, CAB_MODE: 1}, status).dual).toBe(true);
    expect(cabinetRoute({...values, AMP_SOURCE: 4}, status).external).toBe(false);
    expect(cabinetRoute({...values, CAB_MODE: 2}, status).title).toBe('Built-in 4×12');
    expect(cabinetRoute({...values, CAB_MODE: 3}, status).title).toBe('Cabinet bypassed');
    expect(cabinetRoute({...values, AMP_SOURCE: 0}, status).external).toBe(false);
    setParameter('CAB_MODE', 2); render(<CabinetCompare status={status}/>);
    expect(screen.getAllByRole('button').every(button => button.disabled)).toBe(true);
  });
  it('keeps single-IR level independent of blend and does not enable or load cabinet B', () => {
    setParameter('CAB_B_ON', 0); render(<CabinetCompare status={status}/>);
    expect(screen.getByRole('heading', {name: 'Cabinet A only'})).toBeTruthy();
    expect(screen.getByRole('button', {name: 'B only'}).disabled).toBe(true);
    expect(screen.getByRole('button', {name: 'Reset alignment & polarity'}).disabled).toBe(false);
    expect(cabinetRoute({AMP_SOURCE: 2, CAB_MODE: 1, CAB_B_ON: 1}, {irB: 'B.wav'}).title).toBe('Cabinet B only');
    expect(cabinetRoute({AMP_SOURCE: 2, CAB_MODE: 1, CAB_B_ON: 0}, {irB: 'B.wav'}).external).toBe(false);
  });
  it('blocks comparison and reset during recording, take playback and rig loading', () => {
    for (const pending of [{practice: {recordMode: 1}}, {review: {playing: true}}, {rigLoading: true}]) {
      const before = snapshotParameters(), view = render(<CabinetCompare status={{...status, ...pending}}/>);
      for (const button of screen.getAllByRole('button')) { expect(button.disabled).toBe(true); fireEvent.click(button); }
      expect(snapshotParameters()).toEqual(before); view.unmount();
    }
  });
});
