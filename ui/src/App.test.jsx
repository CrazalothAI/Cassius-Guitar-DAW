import React from 'react';
import { afterEach, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render, screen } from '@testing-library/react';
import App from './App.jsx';
afterEach(cleanup);
const choose = name => fireEvent.change(screen.getByRole('combobox', { name: 'Preset' }), { target: { value: name } });
const display = () => document.querySelector('.preset-display').textContent;
const openDrawer = () => fireEvent.click(screen.getByRole('button', { name: /RIG & TONE/ }));
describe('streamlined amp editor', () => {
  it('shows six main controls and keeps rig management in the drawer', () => {
    render(<App/>);
    expect(screen.getAllByRole('slider')).toHaveLength(6);
    expect(screen.getByText(/Browser preview ·/)).toBeTruthy();
    expect(screen.queryByRole('button', { name: /Load amp model/ })).toBeNull();
    openDrawer();
    expect(screen.getAllByRole('slider')).toHaveLength(13);
    fireEvent.click(screen.getByRole('tab', { name: 'Rig' }));
    expect(screen.getByRole('button', { name: /Load amp model/ }).disabled).toBe(true);
    expect(screen.getByRole('button', { name: /Load cabinet IR/ }).disabled).toBe(true);
  });
  it('keeps the main view to a handful of buttons', () => {
    render(<App/>);
    // Previous, next, A/B, tuner and the drawer, plus Revert while a preset is edited.
    expect(screen.getAllByRole('button').length).toBeLessThanOrEqual(6);
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
    openDrawer();
    expect(screen.getByRole('slider', { name: 'Threshold' }).value).toBe('-48');
    expect(screen.getByRole('slider', { name: 'Tight' }).value).toBe('60');
    expect(screen.getByRole('slider', { name: 'Output' }).value).toBe('0');
  });
  it('turns the gate off with its threshold knob fully down, and back on by turning it up', () => {
    render(<App/>);
    choose('Modern metalcore');
    openDrawer();
    const threshold = screen.getByRole('slider', { name: 'Threshold' });
    fireEvent.keyDown(threshold, { key: 'Home' }); fireEvent.keyUp(threshold, { key: 'Home' });
    expect(threshold.getAttribute('aria-valuetext')).toBe('Off');
    expect(screen.getByText('Gate off')).toBeTruthy();
    fireEvent.keyDown(threshold, { key: 'PageUp' }); fireEvent.keyUp(threshold, { key: 'PageUp' });
    expect(screen.getByText('Gate on')).toBeTruthy();
  });
  it('retains warm cleans and separates effects from rig controls', () => {
    render(<App/>);
    choose('Warm clean');
    expect(screen.getByRole('slider', { name: 'Space' }).value).toBe('14');
    openDrawer();
    expect(screen.getByRole('slider', { name: 'Compression' }).value).toBe('55');
    fireEvent.click(screen.getByRole('tab', { name: 'Effects' }));
    expect(screen.getByRole('slider', { name: 'Time' }).value).toBe('280');
    expect(screen.getByRole('slider', { name: 'Chug cut' }).getAttribute('aria-valuetext')).toBe('Off');
    expect(screen.queryByRole('slider', { name: 'Threshold' })).toBeNull();
  });
  it('switches a character effect on by turning its knob up', () => {
    render(<App/>);
    choose('Warm clean');
    openDrawer();
    fireEvent.click(screen.getByRole('tab', { name: 'Effects' }));
    const sub = screen.getByRole('slider', { name: 'Sub' });
    expect(sub.getAttribute('aria-valuetext')).toBe('Off');
    fireEvent.change(sub, { target: { value: '40' } });
    expect(sub.getAttribute('aria-valuetext')).toBe('40 %');
    expect(display()).toContain('Edited');
    fireEvent.change(sub, { target: { value: '0' } });
    expect(sub.getAttribute('aria-valuetext')).toBe('Off');
  });
  it('allows calibration and reset from the Amp page', () => {
    render(<App/>);
    openDrawer();
    const input = screen.getByRole('slider', { name: 'Input' });
    fireEvent.change(input, { target: { value: '12' } });
    expect(input.value).toBe('12');
    fireEvent.doubleClick(input);
    expect(input.value).toBe('0');
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
    expect(display()).toContain('Wide thall');
  });
  it('shows the family of presets chosen from the list', () => {
    render(<App/>);
    choose('Drop-Z djent');
    expect(document.querySelector('.preset-family').textContent).toBe('Thall');
  });
  it('moves between drawer pages with the arrow keys', () => {
    render(<App/>);
    openDrawer();
    const amp = screen.getByRole('tab', { name: 'Amp' });
    fireEvent.click(amp);
    fireEvent.keyDown(amp, { key: 'ArrowRight' });
    expect(screen.getByRole('tab', { name: 'Effects' }).getAttribute('aria-selected')).toBe('true');
    expect(document.activeElement).toBe(screen.getByRole('tab', { name: 'Effects' }));
    fireEvent.keyDown(document.activeElement, { key: 'End' });
    expect(screen.getByRole('tabpanel').getAttribute('aria-labelledby')).toBe('tab-Rig');
    fireEvent.keyDown(document.activeElement, { key: 'ArrowRight' });
    expect(screen.getByRole('tab', { name: 'Amp' }).getAttribute('aria-selected')).toBe('true');
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
