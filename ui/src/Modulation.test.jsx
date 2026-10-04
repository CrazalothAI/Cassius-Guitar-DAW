import React from 'react';
import { afterEach, beforeEach, expect, it } from 'vitest';
import { cleanup, fireEvent, render, screen, within } from '@testing-library/react';
import App from './App.jsx';
import { applyPreset } from './presets.js';
import { snapshotParameters } from './parameterState.js';
beforeEach(() => { localStorage.clear(); applyPreset('Glass clean'); });
afterEach(cleanup);
it('enables modulation, selects voices and recalls its settings in A/B', () => {
  render(<App/>); fireEvent.click(screen.getByRole('tab', {name: 'Effects'}));
  const group = screen.getByRole('group', {name: 'Modulation'});
  fireEvent.click(within(group).getByRole('button', {name: 'Modulation enabled'}));
  fireEvent.change(screen.getByLabelText('Modulation voice'), {target: {value: '1'}});
  fireEvent.change(within(group).getByRole('slider', {name: 'Motion'}), {target: {value: '72'}});
  fireEvent.change(within(group).getByRole('slider', {name: 'Spread'}), {target: {value: '35'}});
  expect(screen.getByRole('tab', {name: 'Effects'}).textContent).toContain('Flanger 50%');
  fireEvent.click(screen.getByRole('button', {name: 'A/B compare'}));
  fireEvent.change(screen.getByLabelText('Modulation voice'), {target: {value: '2'}});
  expect(within(group).queryByRole('slider', {name: 'Regeneration'})).toBeNull();
  fireEvent.click(screen.getByRole('button', {name: 'A/B compare'}));
  expect(snapshotParameters()).toMatchObject({MOD_ON: 1, MOD_TYPE: 1, MOD_DEPTH: 72, MOD_STEREO: 35});
});
it('syncs full modulation cycles and preserves the manual speed', () => {
  render(<App/>); fireEvent.click(screen.getByRole('tab', {name: 'Effects'}));
  const group = screen.getByRole('group', {name: 'Modulation'});
  fireEvent.change(within(group).getByRole('slider', {name: 'Speed'}), {target: {value: '2.7'}});
  fireEvent.click(screen.getByRole('button', {name: 'Modulation tempo sync'}));
  fireEvent.change(screen.getByLabelText('Modulation cycle'), {target: {value: '3'}});
  expect(snapshotParameters()).toMatchObject({MOD_SYNC: 1, MOD_DIVISION: 3, MOD_RATE: 2.7});
  fireEvent.click(screen.getByRole('button', {name: 'Modulation tempo sync'}));
  expect(screen.queryByLabelText('Modulation cycle')).toBeNull();
  expect(within(group).getByRole('slider', {name: 'Speed'}).value).toBe('2.7');
});
it('offers clean and driven modulation presets and restores bypass in ordinary tones', () => {
  applyPreset('Velvet tremolo'); expect(snapshotParameters()).toMatchObject({AMP_SOURCE: 1, MOD_ON: 1, MOD_TYPE: 2});
  applyPreset('Phase lead'); expect(snapshotParameters()).toMatchObject({AMP_SOURCE: 2, MOD_TYPE: 0, OD_ON: 1});
  applyPreset('Jet rock'); expect(snapshotParameters()).toMatchObject({AMP_SOURCE: 2, MOD_TYPE: 1, MOD_ON: 1});
  applyPreset('Glass clean'); expect(snapshotParameters()).toMatchObject({MOD_ON: 0, MOD_TYPE: 0, MOD_SYNC: 0});
});
