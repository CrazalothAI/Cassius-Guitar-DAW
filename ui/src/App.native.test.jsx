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
    if (name === 'applyRig' || name === 'loadRig' || name === 'loadStartingRig' || name === 'saveRig' || name === 'updateActiveRig' || name === 'practiceControl') return engine.error;
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
const stage = name => { if (!screen.queryByRole('tab', {name})) fireEvent.click(screen.getByRole('tab', {name: 'Board'})); fireEvent.click(screen.getByRole('tab', {name})); };
describe('editor connected to the audio engine', () => {
  it('selects an exact capture rig in the header without the legacy amp switch', async () => {
    const recipe = (await import('./startingRigs.json')).default.captureRigs.find(r => r.id === 'factory.capture-red2-tight');
    engine.library.assets = Object.values(recipe.assets).map(a => ({...a,missing:false}));
    render(<App/>); const preset = screen.getByRole('combobox',{name:'Preset'});
    await waitFor(() => expect(within(preset).getByRole('option',{name:'5153 Red-II Tight'}).disabled).toBe(false));
    fireEvent.change(preset,{target:{value:recipe.id}});
    await waitFor(() => expect(engine.calls).toContainEqual(['loadStartingRig',recipe.id]));
    expect(engine.calls.some(([name]) => name === 'selectAmpVoice')).toBe(false);
    engine.status = {...engine.status,activeRigId:recipe.id,activeRigStarter:true,activeRigEdited:false};
    await waitFor(() => expect(document.querySelector('.preset-name').textContent).toBe('5153 Red-II Tight'));
  });
  it('loads complete starters through native recall and reports a rejected load', async () => {
    render(<App/>); fireEvent.click(screen.getByRole('button', {name: 'Library'})); const dialog = screen.getByRole('dialog');
    fireEvent.click(within(dialog).getByRole('button', {name: 'Presets'}));
    fireEvent.change(within(dialog).getByRole('combobox', {name: 'Library rig type'}), {target: {value: 'starter'}});
    const use = within(within(dialog).getByText('Neoclassical Lead').closest('article')).getByRole('button', {name: 'Use'});
    engine.error = 'Finish loading before selecting a starter rig.'; fireEvent.click(use);
    expect((await within(dialog).findByRole('alert')).textContent).toBe(engine.error);
    engine.error = ''; fireEvent.click(use); await waitFor(() => expect(screen.queryByRole('dialog')).toBeNull());
    expect(engine.calls).toContainEqual(['loadStartingRig', 'factory.neoclassical-lead']);
    expect(engine.calls.some(([name]) => name === 'selectAmpVoice')).toBe(false);
  });
  it('searches imported source packs, combines tone filters and saves corrections', async () => {
    engine.library = {assets: [
      {id: 'a', kind: 'amp', name: 'Clean V30', ownership: 'User', pack: 'Clean Pack.zip', path: 'a.nam', notes: 'Local capture'},
      {id: 'b', kind: 'amp', name: 'Lead', ownership: 'User', pack: 'Lead Pack.zip', gain: 'high-gain', path: 'b.nam'}
    ], rigs: []};
    render(<App/>); fireEvent.click(screen.getByRole('button', {name: 'Library'})); const dialog = screen.getByRole('dialog');
    await within(dialog).findByText('Clean V30');
    fireEvent.change(within(dialog).getByRole('combobox', {name: 'Library source pack'}), {target: {value: 'Clean Pack.zip'}});
    fireEvent.change(within(dialog).getByRole('combobox', {name: 'Library gain'}), {target: {value: 'clean'}});
    fireEvent.change(within(dialog).getByRole('combobox', {name: 'Library speaker'}), {target: {value: 'V30'}});
    expect(within(dialog).queryByText('Ferrum')).toBeNull(); expect(within(dialog).queryByRole('button', {name: 'Favorite Lead'})).toBeNull();
    fireEvent.click(within(dialog).getByText('Clean V30'));
    fireEvent.change(within(dialog).getByRole('combobox', {name: 'Asset gain'}), {target: {value: 'breakup'}});
    fireEvent.click(within(dialog).getByRole('button', {name: 'Save metadata'}));
    await waitFor(() => expect(engine.calls.some(([name,id,changes]) => name === 'editAsset' && id === 'a' && changes.gain === 'breakup' && changes.speaker === 'V30')).toBe(true));
    fireEvent.click(within(dialog).getByRole('button', {name: 'Clear filters'})); expect(within(dialog).getByText('Ferrum')).toBeTruthy();
  });
  it('shows authoritative rig identity, saves in place, and keeps failed Save As open for retry', async () => {
    engine.status = {...engine.status, activeRigId: 'lead', activeRigName: 'Quiet lead', activeRigSaved: true, activeRigEdited: true, model: 'Ivory red'};
    render(<App/>);
    const bar = within(screen.getByRole('region', {name: 'Current complete rig'}));
    await bar.findByText('Quiet lead'); expect(bar.getByText('Edited')).toBeTruthy();
    fireEvent.click(bar.getByRole('button', {name: 'Save rig'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['updateActiveRig']));
    engine.status = {...engine.status, activeRigEdited: false};
    await waitFor(() => expect(bar.queryByText('Edited')).toBeNull());
    fireEvent.click(bar.getByRole('button', {name: 'Save rig as'}));
    const dialog = screen.getByRole('dialog', {name: 'Save complete rig'});
    fireEvent.change(within(dialog).getByRole('textbox', {name: 'Rig name'}), {target: {value: 'New lead'}});
    engine.error = 'Could not save the shared library';
    fireEvent.click(within(dialog).getByRole('button', {name: 'Save complete rig'}));
    expect((await within(dialog).findByRole('alert')).textContent).toBe(engine.error);
    expect(bar.getByText('Quiet lead')).toBeTruthy();
    engine.error = ''; fireEvent.click(within(dialog).getByRole('button', {name: 'Save complete rig'}));
    await waitFor(() => expect(screen.queryByRole('dialog')).toBeNull());
    expect(engine.calls).toContainEqual(['saveRig', 'New lead']);
  });
  it('keeps take review stoppable after navigating away from Takes', async () => {
    engine.status = {...engine.status, review: {playing: true}, practice: {recordMode: 3}};
    render(<App/>); const stop = await screen.findByRole('button', {name: 'Stop take review'});
    fireEvent.click(screen.getByRole('tab', {name: 'Board'})); fireEvent.click(stop);
    await waitFor(() => expect(engine.calls).toContainEqual(['reviewControl', 'stop', 0]));
    fireEvent.click(screen.getByRole('button', {name: 'Recording · Open Practice'}));
    expect(screen.getByRole('tab', {name: 'Practice'}).getAttribute('aria-selected')).toBe('true');
  });
  it('shows backing-volume failures inside Mix so the modal does not hide them', async () => {
    engine.status = {...engine.status, deviceSettingsAvailable: true, practice: {duration: 60, level: -12}};
    render(<App/>); await screen.findByText(/48.0 kHz/);
    fireEvent.click(screen.getByRole('button', {name: 'Mix'}));
    const dialog = screen.getByRole('dialog', {name: 'Play along mix'});
    engine.error = 'Backing volume unavailable';
    fireEvent.change(within(dialog).getByRole('slider', {name: 'Cassian backing volume'}), {target: {value: '-15'}});
    expect((await within(dialog).findByRole('alert')).textContent).toContain('Backing volume unavailable');
  });
  it('uses a compact amplifier in Practice and restores the full head in Tone', async () => {
    engine.status = {...engine.status, deviceSettingsAvailable: true, practice: {duration: 60, position: 12, track: 'Track.wav', recordMode: 0}};
    render(<App/>); fireEvent.click(screen.getByRole('tab', {name: 'Practice'}));
    await screen.findByText('Track.wav');
    expect(screen.getByRole('region', {name: 'Compact amplifier'})).toBeTruthy();
    expect(screen.queryByRole('region', {name: 'Amplifier'})).toBeNull();
    fireEvent.click(screen.getByRole('button', {name: 'Load backing track'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['loadBackingTrack']));
    fireEvent.click(screen.getByRole('tab', {name: 'Tone'}));
    expect(screen.getByRole('region', {name: 'Amplifier'})).toBeTruthy();
    expect(screen.getByRole('tab', {name: 'Amp'})).toBeTruthy();
  });
  it('loads and removes cabinet B without replacing cabinet A', async () => {
    engine.status = {...engine.status, ir: 'Cabinet A.wav', irB: 'Cabinet B.wav'};
    render(<App/>); fireEvent.click(screen.getByRole('tab', {name: 'Cab'}));
    await screen.findByText('Cabinet B.wav');
    fireEvent.click(screen.getByRole('button', {name: /Change cabinet B/}));
    await waitFor(() => expect(engine.calls).toContainEqual(['loadIRB']));
    fireEvent.click(screen.getByRole('button', {name: 'Remove cabinet B IR'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['clearStage', 'cabB']));
    expect(engine.calls).not.toContainEqual(['clearStage', 'cab']);
    expect(screen.getByText('Cabinet A.wav')).toBeTruthy();
  });
  it('can place a library cabinet in the second slot', async () => {
    engine.library = {assets: [{id: 'cab:second', kind: 'cab', name: 'Ribbon cab', ownership: 'User', path: 'C:/Cab.wav'}], rigs: []};
    render(<App/>); fireEvent.click(screen.getByRole('button', {name: 'Library'}));
    const dialog = screen.getByRole('dialog'); fireEvent.click(within(dialog).getByRole('button', {name: 'Cabinets'}));
    fireEvent.click(await within(dialog).findByRole('button', {name: /^Ribbon cab/}));
    fireEvent.click(within(dialog).getByRole('button', {name: 'Use as cabinet B'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['selectAsset', 'cab:second', 'cabB']));
  });
  it('lets a standalone user choose a physical input and open driver settings', async () => {
    engine.status = {...engine.status, deviceSettingsAvailable: true, inputChannels: ['Instrument 1', 'Instrument 2', 'Line 3'], selectedInput: 0};
    render(<App/>);
    const input = await screen.findByRole('combobox', {name: 'Guitar input'});
    fireEvent.change(input, {target: {value: '1'}});
    await waitFor(() => expect(engine.calls).toContainEqual(['setInputChannel', 1]));
    fireEvent.click(screen.getByRole('button', {name: 'Audio settings'}));
    await waitFor(() => expect(engine.calls).toContainEqual(['showAudioSettings']));
    engine.status = {...engine.status, selectedInput: 1};
    await waitFor(() => expect(input.value).toBe('1'));
  });
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
    stage('Pedal');
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
    fireEvent.click(screen.getByRole('tab', {name: 'Board'}));
    const input = screen.getByRole('tab', { name: 'Input' });
    await waitFor(() => expect(input.textContent).toContain('Gate closed'));
    engine.status.gate = 1;
    await waitFor(() => expect(input.textContent).toContain('Gate open'));
  });
  it('reports the hum filter once it engages', async () => {
    render(<App/>);
    stage('Input');
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
    stage('Pedal');
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
    for (const [name, slot, tab] of [['Remove amp capture', 'amp', 'Amp'], ['Remove pedal capture', 'pedal', 'Pedal'], ['Remove cabinet IR', 'cab', 'Cab']]) {
      stage(tab);
      fireEvent.click(await screen.findByRole('button', { name }));
      await waitFor(() => expect(engine.calls).toContainEqual(['clearStage', slot]));
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
    expect(screen.getByRole('button', {name: 'Channel'}).getAttribute('aria-pressed')).toBe('false');
    expect(document.querySelector('.app-shell').classList.contains('clean')).toBe(true);
    expect(screen.getByRole('tab', {name: /^Amp/}).textContent).toContain('Rig');
    stage('Pedal');
    expect(screen.getByRole('button', {name: 'Pedal enabled'}).disabled).toBe(false);
    fireEvent.click(screen.getByRole('tab', {name: 'Tone'}));
    expect(screen.getByRole('button', {name: 'Channel'}).disabled).toBe(true);
    expect(screen.getByRole('tab', {name: 'Cab'}).textContent).toContain('Included in capture');
    fireEvent.click(screen.getByRole('tab', {name: 'Cab'}));
    expect(screen.getByText('Separate cabinet bypassed · full-rig capture')).toBeTruthy();
  });
});
