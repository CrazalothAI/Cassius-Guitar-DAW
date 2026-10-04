import { afterEach, beforeEach, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
const bridge = vi.hoisted(() => ({invoke: vi.fn()}));
vi.mock('./juce/bridge.js', () => ({native: true, invoke: bridge.invoke}));
import Midi from './components/Midi.jsx';
const rows = Array.from({length: 8}, (_, i) => ({type: 'cc', channel: 0, number: i + 16, action: 'none', rig: '', inverted: false}));
const status = {deviceSettingsAvailable: true, libraryRevision: 1, midiInputs: [{id: 'controller', name: 'Footboard', enabled: false}], midi: {config: {enabled: false, mappings: rows}, revision: 1, learning: -1}};
beforeEach(() => bridge.invoke.mockReset().mockImplementation(name => Promise.resolve(name === 'getLibrary' ? {rigs: [{id: 'lead', name: 'My lead rig'}]} : '')));
afterEach(cleanup);
it('enables mapping and the selected physical controller separately', async () => {
  render(<Midi status={status} onError={vi.fn()}/>);
  fireEvent.click(screen.getByLabelText('Enable MIDI mapping'));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('setMidiEnabled', true));
  fireEvent.click(screen.getByLabelText('MIDI input Footboard'));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('setMidiInput', 'controller', true));
  expect(screen.getByText('Last input: Waiting for MIDI')).toBeTruthy();
});
it('applies saved-rig assignments, message types and channels', async () => {
  render(<Midi status={status} onError={vi.fn()}/>);
  fireEvent.change(screen.getByLabelText('MIDI action'), {target: {value: 'rig'}});
  await screen.findByRole('option', {name: 'My lead rig'});
  fireEvent.change(screen.getByLabelText('MIDI saved rig'), {target: {value: 'lead'}});
  fireEvent.change(screen.getByLabelText('MIDI message type'), {target: {value: 'pc'}});
  fireEvent.change(screen.getByLabelText('MIDI number'), {target: {value: '3'}});
  fireEvent.change(screen.getByLabelText('MIDI channel'), {target: {value: '2'}});
  expect(screen.getByRole('button', {name: 'Learn controller'}).disabled).toBe(true);
  fireEvent.click(screen.getByRole('button', {name: 'Apply assignment'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('setMidiMapping', 0, {...rows[0], action: 'rig', type: 'pc', rig: 'lead', number: 3, channel: 2}));
});
it('learns without firing an action, allows cancellation and updates the learned fields', async () => {
  const view = render(<Midi status={status} onError={vi.fn()}/>);
  fireEvent.click(screen.getByRole('button', {name: 'Learn controller'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('learnMidi', 0));
  view.rerender(<Midi status={{...status, midi: {...status.midi, learning: 0}}} onError={vi.fn()}/>);
  expect(screen.getByLabelText('MIDI number').disabled).toBe(true);
  expect(screen.getByText(/Listening for assignment 1/)).toBeTruthy();
  fireEvent.click(screen.getByRole('button', {name: 'Cancel learn'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('learnMidi', -1));
  view.rerender(<Midi status={{...status, midi: {...status.midi, revision: 2, config: {...status.midi.config, mappings: [{...rows[0], number: 42, channel: 4}, ...rows.slice(1)]}}}} onError={vi.fn()}/>);
  expect(screen.getByLabelText('MIDI number').value).toBe('42');
  expect(screen.getByLabelText('MIDI channel').value).toBe('4');
});
it('forces CC for expression and submits reversed pedal direction', async () => {
  render(<Midi status={status} onError={vi.fn()}/>);
  fireEvent.change(screen.getByLabelText('MIDI message type'), {target: {value: 'pc'}});
  fireEvent.change(screen.getByLabelText('MIDI action'), {target: {value: 'master'}});
  expect(screen.getByLabelText('MIDI message type').value).toBe('cc');
  fireEvent.click(screen.getByLabelText('Invert expression'));
  fireEvent.click(screen.getByRole('button', {name: 'Apply assignment'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('setMidiMapping', 0, {...rows[0], action: 'master', inverted: true}));
});
it('shows DAW routing, missing targets, errors and queue limits', async () => {
  const onError = vi.fn(); bridge.invoke.mockResolvedValue('Message overlaps another assignment.');
  render(<Midi status={{...status, deviceSettingsAvailable: false, midi: {...status.midi, error: 'Rig not found.', dropped: 3, config: {...status.midi.config, mappings: [{...rows[0], action: 'rig', rig: 'missing'}, ...rows.slice(1)]}}}} onError={onError}/>);
  expect(screen.getByText(/Route MIDI to Cassian in your DAW/)).toBeTruthy();
  expect(screen.getByRole('option', {name: 'Missing saved rig'})).toBeTruthy();
  expect(screen.getByText('Rig not found.')).toBeTruthy();
  expect(screen.getByText(/3 MIDI messages exceeded/)).toBeTruthy();
  fireEvent.change(screen.getByLabelText('MIDI number'), {target: {value: '40'}});
  fireEvent.click(screen.getByRole('button', {name: 'Apply assignment'}));
  await waitFor(() => expect(onError).toHaveBeenCalledWith({title: 'MIDI', text: 'Message overlaps another assignment.'}));
});
it('cancels learning when the panel closes', () => {
  const view = render(<Midi status={{...status, midi: {...status.midi, learning: 2}}} onError={vi.fn()}/>);
  view.unmount(); expect(bridge.invoke).toHaveBeenCalledWith('learnMidi', -1);
});
