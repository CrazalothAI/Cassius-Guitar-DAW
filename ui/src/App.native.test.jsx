import React from 'react';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
const engine = vi.hoisted(() => ({ status: {}, calls: [] }));
vi.mock('./juce/bridge.js', () => ({
  native: true,
  slider: () => null,
  invoke: async (name, ...args) => {
    if (name === 'getStatus') return { ...engine.status };
    engine.calls.push([name, ...args]);
    return true;
  },
}));
import App from './App.jsx';
beforeEach(() => {
  engine.calls = [];
  engine.status = { model: 'Rig.nam', ir: '', pedal: '', input: 0, output: 0, gate: 0, sampleRate: 48000, bufferSize: 256, cpu: 10, overruns: 0, message: 'Loaded Rig.nam' };
});
afterEach(cleanup);
describe('editor connected to the audio engine', () => {
  it('warns when a capture is bypassed for its sample rate', async () => {
    engine.status.message = 'Amp bypassed: set audio device / host to 44100 Hz';
    render(<App/>);
    const alert = await screen.findByRole('alert');
    expect(alert.textContent).toContain('Amp capture bypassed');
    expect(alert.textContent).toContain('Set audio device / host to 44100 Hz');
  });
  it('lets a load failure be dismissed until a different one arrives', async () => {
    engine.status.message = 'Load failed: first';
    render(<App/>);
    fireEvent.click(await screen.findByRole('button', { name: 'Dismiss' }));
    expect(screen.queryByRole('alert')).toBeNull();
    engine.status.message = 'Load failed: second';
    expect((await screen.findByRole('alert')).textContent).toContain('second');
  });
  it('reports in tune only for a detected note', async () => {
    engine.status = { ...engine.status, tunerActive: false, tunerCents: 0, tunerHz: 0, tunerNote: '—' };
    render(<App/>);
    fireEvent.click(screen.getByRole('button', { name: 'TUNER' }));
    const tuner = screen.getByRole('group', { name: 'Chromatic tuner' });
    await waitFor(() => expect(screen.getByText(/48\.0 kHz/)).toBeTruthy());
    expect(tuner.className).not.toContain('in-tune');
    engine.status = { ...engine.status, tunerActive: true, tunerCents: -20, tunerHz: 81.2, tunerNote: 'E2' };
    await waitFor(() => expect(tuner.textContent).toContain('-20 cents · 81.2 Hz'));
    expect(tuner.className).toContain('flat');
    engine.status = { ...engine.status, tunerCents: 2 };
    await waitFor(() => expect(tuner.textContent).toContain('In tune'));
    expect(tuner.className).toContain('in-tune');
  });
  it('shows the gate opening and closing', async () => {
    render(<App/>);
    expect(await screen.findByText('Gate closed')).toBeTruthy();
    engine.status.gate = 1;
    expect(await screen.findByText('Gate open')).toBeTruthy();
  });
  it('loads a cabinet while the clean channel is active', async () => {
    render(<App/>);
    fireEvent.click(screen.getByRole('button', { name: 'Clean', exact: true }));
    fireEvent.click(screen.getByRole('button', { name: /RIG & TONE/ }));
    fireEvent.click(screen.getByRole('tab', { name: 'Rig' }));
    const cab = screen.getByRole('button', { name: /Load cabinet IR/ });
    expect(cab.disabled).toBe(false);
    fireEvent.click(cab);
    await waitFor(() => expect(engine.calls).toContainEqual(['loadIR']));
    expect(screen.getByRole('button', { name: 'Pedal enabled' }).disabled).toBe(true);
  });
  it('flags recent processing overruns in the footer', async () => {
    render(<App/>);
    await screen.findByText(/256 samples/);
    expect(screen.queryByText(/overruns/)).toBeNull();
    engine.status.overruns = 2;
    expect((await screen.findByText(/2 overruns/)).className).toBe('warn');
  });
});
