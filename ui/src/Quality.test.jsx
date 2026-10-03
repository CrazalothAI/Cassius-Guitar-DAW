import React from 'react';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render, screen, within } from '@testing-library/react';
import App from './App.jsx';
import { applyPreset } from './presets.js';
import { snapshotParameters } from './parameterState.js';

beforeEach(() => { localStorage.clear(); applyPreset('Glass clean'); });
afterEach(cleanup);

describe('expanded clean and gain controls', () => {
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
