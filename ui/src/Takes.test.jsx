import { beforeEach, afterEach, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, waitFor, within } from '@testing-library/react';
const bridge = vi.hoisted(() => ({ invoke: vi.fn(), entries: [] }));
vi.mock('./juce/bridge.js', () => ({ native: true, invoke: bridge.invoke }));
import Takes from './components/Takes.jsx';
const status = {deviceSettingsAvailable: true, takes: {revision: 1, exporting: false}, practice: {recordMode: 0}, review: {duration: 60, position: 12, level: -12}};
beforeEach(() => {
  bridge.entries = [{id: 'one', name: 'Lead take', frames: 480000, sampleRate: 48000, originalRig: true, favorite: false, versions: [{id: 'v1', name: 'New clean tone', rigPath: 'Reamp v1.json'}]}, {id: 'two', name: 'Favorite clean', frames: 960000, sampleRate: 48000, favorite: true}];
  bridge.invoke.mockReset().mockImplementation(async name => name === 'getTakes' ? bridge.entries : '');
});
afterEach(cleanup);
it('lists and filters takes by name and favorite', async () => {
  render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  fireEvent.change(screen.getByLabelText('Search takes'), {target: {value: 'clean'}});
  expect(screen.queryByText('Lead take')).toBeNull(); expect(screen.getByText('★ Favorite clean')).toBeTruthy();
  fireEvent.change(screen.getByLabelText('Search takes'), {target: {value: ''}}); fireEvent.click(screen.getByLabelText('Favorites'));
  expect(screen.queryByText('Lead take')).toBeNull();
});
it('edits labels without renaming audio and sends a chosen version for review', async () => {
  render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  fireEvent.change(screen.getByLabelText('Take name'), {target: {value: 'Neoclassical lead'}}); fireEvent.click(screen.getByLabelText('Favorite take'));
  fireEvent.click(screen.getByRole('button', {name: 'Save'}));
  fireEvent.change(screen.getByLabelText('Take version'), {target: {value: 'v1'}}); fireEvent.click(screen.getByRole('button', {name: 'Listen'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('editTake', 'one', 'Neoclassical lead', true));
  expect(bridge.invoke).toHaveBeenCalledWith('previewTake', 'one', 'v1');
});
it('exports with the current rig and exposes cancel/progress while busy', async () => {
  const {rerender} = render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  fireEvent.click(screen.getByRole('button', {name: 'Reamp with current rig'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('reampTake', 'one', 2));
  rerender(<Takes status={{...status, takes: {...status.takes, exporting: true, progress: .4}}} onError={vi.fn()}/>);
  expect(screen.getByRole('button', {name: 'Reamp with current rig'}).disabled).toBe(true); expect(screen.getByLabelText('Audio export progress').value).toBe(.4);
  fireEvent.click(screen.getByRole('button', {name: 'Cancel export'})); await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('cancelReamp'));
});
it('blocks review/export during recording and surfaces worker errors', async () => {
  render(<Takes status={{...status, practice: {recordMode: 3}, takes: {...status.takes, error: 'Dry file is missing'}}} onError={vi.fn()}/>); await screen.findByText('Lead take');
  expect(screen.getByRole('button', {name: 'Listen'}).disabled).toBe(true); expect(screen.getByRole('button', {name: 'Reamp with current rig'}).disabled).toBe(true);
  expect(screen.getByRole('button', {name: 'Export for video'}).disabled).toBe(true);
  expect(screen.getByRole('alert').textContent).toBe('Dry file is missing');
});
it('exports the selected take version with independent soundtrack balance', async () => {
  bridge.entries[0].hasBacking = true;
  render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  fireEvent.change(screen.getByLabelText('Take version'),{target:{value:'v1'}});
  fireEvent.change(screen.getByLabelText('Video guitar balance'),{target:{value:'3'}});
  fireEvent.change(screen.getByLabelText('Video backing balance'),{target:{value:'-6'}});
  fireEvent.click(screen.getByRole('button',{name:'Export for video'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('exportVideoAudio','one','v1',true,3,-6,0,10,.01));
  fireEvent.click(screen.getByLabelText('Include recorded backing'));
  fireEvent.click(screen.getByRole('button',{name:'Export for video'}));
  expect(bridge.invoke).toHaveBeenCalledWith('exportVideoAudio','one','v1',false,3,-6,0,10,.01);
});
it('trims extended reamps, validates ranges and resets to the selected version duration', async () => {
  bridge.entries[0].versions[0].frames = 720000;
  render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  fireEvent.change(screen.getByLabelText('Take version'), {target: {value: 'v1'}});
  expect(screen.getByLabelText('Export end seconds').value).toBe('15');
  fireEvent.change(screen.getByLabelText('Export start seconds'), {target: {value: '2'}});
  fireEvent.change(screen.getByLabelText('Export end seconds'), {target: {value: '1'}});
  expect(screen.getByRole('button', {name: 'Export for video'}).disabled).toBe(true);
  fireEvent.change(screen.getByLabelText('Export end seconds'), {target: {value: ''}});
  expect(screen.getByRole('button', {name: 'Export for video'}).disabled).toBe(true);
  fireEvent.change(screen.getByLabelText('Export end seconds'), {target: {value: '14'}});
  fireEvent.change(screen.getByLabelText('Export fade milliseconds'), {target: {value: '25'}});
  fireEvent.click(screen.getByRole('button', {name: 'Export for video'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('exportVideoAudio', 'one', 'v1', false, 0, 0, 2, 14, .025));
  fireEvent.click(screen.getByRole('button', {name: 'Full take'}));
  expect(screen.getByLabelText('Export start seconds').value).toBe('0');
  expect(screen.getByLabelText('Export end seconds').value).toBe('15');
  fireEvent.change(screen.getByLabelText('Take version'), {target: {value: 'processed'}});
  expect(screen.getByLabelText('Export end seconds').value).toBe('10');
});
it('lets dry rigs omit the extra tail and resets export settings for another take', async () => {
  render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  fireEvent.change(screen.getByLabelText('Reamp effect tail'), {target: {value: '0'}});
  fireEvent.click(screen.getByRole('button', {name: 'Reamp with current rig'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('reampTake', 'one', 0));
  fireEvent.change(screen.getByLabelText('Video guitar balance'), {target: {value: '5'}});
  fireEvent.change(screen.getByLabelText('Export fade milliseconds'), {target: {value: '80'}});
  fireEvent.click(screen.getByText('★ Favorite clean'));
  expect(screen.getByLabelText('Export end seconds').value).toBe('20');
  expect(screen.getByLabelText('Video guitar balance').value).toBe('0');
  expect(screen.getByLabelText('Export fade milliseconds').value).toBe('10');
});
it('leaves standalone-only actions unavailable in a DAW', () => {
  render(<Takes status={{...status, deviceSettingsAvailable: false}} onError={vi.fn()}/>);
  expect(screen.getByRole('button', {name: 'Import take folder'}).disabled).toBe(true);
  expect(screen.getByText(/Open standalone/)).toBeTruthy(); expect(bridge.invoke).not.toHaveBeenCalled();
});
it('recovers the original or selected reamp rig and disables missing snapshots', async () => {
  render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  fireEvent.click(screen.getByRole('button', {name: 'Load recorded rig'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('restoreTakeRig', 'one', 'processed'));
  fireEvent.change(screen.getByLabelText('Take version'), {target: {value: 'dry'}});
  fireEvent.click(screen.getByRole('button', {name: 'Load recorded rig'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('restoreTakeRig', 'one', 'dry'));
  fireEvent.change(screen.getByLabelText('Take version'), {target: {value: 'v1'}});
  fireEvent.click(screen.getByRole('button', {name: 'Load reamp rig'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('restoreTakeRig', 'one', 'v1'));
  fireEvent.click(screen.getByText('★ Favorite clean'));
  expect(screen.getByRole('button', {name: 'Load recorded rig'}).disabled).toBe(true);
});
it('prevents duplicate recovery and exports until reading finishes, then reports failures', async () => {
  let finish; const error = vi.fn();
  bridge.invoke.mockImplementation(name => name === 'getTakes' ? Promise.resolve(bridge.entries) : new Promise(resolve => { finish = resolve; }));
  render(<Takes status={status} onError={error}/>); await screen.findByText('Lead take');
  fireEvent.click(screen.getByRole('button', {name: 'Load recorded rig'}));
  fireEvent.click(screen.getByRole('button', {name: 'Loading saved rig…'}));
  expect(bridge.invoke.mock.calls.filter(([name]) => name === 'restoreTakeRig')).toHaveLength(1);
  expect(screen.getByRole('button', {name: 'Reamp with current rig'}).disabled).toBe(true);
  expect(screen.getByRole('button', {name: 'Export for video'}).disabled).toBe(true);
  finish('Missing amp asset. Relink it in the Library first.');
  await waitFor(() => expect(error).toHaveBeenCalledWith({title: 'Takes', text: 'Missing amp asset. Relink it in the Library first.'}));
  expect(screen.getByRole('button', {name: 'Load recorded rig'}).disabled).toBe(false);
});
it('blocks recovery during recording, export, rig loading and incomplete recordings', async () => {
  const {rerender} = render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  for (const next of [{...status, practice: {recordMode: 3}}, {...status, takes: {...status.takes, exporting: true}}, {...status, rigLoading: true}]) {
    rerender(<Takes status={next} onError={vi.fn()}/>);
    expect(screen.getByRole('button', {name: 'Load recorded rig'}).disabled).toBe(true);
  }
  bridge.entries[0].incomplete = true;
  rerender(<Takes status={{...status, takes: {...status.takes, revision: 2}}} onError={vi.fn()}/>);
  await screen.findByText('This recording was interrupted. Check the audio before using it.');
  expect(screen.getByRole('button', {name: 'Load recorded rig'}).disabled).toBe(true);
  expect(bridge.invoke.mock.calls.some(([name]) => name === 'restoreTakeRig')).toBe(false);
});
it('sorts takes while keeping the selected take and version intact',async()=>{
  bridge.entries[0].created='2026-10-06T12:00:00Z'; bridge.entries[1].created='2026-10-05T12:00:00Z';
  render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  fireEvent.change(screen.getByLabelText('Take version'),{target:{value:'v1'}});
  const rows=()=>within(screen.getByLabelText('Recorded takes')).getAllByRole('button').map(b=>b.querySelector('strong').textContent);
  expect(rows()).toEqual(['Lead take','★ Favorite clean']);
  fireEvent.change(screen.getByLabelText('Take sort'),{target:{value:'favorites'}});
  expect(rows()).toEqual(['★ Favorite clean','Lead take']);
  expect(screen.getByLabelText('Take version').value).toBe('v1');
  fireEvent.change(screen.getByLabelText('Take sort'),{target:{value:'name'}});
  expect(rows()[0]).toBe('★ Favorite clean'); expect(screen.getByLabelText('Take name').value).toBe('Lead take');
});
it('renames only a selected reamp version and keeps original labels fixed',async()=>{
  const {rerender}=render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  expect(screen.queryByLabelText('Reamp version name')).toBeNull();
  fireEvent.change(screen.getByLabelText('Take version'),{target:{value:'v1'}});
  fireEvent.change(screen.getByLabelText('Reamp version name'),{target:{value:'  Singing lead  '}});
  fireEvent.click(screen.getByRole('button',{name:'Rename version'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('renameTakeVersion','one','v1','Singing lead'));
  rerender(<Takes status={{...status,takes:{...status.takes,exporting:true}}} onError={vi.fn()}/>);
  expect(screen.getByRole('button',{name:'Rename version'}).disabled).toBe(true);
});
it('quick-exports the full guitar version without backing, trim selections or mix gains',async()=>{
  bridge.entries[0].hasBacking=true; bridge.entries[0].versions[0].frames=720000;
  const {rerender}=render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  fireEvent.change(screen.getByLabelText('Take version'),{target:{value:'v1'}});
  fireEvent.change(screen.getByLabelText('Export start seconds'),{target:{value:'2'}});
  fireEvent.change(screen.getByLabelText('Video guitar balance'),{target:{value:'6'}});
  fireEvent.click(screen.getByRole('button',{name:'Export guitar WAV'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('exportVideoAudio','one','v1',false,0,0,0,15,.01));
  rerender(<Takes status={{...status,practice:{recordMode:3}}} onError={vi.fn()}/>);
  expect(screen.getByRole('button',{name:'Export guitar WAV'}).disabled).toBe(true);
});
