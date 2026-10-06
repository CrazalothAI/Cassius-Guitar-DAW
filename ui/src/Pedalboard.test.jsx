import React from 'react';
import { afterEach, describe, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, waitFor, within } from '@testing-library/react';
import Pedalboard from './components/Pedalboard.jsx';
import { boardId, boardOnId, boardParameters, boardControls, boardTypes, byId } from './parameters.js';
import { readParameter, setParameter } from './parameterState.js';
const bridge = vi.hoisted(() => ({native: true, invoke: vi.fn()}));
vi.mock('./juce/bridge.js', () => ({get native() { return bridge.native; }, invoke: bridge.invoke, slider: () => null}));
afterEach(() => { cleanup(); bridge.native = true; bridge.invoke.mockReset(); });
const block = (type, slot, lane = 'post') => { const kind = boardTypes.indexOf(type); return {id: `${type}.${slot}`, type, automationSlot: slot, lane, enabledId: boardOnId(kind, slot), trimId: `BOARD_${type.replaceAll('-', '_').toUpperCase()}_${slot}_TRIM`}; };
const status = rows => ({board: {serial: true, blocks: rows, reserved: rows.length, canUndo: true, canRedo: false}, libraryRevision: 1});
describe('independent serial pedalboard', () => {
  it('defines unique appended controls and independent kind/slot bindings', () => {
    expect(new Set(boardParameters.map(p => p.id)).size).toBe(boardParameters.length);
    expect(boardId(3, 0, 'EQ_FOCUS')).toBe('EQ_FOCUS');
    expect(boardId(3, 1, 'EQ_FOCUS')).toBe('BOARD_EQ_1_EQ_FOCUS');
    for (const [kind, controls] of boardControls.entries()) for (const slot of [0, 1]) {
      expect(byId[boardOnId(kind, slot)]).toBeTruthy();
      controls.forEach(id => expect(byId[boardId(kind, slot, id)]).toBeTruthy());
    }
  });
  it('renders pre/post order and moves a stable identity', async () => {
    bridge.invoke.mockResolvedValue(''); render(<Pedalboard status={status([block('eq', 0), block('eq', 1)])}/>);
    fireEvent.click(screen.getByRole('button', {name: 'Move EQ 2 earlier'}));
    await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('boardCommand', 'move', {id: 'eq.1', direction: -1}));
    expect(screen.getByRole('button', {name: 'Move EQ 1 earlier'}).disabled).toBe(true);
    expect(screen.getByRole('region', {name: 'Before amp'})).toBeTruthy();
  });
  it('edits the selected duplicate without writing the original parameter', () => {
    bridge.invoke.mockResolvedValue({assets: []}); setParameter('EQ_FOCUS', 2); setParameter('BOARD_EQ_1_EQ_FOCUS', -2);
    render(<Pedalboard status={status([block('eq', 0), block('eq', 1)])}/>);
    fireEvent.click(screen.getByRole('button', {name: /02 EQ 2/}));
    fireEvent.change(within(screen.getByRole('region', {name: 'Selected pedal controls'})).getByRole('slider', {name: 'Focus'}), {target: {value: '6'}});
    expect(readParameter('EQ_FOCUS')).toBe(2); expect(readParameter('BOARD_EQ_1_EQ_FOCUS')).toBe(6);
  });
  it('routes mono drive additions before the amp', async () => {
    bridge.invoke.mockResolvedValue(''); render(<Pedalboard status={status([])}/>);
    fireEvent.change(screen.getByRole('combobox', {name: 'New pedal type'}), {target: {value: 'neural-pedal'}});
    expect(screen.getByRole('combobox', {name: 'New pedal position'}).disabled).toBe(true);
    fireEvent.click(screen.getByRole('button', {name: 'Add pedal'}));
    await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('boardCommand', 'add', {type: 'neural-pedal', lane: 'pre'}));
  });
  it('assigns only matching library assets to the selected capture slot', async () => {
    bridge.invoke.mockImplementation(name => Promise.resolve(name === 'getLibrary' ? {assets: [{id: 'ambience:abc', kind: 'ambience', name: 'Hall'}, {id: 'cab:xyz', kind: 'cab', name: 'Cabinet'}]} : ''));
    render(<Pedalboard status={status([block('ambience', 1)])}/>);
    expect(screen.getByRole('button', {name: 'Bypass Ambience 2'}).disabled).toBe(true);
    await screen.findByRole('option', {name: 'Hall'}); expect(screen.queryByRole('option', {name: 'Cabinet'})).toBeNull();
    fireEvent.change(screen.getByRole('combobox', {name: 'Ambience response'}), {target: {value: 'ambience:abc'}});
    await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('boardCommand', 'capture', {id: 'ambience.1', assetId: 'ambience:abc'}));
  });
  it('blocks structural edits while a graph prepares and reports rejected edits', async () => {
    const onError = vi.fn(); bridge.invoke.mockImplementation(name => Promise.resolve(name === 'boardCommand' ? 'Both slots reserved' : {assets: []}));
    const {rerender} = render(<Pedalboard status={{...status([block('eq', 0)]), rigLoading: true}} onError={onError}/>);
    expect(screen.getByRole('button', {name: 'Duplicate'}).disabled).toBe(true);
    rerender(<Pedalboard status={status([block('eq', 0)])} onError={onError}/>); fireEvent.click(screen.getByRole('button', {name: 'Duplicate'}));
    await waitFor(() => expect(onError).toHaveBeenCalledWith({title: 'Pedalboard', text: 'Both slots reserved'}));
  });
  it('requires native audio before converting a legacy rig', () => {
    bridge.native = false; render(<Pedalboard status={{}}/>);
    expect(screen.getByRole('button', {name: 'Enable serial editing'}).disabled).toBe(true);
  });
  it('sends replacement and drag placement using stable identities', async () => {
    bridge.invoke.mockResolvedValue(''); render(<Pedalboard status={status([block('eq', 0), block('chorus', 0)])}/>);
    fireEvent.change(screen.getByRole('combobox', {name: 'Replacement pedal type'}), {target: {value: 'delay'}});
    fireEvent.click(screen.getByRole('button', {name: 'Replace pedal'}));
    await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('boardCommand', 'replace', {id: 'eq.0', type: 'delay'}));
    fireEvent.drop(screen.getByRole('button', {name: /01 EQ 1/}).closest('article'), {dataTransfer: {getData: () => 'chorus.0'}});
    await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('boardCommand', 'moveTo', {id: 'chorus.0', beforeId: 'eq.0', lane: 'post'}));
  });
});
