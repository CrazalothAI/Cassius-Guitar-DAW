import React from 'react';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, waitFor, within } from '@testing-library/react';
const engine = vi.hoisted(() => ({ status: {}, calls: [], rig: {}, library: {}, error: '' }));
vi.mock('./juce/bridge.js', () => ({
  native: true,
  slider: () => null,
  invoke: async (name, ...args) => {
    if (name === 'getStatus') return { ...engine.status };
    engine.calls.push([name, ...args]);
    if (name === 'getRig') return engine.rig;
    if (name === 'getLibrary') return engine.library;
    if (name === 'applyRig' || name === 'loadRig' || name === 'saveRig') return engine.error;
    return true;
  },
}));
import App from './App.jsx';
beforeEach(() => {
  engine.calls = [];
  engine.rig = {schema: 1, state: 'full native rig with file references'};
  engine.library = {assets: [], rigs: []}; engine.error = '';
  engine.status = { model: 'Rig.nam', ir: '', pedal: '', input: 0, output: 0, gate: 0, sampleRate: 48000, bufferSize: 256, cpu: 10, overruns: 0, message: 'Loaded Rig.nam' };
});
afterEach(cleanup);
describe('editor connected to the audio engine', () => {
  it('requests optional A/B matching without changing the default comparison', async () => {
    render(<App/>); fireEvent.click(screen.getByRole('checkbox', {name: 'Match A/B loudness'}));
    fireEvent.click(screen.getByRole('button', {name: 'A/B compare'}));
    await waitFor(() => expect(engine.calls.some(([name]) => name === 'getRig')).toBe(true));
    fireEvent.click(screen.getByRole('button', {name: 'A/B compare'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['applyRig', engine.rig, true]));
  });
  it('opens portable pack pickers separately from reference-only rig documents', async () => {
    render(<App/>); fireEvent.click(screen.getByRole('button', {name: 'Library'}));
    const dialog = screen.getByRole('dialog'); fireEvent.click(within(dialog).getByRole('button', {name: 'Presets'}));
    fireEvent.click(within(dialog).getByRole('button', {name: 'Export pack'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['exportRigPack']));
    fireEvent.click(within(dialog).getByRole('button', {name: 'Import pack'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['importRigPack']));
  });
  it('reports captures the engine resamples to the host rate', async () => {
    engine.status = { ...engine.status, sampleRate: 44100, ampExpectedRate: 48000, ampResampled: true, pedal: 'Drive.nam', pedalExpectedRate: 44100, pedalResampled: false };
    render(<App/>);
    expect(await screen.findByText('48.0 kHz capture · resampled to 44.1 kHz')).toBeTruthy();
    fireEvent.click(screen.getByRole('tab', { name: 'Pedal' }));
    expect(screen.getByText('Before the selected amp')).toBeTruthy();
    expect(screen.queryByRole('alert')).toBeNull();
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
  it('shows the gate opening and closing on the Input stage', async () => {
    render(<App/>);
    const input = screen.getByRole('tab', { name: 'Input' });
    await waitFor(() => expect(input.textContent).toContain('Gate closed'));
    engine.status.gate = 1;
    await waitFor(() => expect(input.textContent).toContain('Gate open'));
  });
  it('reports the hum filter once it engages', async () => {
    render(<App/>);
    fireEvent.click(screen.getByRole('tab', { name: 'Input' }));
    expect(await screen.findByText('Listening · engages if hum is heard between notes')).toBeTruthy();
    engine.status = { ...engine.status, humCancelling: true, mainsHz: 50 };
    expect(await screen.findByText('Removing 50 Hz hum')).toBeTruthy();
  });
  it('warns about dropouts and offers a larger buffer in the standalone app', async () => {
    engine.status = { ...engine.status, bufferSize: 128, bufferSizes: [64, 128, 256, 512], dropouts: 4 };
    render(<App/>);
    await screen.findByRole('combobox', { name: 'Buffer size' });
    expect(screen.queryByText('Audio is dropping out')).toBeNull();
    engine.status.dropouts = 6;
    expect(await screen.findByText('Audio is dropping out')).toBeTruthy();
    fireEvent.click(screen.getByRole('button', { name: 'Use 256 samples' }));
    await waitFor(() => expect(engine.calls).toContainEqual(['setBufferSize', 256]));
    fireEvent.change(screen.getByRole('combobox', { name: 'Buffer size' }), { target: { value: '512' } });
    await waitFor(() => expect(engine.calls).toContainEqual(['setBufferSize', 512]));
  });
  it('points a plugin user at the DAW buffer when blocks overrun', async () => {
    render(<App/>);
    await screen.findByText(/256 samples/);
    engine.status.overruns = 1;
    expect((await screen.findByText('Audio is dropping out')).closest('.alert').textContent).toContain('raise it in your DAW');
    expect(screen.queryByRole('button', { name: /Use .* samples/ })).toBeNull();
  });
  it('follows the DAW tempo while it plays', async () => {
    engine.status = { ...engine.status, metronomeFollowsHost: true, metronomeBpm: 97.6 };
    render(<App/>);
    fireEvent.click(screen.getByRole('button', { name: 'Metronome' }));
    expect(await screen.findByText('Following your DAW’s tempo and bars while it plays.')).toBeTruthy();
    expect(screen.getByRole('button', { name: 'Tap' }).disabled).toBe(true);
  });
  it('loads a cabinet while the clean channel is active', async () => {
    render(<App/>);
    fireEvent.change(screen.getByRole('combobox', { name: 'Preset' }), { target: { value: 'Glass clean' } });
    fireEvent.click(screen.getByRole('tab', { name: 'Cab' }));
    const cab = screen.getByRole('button', { name: /Load cabinet IR/ });
    expect(cab.disabled).toBe(false);
    fireEvent.click(cab);
    await waitFor(() => expect(engine.calls).toContainEqual(['loadIR']));
    fireEvent.click(screen.getByRole('tab', { name: 'Pedal' }));
    expect(screen.getByRole('button', { name: 'Pedal enabled' }).disabled).toBe(true);
  });
  it('runs the engine pitch analysis only while the tuner is open', async () => {
    render(<App/>);
    await waitFor(() => expect(engine.calls).toContainEqual(['setTuner', false]));
    fireEvent.click(screen.getByRole('button', { name: 'TUNER' }));
    await waitFor(() => expect(engine.calls.at(-1)).toEqual(['setTuner', true]));
    fireEvent.click(screen.getByRole('button', { name: 'TUNER ON' }));
    await waitFor(() => expect(engine.calls.at(-1)).toEqual(['setTuner', false]));
  });
  it('describes the built-in amp and speaker when no capture or IR is loaded', async () => {
    engine.status = { ...engine.status, model: '', fallbackAmp: true, speakerSim: true };
    render(<App/>);
    fireEvent.change(screen.getByRole('combobox', { name: 'Preset' }), { target: { value: 'Modern metalcore' } });
    expect(await screen.findByText('Ferrum built-in')).toBeTruthy();
    expect(screen.getByText('Built-in high-gain amp')).toBeTruthy();
    expect(screen.getByRole('tab', { name: 'Cab' }).textContent).toContain('Built-in 4×12');
    fireEvent.click(screen.getByRole('tab', { name: 'Cab' }));
    expect(screen.getByText('Built-in 4×12 speaker')).toBeTruthy();
  });
  it('shows how much a capture was level matched', async () => {
    engine.status = { ...engine.status, ampLevelled: true, ampLevelDb: 19.84, speakerSim: false };
    render(<App/>);
    fireEvent.change(screen.getByRole('combobox', { name: 'Preset' }), { target: { value: 'Modern metalcore' } });
    expect(await screen.findByText('Level matched +19.8 dB')).toBeTruthy();
  });
  it('removes a loaded capture, pedal or cabinet', async () => {
    engine.status = { ...engine.status, pedal: 'Drive.nam', ir: 'Cab.wav' };
    render(<App/>);
    for (const [name, stage, tab] of [['Remove amp capture', 'amp', 'Amp'], ['Remove pedal capture', 'pedal', 'Pedal'], ['Remove cabinet IR', 'cab', 'Cab']]) {
      fireEvent.click(screen.getByRole('tab', { name: tab }));
      fireEvent.click(await screen.findByRole('button', { name }));
      await waitFor(() => expect(engine.calls).toContainEqual(['clearStage', stage]));
    }
  });
  it('flags recent processing overruns in the footer', async () => {
    render(<App/>);
    await screen.findByText(/256 samples/);
    expect(screen.queryByText(/overruns/)).toBeNull();
    engine.status.overruns = 2;
    expect((await screen.findByText(/2 overruns/)).className).toBe('warn');
  });
  it('compares complete native rigs rather than only knob values', async () => {
    render(<App/>);
    fireEvent.click(screen.getByRole('button', {name: 'A/B compare'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['getRig']));
    engine.rig = {schema: 1, state: 'second amp and cabinet'};
    fireEvent.click(screen.getByRole('button', {name: 'A/B compare'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['applyRig', {schema: 1, state: 'full native rig with file references'}]));
    expect(screen.getByRole('button', {name: 'A/B compare'}).textContent).toBe('A/B · B');
    engine.error = 'Missing model asset. Relink it in the Library first.';
    fireEvent.click(screen.getByRole('button', {name: 'A/B compare'}));
    expect(await screen.findByText(/Missing model asset/)).toBeTruthy();
    expect(screen.getByRole('button', {name: 'A/B compare'}).textContent).toBe('A/B · B');
  });
  it('finds library captures by tags and relinks missing originals', async () => {
    engine.library.assets = [
      {id: 'amp:123', name: 'Ivory green', kind: 'amp', ownership: 'User', tags: 'jazz clean', favorite: true, missing: true},
      {id: 'amp:456', name: 'Ivory red', kind: 'amp', ownership: 'User', tags: 'metal lead', favorite: false},
    ];
    render(<App/>); fireEvent.click(screen.getByRole('button', {name: 'Library'}));
    const dialog = await screen.findByRole('dialog');
    await within(dialog).findByText('Ivory green');
    fireEvent.change(within(dialog).getByRole('textbox', {name: 'Search library'}), {target: {value: 'jazz'}});
    expect(within(dialog).queryByText('Ivory red')).toBeNull();
    fireEvent.click(within(dialog).getByRole('button', {name: 'Relink'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['relinkAsset', 'amp:123']));
    fireEvent.click(within(dialog).getByRole('button', {name: 'Import NAM files'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['importAssets', 'amp']));
  });
  it('saves named full rigs and reports native recall errors inside the library', async () => {
    engine.library.rigs = [{id: 'rig-one', name: 'Quiet lead'}];
    render(<App/>); fireEvent.click(screen.getByRole('button', {name: 'Library'}));
    const dialog = screen.getByRole('dialog');
    fireEvent.change(within(dialog).getByRole('textbox', {name: 'Rig name'}), {target: {value: 'My nylon'}});
    fireEvent.click(within(dialog).getByRole('button', {name: 'Save current rig'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['saveRig', 'My nylon']));
    const row = (await within(dialog).findByText('Quiet lead')).closest('article');
    engine.error = 'Missing cabinet asset';
    fireEvent.click(within(row).getByRole('button', {name: 'Use'}));
    expect(await within(dialog).findByRole('alert')).toHaveProperty('textContent', 'Missing cabinet asset');
    expect(screen.getByRole('dialog')).toBeTruthy();
  });
  it('keeps the universal pedal available and labels full-rig cabinets as included', async () => {
    engine.status = {...engine.status, pedal: 'Boost.nam', ir: 'Cab.wav', ampHasCab: true};
    render(<App/>);
    fireEvent.change(screen.getByRole('combobox', {name: 'Preset'}), {target: {value: 'Glass clean'}});
    fireEvent.change(screen.getByRole('combobox', {name: 'Amp source'}), {target: {value: '3'}});
    await screen.findByRole('button', {name: 'Change amp'});
    fireEvent.click(screen.getByRole('tab', {name: 'Pedal'}));
    expect(screen.getByRole('button', {name: 'Pedal enabled'}).disabled).toBe(false);
    expect(screen.getByRole('button', {name: 'Channel'}).disabled).toBe(true);
    expect(screen.getByRole('tab', {name: 'Cab'}).textContent).toContain('Included in capture');
    fireEvent.click(screen.getByRole('tab', {name: 'Cab'}));
    expect(screen.getByText('Separate cabinet bypassed · full-rig capture')).toBeTruthy();
  });
});
