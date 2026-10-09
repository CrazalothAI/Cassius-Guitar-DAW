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
it('enables streamed looping and explains loop-head prefetch without changing export ranges', async () => {
  const loaded = {...status, takes: {...status.takes, reviewId: 'one', reviewVersion: 'processed'}, review: {...status.review, streaming: true, loopAvailable: true, loop: true, loopPrefetchReady: false, a: 1, b: 3}};
  const {rerender} = render(<Takes status={loaded} onError={vi.fn()}/>); await screen.findByText('Lead take');
  const loop = screen.getByLabelText('Loop take review'); expect(loop.disabled).toBe(false); expect(loop.checked).toBe(true);
  expect(screen.getByText(/Preparing loop start/)).toBeTruthy();
  fireEvent.click(loop); await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('takeReviewControl', 'one', 'processed', 'loop', 0));
  rerender(<Takes status={{...loaded, review: {...loaded.review, loopPrefetchReady: true}}} onError={vi.fn()}/>);
  expect(screen.queryByText(/Preparing loop start/)).toBeNull();
});
it('explains streaming buffering, disables looping and keeps export ranges usable', async () => {
  const streamed = {...status, takes: {...status.takes, reviewId: 'one', reviewVersion: 'processed'}, review: {...status.review, streaming: true, loopAvailable: false, playing: true, buffering: true, reviewUnderruns: 2, a: 1, b: 3}};
  const {rerender} = render(<Takes status={streamed} onError={vi.fn()}/>); await screen.findByText('Lead take');
  expect(screen.getByLabelText('Loop take review').disabled).toBe(true);
  expect(screen.getByLabelText('Take review position').disabled).toBe(false);
  expect(screen.getByRole('status', {name: 'Take review status'}).textContent).toContain('Buffering');
  expect(screen.getByRole('status', {name: 'Take review status'}).textContent).toContain('Playback waits here');
  expect(screen.getByText(/Playback buffer waits: 2/)).toBeTruthy();
  fireEvent.click(screen.getByText('Video soundtrack'));
  expect(screen.getByRole('button', {name: 'Use review A–B'}).disabled).toBe(false);
  fireEvent.click(screen.getByRole('button', {name: 'Use review A–B'}));
  expect(screen.getByLabelText('Export start seconds').value).toBe('1');
  expect(screen.getByLabelText('Export end seconds').value).toBe('3');
  rerender(<Takes status={{...streamed, review: {...streamed.review, buffering: false, error: 'Take audio changed. Reload it.'}}} onError={vi.fn()}/>);
  expect(screen.getByRole('status', {name: 'Take review status'}).textContent).toContain('Playing');
  expect(screen.getByRole('alert').textContent).toContain('Take audio changed');
  rerender(<Takes status={{...streamed, takes: {...streamed.takes, reviewLoading: true}, review: {...streamed.review, loadProgress: .7}}} onError={vi.fn()}/>);
  expect(screen.getByRole('status', {name: 'Take review status'}).textContent).toContain('Preparing take review… 70%');
  expect(screen.getByLabelText('Take review position').disabled).toBe(true);
});
it('offers explicit interrupted-folder recovery and disables it during recording/export/backup', async () => {
  const {rerender}=render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  const choose=screen.getByRole('button',{name:'Choose interrupted take folder'});
  fireEvent.click(choose); await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('recoverRecording'));
  for(const next of [{...status,practice:{recordMode:3}},{...status,takes:{...status.takes,exporting:true}},{...status,backup:{busy:true}},{...status,deviceSettingsAvailable:false}]) {
    rerender(<Takes status={next} onError={vi.fn()}/>); expect(choose.disabled).toBe(true);
  }
});
it('requires explicit listening and review before confirming a recovered take', async () => {
  bridge.entries[0].incomplete=true; bridge.entries[0].recovered=true;
  const error=vi.fn(), {rerender}=render(<Takes status={status} onError={error}/>); await screen.findByText('Lead take');
  const confirm=screen.getByRole('button',{name:'Confirm recovered take'});
  expect(confirm.disabled).toBe(true);
  fireEvent.click(screen.getByLabelText('I listened to the recovered processed audio')); expect(confirm.disabled).toBe(true);
  const listened={...status,takes:{...status.takes,reviewId:'one',reviewVersion:'processed'}};
  rerender(<Takes status={listened} onError={error}/>); expect(confirm.disabled).toBe(false);
  fireEvent.click(confirm); await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('confirmTakeRecovery','one',true,false));
  expect(screen.getByRole('button',{name:'Export guitar WAV'}).disabled).toBe(true);
  rerender(<Takes status={{...listened,takes:{...listened.takes,reviewVersion:'dry'}}} onError={error}/>); expect(confirm.disabled).toBe(true);
});
it('allows explicit external-player review and clears confirmation on take selection changes', async () => {
  bridge.entries[0].incomplete=true; bridge.entries[0].recovered=true;
  render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  const confirm=screen.getByRole('button',{name:'Confirm recovered take'});
  fireEvent.click(screen.getByLabelText('I reviewed Guitar processed.wav in another player')); expect(confirm.disabled).toBe(true);
  fireEvent.click(screen.getByLabelText('I listened to the recovered processed audio')); expect(confirm.disabled).toBe(false);
  fireEvent.click(confirm); await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('confirmTakeRecovery','one',true,true));
  fireEvent.click(screen.getByRole('button',{name:/Favorite clean/})); fireEvent.click(screen.getByRole('button',{name:/Lead take/}));
  expect(screen.getByLabelText('I reviewed Guitar processed.wav in another player').checked).toBe(false);
  expect(screen.getByRole('button',{name:'Confirm recovered take'}).disabled).toBe(true);
});
it('shows recovery progress and offers cancellation and recovered-file access', async () => {
  const progress={...status,takes:{...status.takes,exporting:true,recoveringRecording:true,progress:.4}};
  const {rerender}=render(<Takes status={progress} onError={vi.fn()}/>); await screen.findByText('Lead take');
  expect(screen.getByRole('progressbar',{name:'Recording recovery progress'}).value).toBe(.4);
  fireEvent.click(screen.getByRole('button',{name:'Cancel recovery'})); await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('cancelReamp'));
  rerender(<Takes status={{...status,takes:{...status.takes,lastRecoverySummary:'Recovered 10 seconds. Listen before confirming.',lastRecoveryId:'one',lastRecoveryPath:'local copy'}}} onError={vi.fn()}/>);
  expect(screen.getByRole('status',{name:'Recording recovery result'}).textContent).toContain('Listen before confirming');
  fireEvent.click(screen.getByRole('button',{name:'Open recovered files'})); await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('revealRecordingRecovery'));
});
it('copies the exact loaded review range for export without following later loop edits', async () => {
  bridge.entries[0].versions[0].frames = 720000;
  const loaded = {...status, takes: {...status.takes, reviewId: 'one', reviewVersion: 'v1'}, review: {...status.review, a: 2.125, b: 13.75}};
  const view=render(<Takes status={loaded} onError={vi.fn()}/>); await screen.findByText('Lead take');
  expect(screen.getByRole('button', {name: 'Use review A–B'}).disabled).toBe(true);
  fireEvent.change(screen.getByLabelText('Take version'), {target: {value: 'v1'}});
  fireEvent.click(screen.getByRole('button', {name: 'Use review A–B'}));
  expect(screen.getByLabelText('Export start seconds').value).toBe('2.125'); expect(screen.getByLabelText('Export end seconds').value).toBe('13.75');
  view.rerender(<Takes status={{...loaded, review: {...loaded.review, a: 4, b: 10}}} onError={vi.fn()}/>);
  expect(screen.getByLabelText('Export start seconds').value).toBe('2.125');
  fireEvent.change(screen.getByLabelText('Export fade milliseconds'), {target: {value: '30'}});
  fireEvent.change(screen.getByLabelText('Video guitar balance'), {target: {value: '9'}});
  fireEvent.click(screen.getByRole('button', {name: 'Export range as guitar WAV'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('exportVideoAudio','one','v1',false,0,0,2.125,13.75,.03));
  fireEvent.change(screen.getByLabelText('Take version'), {target: {value: 'dry'}});
  expect(screen.getByLabelText('Export end seconds').value).toBe('10'); expect(screen.getByRole('button', {name: 'Use review A–B'}).disabled).toBe(true);
});
it('blocks out-of-bounds and startup review ranges while permitting manual trims', async () => {
  const loaded={...status, takes:{...status.takes, reviewId:'one', reviewVersion:'processed'}, review:{...status.review, a:2, b:20}};
  const view=render(<Takes status={loaded} onError={vi.fn()}/>); await screen.findByText('Lead take');
  expect(screen.getByRole('button',{name:'Use review A–B'}).disabled).toBe(true);
  for(const state of [{a:2,b:2.01},{a:2,b:5,starting:true},{a:2,b:5,counting:true}]) {
    view.rerender(<Takes status={{...loaded,review:{...loaded.review,...state}}} onError={vi.fn()}/>);
    expect(screen.getByRole('button',{name:'Use review A–B'}).disabled).toBe(true);
  }
  view.rerender(<Takes status={{...loaded,review:{...loaded.review,a:2,b:10.00002}}} onError={vi.fn()}/>);
  expect(screen.getByRole('button',{name:'Use review A–B'}).disabled).toBe(false);
  fireEvent.click(screen.getByRole('button',{name:'Use review A–B'})); expect(screen.getByLabelText('Export end seconds').value).toBe('10');
  fireEvent.change(screen.getByLabelText('Export start seconds'),{target:{value:'3'}}); fireEvent.change(screen.getByLabelText('Export end seconds'),{target:{value:'7'}});
  expect(screen.getByRole('button',{name:'Export range as guitar WAV'}).disabled).toBe(false);
  fireEvent.change(screen.getByLabelText('Export end seconds'),{target:{value:'2'}});
  expect(screen.getByRole('button',{name:'Export range as guitar WAV'}).disabled).toBe(true);
});
it('shows the last completed export and measured peak protection independently of selection', async () => {
  const last={...status,takes:{...status.takes,lastExportPath:'C:/Video/solo.wav',lastExportReport:{source:'Lead take · Dry DI',duration:1.75,backing:false,attenuationDb:3.123}}};
  const view=render(<Takes status={last} onError={vi.fn()}/>); await screen.findByText('Lead take');
  expect(screen.getByText(/Lead take · Dry DI · 1.75 seconds · guitar only/)).toBeTruthy();
  expect(screen.getByText(/reduced this export by 3.12 dB/)).toBeTruthy(); fireEvent.click(screen.getByText('★ Favorite clean'));
  expect(screen.getByText(/Lead take · Dry DI · 1.75 seconds/)).toBeTruthy();
  fireEvent.click(screen.getByRole('button',{name:'Show exported audio'})); await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('revealVideoExport'));
  view.rerender(<Takes status={{...last,takes:{...last.takes,lastExportReport:{...last.takes.lastExportReport,attenuationDb:0}}}} onError={vi.fn()}/>);
  expect(screen.getByText(/No peak attenuation was needed/)).toBeTruthy();
});
it('shows sections only for the loaded version and discards selection when versions change', async () => {
  const loaded = {...status, takes: {...status.takes, reviewId: 'one', reviewVersion: 'processed'}, review: {...status.review, sectionRevision: 1, a: 1, b: 4, sections: [{id: 'phrase', name: 'Fast phrase', a: 1, b: 4}]}};
  const view = render(<Takes status={loaded} onError={vi.fn()}/>); await screen.findByText('Lead take');
  fireEvent.click(screen.getByRole('button', {name: 'Sections · 1'}));
  fireEvent.change(screen.getByLabelText('Saved section'), {target: {value: 'phrase'}});
  fireEvent.click(screen.getByRole('button', {name: 'Recall section'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('takeReviewSection', 'one', 'processed', 'recall', '', 'phrase'));
  fireEvent.change(screen.getByLabelText('Take version'), {target: {value: 'v1'}});
  fireEvent.click(screen.getByRole('button', {name: 'Sections · 0'}));
  expect(screen.getByLabelText('Saved section').disabled).toBe(true);
  view.rerender(<Takes status={{...loaded, takes: {...loaded.takes, reviewVersion: 'v1'}, review: {...loaded.review, sections: [], sectionRevision: 2}}} onError={vi.fn()}/>);
  fireEvent.click(screen.getByRole('button', {name: 'Sections · 0'}));
  expect(screen.getByLabelText('Saved section').value).toBe(''); expect(screen.getByRole('button', {name: 'Recall section'}).disabled).toBe(true);
});
it('searches saved notes, edits them separately and loads notes from another take', async () => {
  bridge.entries[0].notes = 'Drop D, 140 bpm'; bridge.entries[1].notes = 'Fingerstyle';
  render(<Takes status={status} onError={vi.fn()}/>); await screen.findByText('Lead take');
  expect(screen.getByLabelText('Take notes').value).toBe('Drop D, 140 bpm');
  fireEvent.change(screen.getByLabelText('Search takes'), {target: {value: '140 bpm'}});
  expect(screen.queryByText('★ Favorite clean')).toBeNull();
  fireEvent.change(screen.getByLabelText('Take notes'), {target: {value: 'Drop D\nPractice the solo'}});
  fireEvent.click(screen.getByRole('button', {name: 'Save notes'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('saveTakeNotes', 'one', 'Drop D\nPractice the solo'));
  fireEvent.change(screen.getByLabelText('Search takes'), {target: {value: ''}}); fireEvent.click(screen.getByText('★ Favorite clean'));
  expect(screen.getByLabelText('Take notes').value).toBe('Fingerstyle');
});
it('reports note-save failures without losing the draft on ordinary polls', async () => {
  const onError = vi.fn(), view = render(<Takes status={status} onError={onError}/>); await screen.findByText('Lead take');
  fireEvent.change(screen.getByLabelText('Take notes'), {target: {value: 'Unfinished draft'}});
  view.rerender(<Takes status={{...status, review: {...status.review, position: 20}}} onError={onError}/>);
  expect(screen.getByLabelText('Take notes').value).toBe('Unfinished draft');
  bridge.invoke.mockResolvedValue('Could not save the take catalog.'); fireEvent.click(screen.getByRole('button', {name: 'Save notes'}));
  await waitFor(() => expect(onError).toHaveBeenCalledWith({title: 'Takes', text: 'Could not save the take catalog.'}));
});
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
  await screen.findByText('This recording was interrupted. Use Recover interrupted recording to create a reviewable copy.');
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
it('pauses, resumes, seeks and loops only the loaded selected version', async () => {
  const loaded = {...status, takes: {...status.takes, reviewId:'one', reviewVersion:'processed'}, review:{...status.review, playing:true, a:2, b:8, loop:false}};
  const {rerender} = render(<Takes status={loaded} onError={vi.fn()}/>); await screen.findByText('Lead take');
  expect(screen.getByRole('status', {name:'Take review status'}).textContent).toContain('Playing: Lead take · Original processed');
  fireEvent.click(screen.getByRole('button',{name:'Pause review'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('takeReviewControl','one','processed','pause',0));
  fireEvent.change(screen.getByLabelText('Take review position'),{target:{value:'4'}});
  fireEvent.click(screen.getByRole('button',{name:'Set review A here'}));
  fireEvent.click(screen.getByRole('button',{name:'Set review B here'}));
  fireEvent.click(screen.getByLabelText('Loop take review'));
  fireEvent.click(screen.getByRole('button',{name:'Go to A'}));
  expect(bridge.invoke).toHaveBeenCalledWith('takeReviewControl','one','processed','seek',4);
  expect(bridge.invoke).toHaveBeenCalledWith('takeReviewControl','one','processed','a',12);
  expect(bridge.invoke).toHaveBeenCalledWith('takeReviewControl','one','processed','b',12);
  expect(bridge.invoke).toHaveBeenCalledWith('takeReviewControl','one','processed','loop',1);
  expect(bridge.invoke).toHaveBeenCalledWith('takeReviewControl','one','processed','seek',2);
  rerender(<Takes status={{...loaded,review:{...loaded.review,playing:false,loop:true}}} onError={vi.fn()}/>);
  fireEvent.click(screen.getByRole('button',{name:'Resume review'}));
  expect(bridge.invoke).toHaveBeenCalledWith('takeReviewControl','one','processed','play',0);
  expect(screen.getByLabelText('Loop take review').checked).toBe(true);
});
it('keeps the actual review label while selection changes and blocks controls for other versions', async () => {
  render(<Takes status={{...status,takes:{...status.takes,reviewId:'one',reviewVersion:'processed'},review:{...status.review,playing:true}}} onError={vi.fn()}/>); await screen.findByText('Lead take');
  fireEvent.change(screen.getByLabelText('Take version'),{target:{value:'v1'}});
  expect(screen.getByRole('status', {name:'Take review status'}).textContent).toContain('Playing: Lead take · Original processed');
  expect(screen.getByLabelText('Take review position').disabled).toBe(true);
  expect(screen.getByRole('button',{name:'Pause review'}).disabled).toBe(true);
  expect(screen.getByLabelText('Loop take review').disabled).toBe(true);
  fireEvent.click(screen.getByText('★ Favorite clean'));
  expect(screen.getByRole('button',{name:'Set review A here'}).disabled).toBe(true);
  expect(screen.getByRole('status', {name:'Take review status'}).textContent).toContain('Press Listen to load your selected version');
  fireEvent.click(screen.getByRole('button',{name:'Listen'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('previewTake','two','processed'));
  expect(bridge.invoke.mock.calls.some(([fn])=>fn==='takeReviewControl')).toBe(false);
});
it('blocks pending and recording review controls and prevents too-short loops', async () => {
  const loaded={...status,takes:{...status.takes,reviewId:'one',reviewVersion:'processed'},review:{...status.review,a:2,b:2.01}};
  const {rerender}=render(<Takes status={loaded} onError={vi.fn()}/>); await screen.findByText('Lead take');
  expect(screen.getByLabelText('Loop take review').disabled).toBe(true);
  for (const next of [{...loaded,takes:{...loaded.takes,reviewLoading:true}},{...loaded,review:{...loaded.review,loading:true}},{...loaded,practice:{recordMode:3}}]) {
    rerender(<Takes status={next} onError={vi.fn()}/>);
    expect(screen.getByLabelText('Take review position').disabled).toBe(true);
    expect(screen.getByRole('button',{name:'Resume review'}).disabled).toBe(true);
  }
});
it('surfaces review command failures without changing soundtrack export settings', async () => {
  const error=vi.fn();
  bridge.invoke.mockImplementation(async fn=>fn==='getTakes'?bridge.entries:fn==='takeReviewControl'?'Listen to the selected take version before using its review controls.':'');
  render(<Takes status={{...status,takes:{...status.takes,reviewId:'one',reviewVersion:'processed'},review:{...status.review,a:2,b:8}}} onError={error}/>); await screen.findByText('Lead take');
  fireEvent.change(screen.getByLabelText('Export start seconds'),{target:{value:'3'}});
  fireEvent.click(screen.getByRole('button',{name:'Go to A'}));
  await waitFor(()=>expect(error).toHaveBeenCalledWith({title:'Takes',text:'Listen to the selected take version before using its review controls.'}));
  expect(screen.getByLabelText('Export start seconds').value).toBe('3');
  fireEvent.click(screen.getByRole('button',{name:'Export for video'}));
  expect(bridge.invoke).toHaveBeenCalledWith('exportVideoAudio','one','processed',false,0,0,3,10,.01);
});
it('draws the loaded version waveform and seeks by pointer and keyboard without fetching on position polls', async () => {
  const loaded={...status,takes:{...status.takes,reviewId:'one',reviewVersion:'processed'},review:{...status.review,duration:10,position:2,waveRevision:1}};
  bridge.invoke.mockImplementation(async fn=>fn==='getTakes'?bridge.entries:fn==='getTakeReviewWaveform'?{revision:1,takeId:'one',version:'processed',peaks:[[-.4,.6],[0,.2]]}:'');
  const {container,rerender}=render(<Takes status={loaded} onError={vi.fn()}/>); await screen.findByText('Lead take');
  await waitFor(()=>expect(container.querySelector('.wave-peaks').getAttribute('d')).toContain('M250.00,23.00V68.00'));
  const slider=screen.getByRole('slider',{name:'Take waveform position'});
  vi.spyOn(slider,'getBoundingClientRect').mockReturnValue({left:10,width:400});
  fireEvent.click(slider,{clientX:210});
  expect(bridge.invoke).toHaveBeenCalledWith('takeReviewControl','one','processed','seek',5);
  fireEvent.keyDown(slider,{key:'ArrowRight'});
  expect(bridge.invoke).toHaveBeenCalledWith('takeReviewControl','one','processed','seek',3);
  rerender(<Takes status={{...loaded,review:{...loaded.review,position:4}}} onError={vi.fn()}/>);
  expect(bridge.invoke.mock.calls.filter(([fn])=>fn==='getTakeReviewWaveform')).toHaveLength(1);
  fireEvent.change(screen.getByLabelText('Take version'),{target:{value:'v1'}});
  expect(slider.getAttribute('aria-disabled')).toBe('true');
  expect(container.querySelector('.wave-peaks').getAttribute('d')).toBe('');
  fireEvent.keyDown(slider,{key:'End'});
  expect(bridge.invoke.mock.calls.filter(([fn])=>fn==='takeReviewControl')).toHaveLength(2);
});
it('reports unavailable take waveforms while keeping the ordinary position slider usable',async()=>{
  const error=vi.fn();
  bridge.invoke.mockImplementation(async fn=>fn==='getTakes'?bridge.entries:fn==='getTakeReviewWaveform'?{error:'Version changed'}:'');
  render(<Takes status={{...status,takes:{...status.takes,reviewId:'one',reviewVersion:'processed'},review:{...status.review,waveRevision:1}}} onError={error}/>); await screen.findByText('Lead take');
  await waitFor(()=>expect(error).toHaveBeenCalledWith({title:'Takes',text:'Couldn’t read this version’s waveform. Use the position slider.'}));
  expect(screen.getByLabelText('Take review position').disabled).toBe(false);
  expect(screen.getByText('Waveform unavailable. Use the position slider.')).toBeTruthy();
});
