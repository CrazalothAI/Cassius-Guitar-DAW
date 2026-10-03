import React from 'react';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render, screen, within } from '@testing-library/react';
import App from './App.jsx';
import { applyPreset } from './presets.js';
import { setParameter, snapshotParameters } from './parameterState.js';

beforeEach(() => { localStorage.clear(); applyPreset('Glass clean'); });
afterEach(cleanup);

describe('expanded clean and gain controls', () => {
  it('offers compressor routing for every amp, and includes it in A/B recall', () => {
    applyPreset('Modern metalcore'); render(<App/>);
    fireEvent.change(screen.getByRole('combobox', {name: 'Amp source'}), {target: {value: '3'}});
    fireEvent.change(screen.getByRole('combobox', {name: 'Compressor routing'}), {target: {value: '2'}});
    fireEvent.change(screen.getByRole('slider', {name: 'Compression'}), {target: {value: '70'}});
    fireEvent.click(screen.getByRole('button', {name: 'A/B compare'}));
    fireEvent.change(screen.getByRole('combobox', {name: 'Compressor routing'}), {target: {value: '3'}});
    fireEvent.click(screen.getByRole('button', {name: 'A/B compare'}));
    expect(snapshotParameters()).toMatchObject({AMP_SOURCE: 3, COMP_MODE: 2, CLEAN_COMP: 70});
  });
  it('enables built-in drive without a capture and leaves the captured pedal independent', () => {
    render(<App/>); fireEvent.click(screen.getByRole('tab', {name: 'Pedal'}));
    fireEvent.click(screen.getByRole('button', {name: 'Overdrive enabled'}));
    const group = screen.getByRole('group', {name: 'Cassian overdrive'});
    fireEvent.change(within(group).getByRole('slider', {name: 'Drive'}), {target: {value: '45'}});
    fireEvent.change(within(group).getByRole('slider', {name: 'Level'}), {target: {value: '-3'}});
    expect(snapshotParameters()).toMatchObject({OD_ON: 1, OD_DRIVE: 45, OD_LEVEL: -3, PEDAL_ON: 0});
    expect(screen.getByRole('tab', {name: 'Pedal'}).textContent).toContain('Overdrive');
  });
  it('saves independent cabinet alignment, polarity, and blend in A/B', () => {
    render(<App/>); fireEvent.click(screen.getByRole('tab', {name: 'Cab'}));
    fireEvent.click(screen.getByRole('button', {name: 'Second cabinet enabled'}));
    fireEvent.click(screen.getByRole('button', {name: 'Invert cabinet B polarity'}));
    fireEvent.change(screen.getByRole('slider', {name: 'B alignment'}), {target: {value: '1.25'}});
    fireEvent.change(screen.getByRole('slider', {name: 'B blend'}), {target: {value: '67'}});
    fireEvent.click(screen.getByRole('button', {name: 'A/B compare'}));
    fireEvent.change(screen.getByRole('slider', {name: 'B blend'}), {target: {value: '20'}});
    fireEvent.click(screen.getByRole('button', {name: 'A/B compare'}));
    expect(snapshotParameters()).toMatchObject({CAB_B_ON: 1, CAB_B_INVERT: 1, CAB_B_DELAY: 1.25, CAB_BLEND: 67});
  });
  it('restores bypass defaults when changing to a clean starting point', () => {
    setParameter('OD_ON', 1); setParameter('COMP_MODE', 2); setParameter('CAB_B_ON', 1);
    applyPreset('Glass clean');
    expect(snapshotParameters()).toMatchObject({OD_ON: 0, COMP_MODE: 0, CAB_B_ON: 0, CAB_A_LEVEL: 0, CAB_A_DELAY: 0});
  });
  it('exposes compressor settings for the built-in clean amp', () => {
    render(<App/>);
    const group = screen.getByRole('group', {name: 'Compressor'});
    fireEvent.change(within(group).getByRole('slider', {name: 'Ratio'}), {target: {value: '4'}});
    fireEvent.change(within(group).getByRole('slider', {name: 'Threshold'}), {target: {value: '-28'}});
    expect(snapshotParameters().COMP_RATIO).toBe(4);
    expect(snapshotParameters().COMP_THRESH).toBe(-28);
  });
  it('adds independent pedal trims with zero-gain defaults', () => {
    render(<App/>); fireEvent.click(screen.getByRole('tab', {name: 'Pedal'}));
    const group = screen.getByRole('group', {name: 'Pedal gain'});
    expect(within(group).getByRole('slider', {name: 'Input'}).value).toBe('0');
    expect(within(group).getByRole('slider', {name: 'Output'}).value).toBe('0');
  });
  it('switches delay subdivisions without overwriting the manual time', () => {
    render(<App/>); fireEvent.click(screen.getByRole('tab', {name: 'Effects'}));
    const time = screen.getByRole('slider', {name: 'Time'}), previous = time.value;
    fireEvent.click(screen.getByRole('button', {name: 'Delay tempo sync'}));
    fireEvent.change(screen.getByRole('combobox', {name: 'Delay division'}), {target: {value: '3'}});
    expect(snapshotParameters().DELAY_SYNC).toBe(1);
    expect(snapshotParameters().DELAY_DIVISION).toBe(3);
    expect(time.value).toBe(previous);
    fireEvent.click(screen.getByRole('button', {name: 'Delay tempo sync'}));
    expect(screen.queryByRole('combobox', {name: 'Delay division'})).toBeNull();
  });
  it('saves chorus, reverb, and feedback as part of tone snapshots', () => {
    render(<App/>); fireEvent.click(screen.getByRole('tab', {name: 'Effects'}));
    const chorus = screen.getByRole('group', {name: 'Chorus'}), reverb = screen.getByRole('group', {name: 'Reverb'});
    fireEvent.change(within(chorus).getByRole('slider', {name: 'Mix'}), {target: {value: '35'}});
    fireEvent.change(within(reverb).getByRole('combobox', {name: 'Reverb voice'}), {target: {value: '2'}});
    fireEvent.change(within(reverb).getByRole('slider', {name: 'Pre-delay'}), {target: {value: '30'}});
    fireEvent.change(screen.getByRole('slider', {name: 'Feedback'}), {target: {value: '48'}});
    expect(snapshotParameters()).toMatchObject({CHORUS_MIX: 35, REVERB_STYLE: 2, REVERB_PREDELAY: 30, DELAY_FEEDBACK: 48});
    expect(screen.getByRole('checkbox', {name: 'Match A/B loudness'}).disabled).toBe(true);
  });
});
