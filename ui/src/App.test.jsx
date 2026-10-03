import React from 'react';
import { afterEach, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render, screen, within } from '@testing-library/react';
import App from './App.jsx';
afterEach(cleanup);
const choose = name => fireEvent.change(screen.getByRole('combobox', { name: 'Preset' }), { target: { value: name } });
const display = () => document.querySelector('.preset-display').textContent;
const stage = name => fireEvent.click(screen.getByRole('tab', { name }));
const amp = () => within(screen.getByRole('region', { name: 'Amplifier' }));
describe('amp and signal chain', () => {
  it('shows six amp controls and the selected stage’s controls below, in signal order', () => {
    render(<App/>);
    expect(amp().getAllByRole('slider')).toHaveLength(6);
    expect(screen.getAllByRole('tab').map(t => t.getAttribute('aria-label'))).toEqual(['Input', 'Pedal', 'Amp', 'Cab', 'EQ', 'Effects']);
    expect(screen.getByRole('tab', { name: 'Amp' }).getAttribute('aria-selected')).toBe('true');
    expect(screen.getByRole('slider', { name: 'High cut' })).toBeTruthy();
    expect(screen.getByRole('button', { name: /Load amp model/ }).disabled).toBe(true);
    stage('Cab');
    expect(screen.getByRole('button', { name: /Load cabinet IR/ }).disabled).toBe(true);
    expect(screen.getByText(/Browser preview ·/)).toBeTruthy();
  });
  it('smooths distortion with the EQ pedal and preserves the clean starting points', () => {
    render(<App/>);
    choose('Modern metalcore'); stage('EQ');
    expect(screen.getByRole('button', { name: 'EQ enabled' }).getAttribute('aria-pressed')).toBe('true');
    expect(screen.getByRole('slider', { name: 'Fizz' }).value).toBe('-4');
    fireEvent.click(screen.getByRole('button', { name: 'Flat EQ' }));
    expect(screen.getByRole('slider', { name: 'Mud' }).value).toBe('0');
    fireEvent.click(screen.getByRole('button', { name: 'Smooth distortion' }));
    expect(screen.getByRole('slider', { name: 'Body' }).value).toBe('-2');
    expect(screen.getByRole('slider', { name: 'Focus' }).value).toBe('1');
    choose('Glass clean');
    expect(screen.getByRole('button', { name: 'EQ enabled' }).getAttribute('aria-pressed')).toBe('false');
    expect(screen.getByRole('slider', { name: 'Fizz' }).value).toBe('0');
  });
  it('includes the EQ pedal in A/B comparisons', () => {
    render(<App/>);
    choose('Modern metalcore'); stage('EQ');
    fireEvent.change(screen.getByRole('slider', { name: 'Fizz' }), { target: { value: '-9' } });
    fireEvent.click(screen.getByRole('button', { name: 'A/B compare' }));
    fireEvent.click(screen.getByRole('button', { name: 'Flat EQ' }));
    fireEvent.click(screen.getByRole('button', { name: 'A/B compare' }));
    expect(screen.getByRole('slider', { name: 'Fizz' }).value).toBe('-9');
  });
  it('keeps the header to a handful of buttons', () => {
    render(<App/>);
    // Previous, next, A/B, metronome and tuner, plus Revert while a preset is edited.
    expect(within(document.querySelector('header')).getAllByRole('button').length).toBeLessThanOrEqual(6);
  });
  it('switches presets without moving master volume', () => {
    render(<App/>);
    const master = screen.getByRole('slider', { name: 'Master' });
    fireEvent.change(master, { target: { value: '-18' } });
    choose('Glass clean');
    expect(display()).toContain('Clean');
    expect(screen.getByRole('slider', { name: 'Space' }).value).toBe('20');
    choose('Modern metalcore');
    expect(screen.getByRole('slider', { name: 'Drive' }).value).toBe('0');
    expect(master.value).toBe('-18');
    expect(screen.getByRole('slider', { name: 'Tight' }).value).toBe('60');
    expect(screen.getByRole('slider', { name: 'Output' }).value).toBe('0');
    stage('Input');
    expect(screen.getByRole('slider', { name: 'Threshold' }).value).toBe('-48');
  });
  it('switches channel from the amp and names it on the badge', () => {
    render(<App/>);
    choose('Modern metalcore');
    const channel = screen.getByRole('button', { name: 'Channel' });
    expect(channel.getAttribute('aria-pressed')).toBe('true');
    expect(screen.getByText('FERRUM · HIGH GAIN')).toBeTruthy();
    fireEvent.click(channel);
    expect(channel.getAttribute('aria-pressed')).toBe('false');
    expect(screen.getByText('LUMEN · CLEAN')).toBeTruthy();
    expect(screen.getByRole('slider', { name: 'Compression' })).toBeTruthy();
    expect(display()).toContain('Edited');
  });
  it('turns the gate off with its threshold knob fully down, and back on by turning it up', () => {
    render(<App/>);
    choose('Modern metalcore');
    stage('Input');
    const threshold = screen.getByRole('slider', { name: 'Threshold' });
    fireEvent.keyDown(threshold, { key: 'Home' }); fireEvent.keyUp(threshold, { key: 'Home' });
    expect(threshold.getAttribute('aria-valuetext')).toBe('Off');
    expect(screen.getByRole('tab', { name: 'Input' }).textContent).toContain('Gate off');
    fireEvent.keyDown(threshold, { key: 'PageUp' }); fireEvent.keyUp(threshold, { key: 'PageUp' });
    expect(screen.getByRole('tab', { name: 'Input' }).textContent).toContain('Gate on');
  });
  it('retains warm cleans and keeps effects on their own stage', () => {
    render(<App/>);
    choose('Warm clean');
    expect(screen.getByRole('slider', { name: 'Space' }).value).toBe('14');
    expect(screen.getByRole('slider', { name: 'Compression' }).value).toBe('55');
    stage('Effects');
    expect(screen.getByRole('slider', { name: 'Time' }).value).toBe('280');
    expect(screen.getByRole('slider', { name: 'Chug cut' }).getAttribute('aria-valuetext')).toBe('Off');
    expect(screen.queryByRole('slider', { name: 'Threshold' })).toBeNull();
    expect(screen.getByRole('tab', { name: 'Effects' }).textContent).toContain('Delay 5% · Space 14%');
  });
  it('switches a character effect on by turning its knob up', () => {
    render(<App/>);
    choose('Warm clean');
    stage('Effects');
    const sub = screen.getByRole('slider', { name: 'Sub' });
    expect(sub.getAttribute('aria-valuetext')).toBe('Off');
    fireEvent.change(sub, { target: { value: '40' } });
    expect(sub.getAttribute('aria-valuetext')).toBe('40 %');
    expect(display()).toContain('Edited');
    fireEvent.change(sub, { target: { value: '0' } });
    expect(sub.getAttribute('aria-valuetext')).toBe('Off');
  });
  it('allows calibration and reset on the Input stage', () => {
    render(<App/>);
    stage('Input');
    const input = screen.getByRole('slider', { name: 'Input' });
    fireEvent.change(input, { target: { value: '12' } });
    expect(input.value).toBe('12');
    fireEvent.doubleClick(input);
    expect(input.value).toBe('0');
    expect(screen.getByText('Removes mains hum on its own')).toBeTruthy();
  });
  it('compares two tone states without changing the master level', () => {
    render(<App/>);
    choose('Warm clean');
    const drive = screen.getByRole('slider', { name: 'Drive' });
    const master = screen.getByRole('slider', { name: 'Master' });
    fireEvent.click(screen.getByRole('button', { name: 'A/B compare' }));
    fireEvent.change(drive, { target: { value: '8' } });
    fireEvent.change(master, { target: { value: '-30' } });
    fireEvent.click(screen.getByRole('button', { name: 'A/B compare' }));
    expect(drive.value).toBe('2');
    expect(master.value).toBe('-30');
    expect(screen.getByRole('button', { name: 'A/B compare' }).textContent).toBe('A/B · B');
    fireEvent.click(screen.getByRole('button', { name: 'A/B compare' }));
    expect(drive.value).toBe('8');
  });
  it('names the preset, marks edits and reverts them', () => {
    render(<App/>);
    choose('Singing lead');
    expect(display()).toContain('Singing lead');
    expect(display()).toContain('Lead');
    expect(display()).not.toContain('Edited');
    expect(screen.queryByRole('button', { name: /Revert/ })).toBeNull();
    fireEvent.change(screen.getByRole('slider', { name: 'Drive' }), { target: { value: '6' } });
    expect(display()).toContain('Edited');
    fireEvent.click(screen.getByRole('button', { name: 'Revert to Singing lead' }));
    expect(display()).not.toContain('Edited');
    expect(screen.getByRole('slider', { name: 'Drive' }).value).toBe('0');
  });
  it('steps through presets in family order and wraps around', () => {
    render(<App/>);
    choose('Glass clean');
    fireEvent.click(screen.getByRole('button', { name: 'Next preset' }));
    expect(display()).toContain('Warm clean');
    choose('Glass clean');
    fireEvent.click(screen.getByRole('button', { name: 'Previous preset' }));
    expect(display()).toContain('Wide low-tuned metal');
  });
  it('shows the family of presets chosen from the list', () => {
    render(<App/>);
    choose('Drop-Z djent');
    expect(document.querySelector('.preset-family').textContent).toBe('Extended range');
  });
  it('moves along the signal chain with the arrow keys', () => {
    render(<App/>);
    const ampTab = screen.getByRole('tab', { name: 'Amp' });
    fireEvent.keyDown(ampTab, { key: 'ArrowRight' });
    expect(screen.getByRole('tab', { name: 'Cab' }).getAttribute('aria-selected')).toBe('true');
    expect(document.activeElement).toBe(screen.getByRole('tab', { name: 'Cab' }));
    fireEvent.keyDown(document.activeElement, { key: 'End' });
    expect(screen.getByRole('tabpanel').getAttribute('aria-labelledby')).toBe('tab-Effects');
    fireEvent.keyDown(document.activeElement, { key: 'ArrowRight' });
    expect(screen.getByRole('tab', { name: 'Input' }).getAttribute('aria-selected')).toBe('true');
  });
  it('opens the tuner over the amp without a detected note', () => {
    render(<App/>);
    fireEvent.click(screen.getByRole('button', { name: 'TUNER' }));
    const tuner = screen.getByRole('group', { name: 'Chromatic tuner' });
    expect(tuner.closest('.grille')).toBeTruthy();
    expect(tuner.className).not.toContain('in-tune');
    expect(tuner.textContent).toContain('Play a single open string');
  });
});
describe('metronome', () => {
  const open = () => fireEvent.click(screen.getByRole('button', { name: 'Metronome' }));
  // Preview values persist between tests, so each test switches the click on explicitly.
  const switchOn = () => { const click = screen.getByRole('button', { name: 'Click' }); if (click.getAttribute('aria-pressed') !== 'true') fireEvent.click(click); };
  const tempo = () => Number(screen.getByRole('slider', { name: 'Tempo' }).value);
  it('turns on from its panel and shows the tempo in the header', () => {
    render(<App/>);
    open();
    const panel = screen.getByRole('group', { name: 'Metronome settings' });
    expect(screen.getByRole('button', { name: 'Metronome' }).getAttribute('aria-expanded')).toBe('true');
    switchOn();
    const before = tempo();
    expect(screen.getByRole('button', { name: 'Metronome' }).textContent).toContain(`${before} BPM`);
    fireEvent.click(within(panel).getByRole('button', { name: 'Faster' }));
    expect(tempo()).toBe(before + 1);
    fireEvent.change(within(panel).getByRole('combobox', { name: 'Beats per bar' }), { target: { value: '3' } });
    expect(panel.querySelectorAll('.metro-beats i')).toHaveLength(3);
    fireEvent.keyDown(panel, { key: 'Escape' });
    expect(screen.queryByRole('group', { name: 'Metronome settings' })).toBeNull();
  });
  it('sets the tempo from taps', () => {
    render(<App/>);
    open();
    const realNow = performance.now;
    let t = 1000;
    performance.now = () => t;
    try {
      for (let i = 0; i < 4; ++i) { fireEvent.click(screen.getByRole('button', { name: 'Tap' })); t += 400; }
    } finally { performance.now = realNow; }
    expect(tempo()).toBe(150);
  });
  it('is left alone by presets and A/B', () => {
    render(<App/>);
    open();
    switchOn();
    fireEvent.click(screen.getByRole('button', { name: 'A/B compare' }));
    fireEvent.click(screen.getByRole('button', { name: 'Faster' }));
    const set = tempo();
    fireEvent.click(screen.getByRole('button', { name: 'A/B compare' }));
    choose('Low-tuned chug');
    expect(tempo()).toBe(set);
    expect(screen.getByRole('button', { name: 'Click' }).getAttribute('aria-pressed')).toBe('true');
  });
});
