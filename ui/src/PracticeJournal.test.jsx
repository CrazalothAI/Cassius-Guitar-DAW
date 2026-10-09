import { beforeEach, afterEach, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, waitFor, within } from '@testing-library/react';
const bridge = vi.hoisted(() => ({ invoke: vi.fn(), setParameter: vi.fn() }));
vi.mock('./juce/bridge.js', () => ({ invoke: bridge.invoke }));
vi.mock('./parameterState.js', () => ({ setParameter: bridge.setParameter }));
import PracticeJournal from './components/PracticeJournal.jsx';
const plan = { id: 'plan', name: 'Clean and lead', tasks: [{ title: 'Clean touch', minutes: 5, bpm: 80 }, { title: 'Lead picking', minutes: 10, bpm: 140 }] };
const session = { id: 'old', title: 'Clean touch', setName: 'Clean and lead', minutes: 5, bpm: 80, seconds: 300, state: 'finished', notes: 'Try softer picks', started: '2026-10-08T12:00:00Z' };
const doc = { sets: [plan], sessions: [session] }, summary = { available: true, writable: true, busy: false, revision: 1, setCount: 1, sessionCount: 1 };
const props = { journal: summary, recording: false, available: true, onError: vi.fn() };
beforeEach(() => { bridge.invoke.mockReset().mockImplementation(method => Promise.resolve(method === 'getPracticeJournal' ? doc : '')); bridge.setParameter.mockReset(); props.onError.mockReset(); });
afterEach(cleanup);
async function open(extra = {}) { const view = render(<PracticeJournal {...props} {...extra}/>); fireEvent.click(screen.getByRole('button', { name: /Practice sets & history/ })); await screen.findByRole('option', { name: plan.name }); return view; }
it('starts a selected exercise without silently changing tone, transport or tempo', async () => {
  await open(); fireEvent.change(screen.getByLabelText('Practice set'), { target: { value: 'plan' } });
  fireEvent.change(screen.getByLabelText('Exercise to practice'), { target: { value: '1' } });
  fireEvent.click(screen.getByRole('button', { name: 'Start practice timer' }));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('practiceJournalCommand', 'start', { title: 'Lead picking', minutes: 10, bpm: 140, setName: 'Clean and lead' }));
  expect(bridge.setParameter).not.toHaveBeenCalled();
  fireEvent.click(screen.getByRole('button', { name: 'Use exercise tempo' })); expect(bridge.setParameter).toHaveBeenCalledWith('METRO_BPM', 140);
});
it('edits bounded independent exercises and preserves selected plan identity', async () => {
  await open(); fireEvent.change(screen.getByLabelText('Practice set'), { target: { value: 'plan' } });
  fireEvent.change(screen.getByLabelText('BPM target 2'), { target: { value: '150' } });
  fireEvent.click(screen.getByRole('button', { name: 'Save set changes' }));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('practiceJournalCommand', 'saveSet', { ...plan, tasks: [plan.tasks[0], { ...plan.tasks[1], bpm: 150 }] }));
  fireEvent.change(screen.getByLabelText('Minutes 1'), { target: { value: '0' } }); expect(screen.getByRole('button', { name: 'Save set changes' }).disabled).toBe(true);
});
it('selects the newly saved set after persistence so another Save edits it', async () => {
  const view = await open();
  fireEvent.click(screen.getByRole('button', { name: 'Save practice set' }));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('practiceJournalCommand', 'saveSet', expect.objectContaining({ id: '' })));
  const added = { id: 'new', name: 'My practice set', tasks: [{ title: 'Clean dynamics', minutes: 10, bpm: 80 }] };
  bridge.invoke.mockImplementation(method => Promise.resolve(method === 'getPracticeJournal' ? { sets: [...doc.sets, added], sessions: doc.sessions } : ''));
  view.rerender(<PracticeJournal {...props} journal={{ ...summary, revision: 2, setCount: 2 }}/>);
  await waitFor(() => expect(screen.getByLabelText('Practice set').value).toBe('new'));
  fireEvent.click(screen.getByRole('button', { name: 'Save set changes' }));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('practiceJournalCommand', 'saveSet', added));
});
it('shows the native timer and saves notes only on explicit finish', async () => {
  await open({ journal: { ...summary, active: { ...session, id: 'active', state: 'running', seconds: 302 } } });
  expect(screen.getByText(/5:02 \/ 5 min/)).toBeTruthy();
  fireEvent.change(screen.getByLabelText('Session notes'), { target: { value: 'Cleaner picking' } });
  expect(bridge.invoke).not.toHaveBeenCalledWith('practiceJournalCommand', 'finish', expect.anything());
  fireEvent.click(screen.getByRole('button', { name: 'Pause timer' })); await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('practiceJournalCommand', 'pause', null));
  fireEvent.click(screen.getByRole('button', { name: 'Finish session' })); await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('practiceJournalCommand', 'finish', 'Cleaner picking'));
  expect(screen.getByRole('button', { name: 'Start practice timer' }).disabled).toBe(true);
});
it('requires confirmation for history removal and searches notes', async () => {
  await open(); fireEvent.change(screen.getByLabelText('Find a session'), { target: { value: 'softer' } }); expect(screen.getByText('Try softer picks')).toBeTruthy();
  fireEvent.click(screen.getByRole('button', { name: 'Remove session' })); expect(bridge.invoke).not.toHaveBeenCalledWith('practiceJournalCommand', 'removeSession', 'old');
  fireEvent.click(screen.getByRole('button', { name: 'Confirm remove session' })); await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('practiceJournalCommand', 'removeSession', 'old'));
});
it('uses explicit portable transfers and disables import during a session', async () => {
  const view = await open(); fireEvent.click(screen.getByRole('button', { name: 'Export sets & history' })); await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('transferPracticeJournal', true));
  fireEvent.click(screen.getByRole('button', { name: 'Import sets & history' })); await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('transferPracticeJournal', false));
  view.rerender(<PracticeJournal {...props} journal={{ ...summary, active: { ...session, state: 'paused' } }}/ >);
  expect(screen.getByRole('button', { name: 'Import sets & history' }).disabled).toBe(true); expect(screen.getByRole('button', { name: 'Resume timer' })).toBeTruthy();
});
it('keeps tempo and transfers locked during recording while timer controls remain available', async () => {
  await open({ recording: true }); expect(screen.getByRole('button', { name: 'Use exercise tempo' }).disabled).toBe(true); expect(screen.getByRole('button', { name: 'Export sets & history' }).disabled).toBe(true);
  expect(screen.getByRole('button', { name: 'Start practice timer' }).disabled).toBe(false);
});
it('surfaces journal errors without resetting existing history', async () => {
  await open({ journal: { ...summary, error: 'Storage unavailable' } }); expect(screen.getByRole('alert').textContent).toBe('Storage unavailable');
  bridge.invoke.mockResolvedValue('Saving, try again'); fireEvent.click(screen.getByRole('button', { name: 'Start practice timer' }));
  await waitFor(() => expect(props.onError).toHaveBeenCalledWith({ title: 'Practice history', text: 'Saving, try again' })); expect(screen.getByText('Try softer picks')).toBeTruthy();
});
it('fetches lists only when the native revision changes and hides the journal in plugins', async () => {
  const view = await open(); const count = bridge.invoke.mock.calls.filter(([name]) => name === 'getPracticeJournal').length;
  view.rerender(<PracticeJournal {...props} journal={{ ...summary, active: { ...session, seconds: 2 } }}/ >);
  expect(bridge.invoke.mock.calls.filter(([name]) => name === 'getPracticeJournal').length).toBe(count);
  view.rerender(<PracticeJournal {...props} journal={null}/>); expect(screen.queryByLabelText('Practice sets and history')).toBeNull();
});
it('links a chosen version, opens it explicitly and unlinks without touching recordings', async () => {
  const link = { takeId: 'take', version: 'v1' }, onOpenTake = vi.fn();
  const entries = [{ id: 'take', name: 'Clean recording', versions: [{ id: 'v1', name: 'Lead reamp' }] }];
  bridge.invoke.mockImplementation(method => Promise.resolve(method === 'getTakes' ? entries : method === 'getPracticeJournal' ? { ...doc, sessions: [{ ...session, recordings: [link] }] } : ''));
  await open({ onOpenTake });
  fireEvent.click(screen.getByText('Recordings · 1 / 8'));
  await screen.findByRole('option', { name: 'Clean recording' });
  fireEvent.click(screen.getByRole('button', { name: 'Open in Takes' })); expect(onOpenTake).toHaveBeenCalledWith(link);
  expect(bridge.invoke).not.toHaveBeenCalledWith('previewTake', expect.anything(), expect.anything());
  fireEvent.change(screen.getByLabelText('Recording for Clean touch (old)'), { target: { value: 'take' } });
  fireEvent.change(screen.getByLabelText('Recording version for Clean touch (old)'), { target: { value: 'v1' } });
  expect(screen.getByRole('button', { name: 'Link recording' }).disabled).toBe(true);
  fireEvent.change(screen.getByLabelText('Recording version for Clean touch (old)'), { target: { value: 'dry' } });
  fireEvent.click(screen.getByRole('button', { name: 'Link recording' }));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('practiceJournalCommand', 'addRecording', { sessionId: 'old', takeId: 'take', version: 'dry' }));
  fireEvent.click(screen.getByRole('button', { name: 'Unlink recording' }));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('practiceJournalCommand', 'removeRecording', { sessionId: 'old', ...link }));
});
it('preserves portable unavailable links and disables opening/linking during recording', async () => {
  const link = { takeId: 'other-pc', version: 'dry' };
  bridge.invoke.mockImplementation(method => Promise.resolve(method === 'getPracticeJournal' ? { ...doc, sessions: [{ ...session, recordings: [link] }] } : method === 'getTakes' ? [] : ''));
  const view = await open({ onOpenTake: vi.fn() }); fireEvent.click(screen.getByText('Recordings · 1 / 8'));
  expect(screen.getByText(/Unavailable recording · other-pc/)).toBeTruthy(); expect(screen.getByRole('button', { name: 'Open in Takes' }).disabled).toBe(true);
  view.rerender(<PracticeJournal {...props} recording onOpenTake={vi.fn()}/>);
  expect(screen.getByLabelText('Recording for Clean touch (old)').disabled).toBe(true);
  expect(screen.getByRole('button', { name: 'Unlink recording' }).disabled).toBe(false);
});
it('shows progress separately from the history search and refreshes takes only on catalog changes', async () => {
  const view = await open({ takeRevision: 1 });
  const progress = within(screen.getByRole('region', { name: 'Practice progress' }));
  expect(progress.getByRole('list', { name: 'Daily finished practice time' }).children).toHaveLength(28);
  expect(progress.getByText(/do not measure notes played/)).toBeTruthy();
  fireEvent.change(screen.getByLabelText('Find a session'), { target: { value: 'missing' } });
  expect(screen.getByRole('region', { name: 'Practice progress' })).toBeTruthy();
  const count = bridge.invoke.mock.calls.filter(([name]) => name === 'getTakes').length;
  view.rerender(<PracticeJournal {...props} takeRevision={1} journal={{ ...summary, active: { ...session, seconds: 2 } }}/>);
  expect(bridge.invoke.mock.calls.filter(([name]) => name === 'getTakes').length).toBe(count);
  view.rerender(<PracticeJournal {...props} takeRevision={2}/>);
  await waitFor(() => expect(bridge.invoke.mock.calls.filter(([name]) => name === 'getTakes').length).toBe(count + 1));
});
