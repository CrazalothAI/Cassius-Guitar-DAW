import { afterEach, beforeEach, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
const bridge = vi.hoisted(() => ({invoke: vi.fn(), native: true}));
vi.mock('./juce/bridge.js', () => ({get native() { return bridge.native; }, invoke: bridge.invoke}));
import Scenes from './components/Scenes.jsx';
const slots = [{stored: true, name: 'Rhythm'}, {stored: true, name: 'Lead'}, {stored: false, name: ''}, {stored: true, name: 'Clean'}];
const status = {scenes: {slots, active: 0, edited: false, revision: 1}};
beforeEach(() => { bridge.native = true; bridge.invoke.mockReset().mockResolvedValue(''); });
afterEach(cleanup);
it('copies the selected saved scene and clearly identifies replacing a filled destination',async()=>{
  render(<Scenes status={status}/>);fireEvent.click(screen.getByRole('button',{name:'Edit scenes'}));
  expect(screen.getByRole('button',{name:'Copy saved scene'}).disabled).toBe(true);
  fireEvent.change(screen.getByLabelText('Scene name'),{target:{value:'Rhythm variation'}});
  fireEvent.change(screen.getByLabelText('Scene copy destination'),{target:{value:'2'}});
  fireEvent.click(screen.getByRole('button',{name:'Copy saved scene'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('copyScene',0,2,'Rhythm variation'));
  fireEvent.change(screen.getByLabelText('Scene copy destination'),{target:{value:'1'}});
  expect(screen.getByRole('button',{name:'Replace scene 2 with copy'})).toBeTruthy();
  fireEvent.click(screen.getByRole('button',{name:'Scene 3: Empty'}));
  expect(screen.getByLabelText('Scene copy destination').disabled).toBe(true);expect(screen.getByRole('button',{name:'Copy saved scene'}).disabled).toBe(true);
});
it('renames saved settings without recalling a scene while the editor is open',async()=>{
  render(<Scenes status={status}/>);fireEvent.click(screen.getByRole('button',{name:'Edit scenes'}));
  fireEvent.click(screen.getByRole('button',{name:'Scene 2: Lead'}));expect(bridge.invoke).not.toHaveBeenCalled();
  fireEvent.change(screen.getByLabelText('Scene name'),{target:{value:'  Melodic lead  '}});
  fireEvent.click(screen.getByRole('button',{name:'Rename scene'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('renameScene',1,'Melodic lead'));
});
it('disables renaming an empty slot, unchanged name, blank name or loading rig',()=>{
  const view=render(<Scenes status={status}/>);fireEvent.click(screen.getByRole('button',{name:'Edit scenes'}));
  expect(screen.getByRole('button',{name:'Rename scene'}).disabled).toBe(true);
  fireEvent.click(screen.getByRole('button',{name:'Scene 3: Empty'}));expect(screen.getByRole('button',{name:'Rename scene'}).disabled).toBe(true);
  fireEvent.click(screen.getByRole('button',{name:'Scene 1: Rhythm'}));fireEvent.change(screen.getByLabelText('Scene name'),{target:{value:'   '}});
  expect(screen.getByRole('button',{name:'Rename scene'}).disabled).toBe(true);
  fireEvent.change(screen.getByLabelText('Scene name'),{target:{value:'Heavy'}});view.rerender(<Scenes status={{...status,rigLoading:true}}/>);
  expect(screen.getByRole('button',{name:'Rename scene'}).disabled).toBe(true);
});
it('recalls a stored scene and distinguishes the active tone from the edit slot', async () => {
  render(<Scenes status={status}/>);
  expect(screen.getByRole('button', {name: 'Scene 1: Rhythm'}).getAttribute('aria-pressed')).toBe('true');
  fireEvent.click(screen.getByRole('button', {name: 'Scene 2: Lead'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('recallScene', 1));
  fireEvent.click(screen.getByRole('button', {name: 'Edit scenes'}));
  expect(screen.getByLabelText('Scene name').value).toBe('Lead');
  expect(screen.getByRole('button', {name: 'Replace with current tone'})).toBeTruthy();
});
it('selects an empty slot without recalling and stores its trimmed name', async () => {
  render(<Scenes status={status}/>);
  fireEvent.click(screen.getByRole('button', {name: 'Scene 3: Empty'}));
  expect(bridge.invoke).not.toHaveBeenCalled();
  fireEvent.click(screen.getByRole('button', {name: 'Edit scenes'}));
  expect(screen.getByRole('button', {name: 'Clear scene'}).disabled).toBe(true);
  fireEvent.change(screen.getByLabelText('Scene name'), {target: {value: '  Ambient clean  '}});
  fireEvent.click(screen.getByRole('button', {name: 'Store current tone'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('storeScene', 2, 'Ambient clean'));
});
it('clears only the selected slot and reports native failures', async () => {
  const onError = vi.fn(); bridge.invoke.mockResolvedValue('Finish loading the rig.');
  render(<Scenes status={status} onError={onError}/>);
  fireEvent.click(screen.getByRole('button', {name: 'Edit scenes'}));
  fireEvent.click(screen.getByRole('button', {name: 'Clear scene'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('clearScene', 0));
  await waitFor(() => expect(onError).toHaveBeenCalledWith({title: 'Scenes', text: 'Finish loading the rig.'}));
});
it('follows MIDI status, marks edits and preserves a draft name across recall revisions', () => {
  const view = render(<Scenes status={status}/>);
  fireEvent.click(screen.getByRole('button', {name: 'Edit scenes'}));
  fireEvent.change(screen.getByLabelText('Scene name'), {target: {value: 'New rhythm'}});
  view.rerender(<Scenes status={{scenes: {...status.scenes, active: 3, edited: true, revision: 2}}}/>);
  expect(screen.getByRole('button', {name: 'Scene 4: Clean'}).getAttribute('aria-pressed')).toBe('true');
  expect(screen.getByText('Edited')).toBeTruthy();
  expect(screen.getByLabelText('Scene name').value).toBe('New rhythm');
});
it('blocks changes while a rig loads and rejects empty names', () => {
  const view = render(<Scenes status={{...status, rigLoading: true}}/>);
  fireEvent.click(screen.getByRole('button', {name: 'Edit scenes'}));
  expect(screen.getByRole('button', {name: 'Scene 1: Rhythm'}).disabled).toBe(true);
  expect(screen.getByRole('button', {name: 'Replace with current tone'}).disabled).toBe(true);
  view.rerender(<Scenes status={status}/>);
  fireEvent.change(screen.getByLabelText('Scene name'), {target: {value: '   '}});
  expect(screen.getByRole('button', {name: 'Replace with current tone'}).disabled).toBe(true);
});
it('keeps browser preview read-only and surfaces invalid session banks', () => {
  bridge.native = false;
  render(<Scenes status={{scenes: {error: 'Invalid scene bank.'}}}/>);
  fireEvent.click(screen.getByRole('button', {name: 'Scene 2: Empty'}));
  fireEvent.click(screen.getByRole('button', {name: 'Edit scenes'}));
  expect(screen.getByRole('button', {name: 'Store current tone'}).disabled).toBe(true);
  expect(screen.getByText(/Browser preview has no native scene bank/)).toBeTruthy();
  expect(screen.getByRole('alert').textContent).toBe('Invalid scene bank.');
  expect(bridge.invoke).not.toHaveBeenCalled();
});
