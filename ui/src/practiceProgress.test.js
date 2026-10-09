import { expect, it } from 'vitest';
import { localDay, practiceProgress } from './practiceProgress.js';
const now = new Date(2026, 9, 8, 12);
const session = (day, extra = {}) => ({ started: new Date(2026, 9, day, 20).toISOString(), state: 'finished', title: 'Clean touch', setName: 'Daily', bpm: 80, seconds: 300, minutes: 5, ...extra });
it('separates this and previous seven-day windows and excludes unfinished, invalid and future work', () => {
  const result = practiceProgress([session(8), session(2), session(1), session(-5), session(-6), session(-19), session(-20), session(9), session(8, { state: 'interrupted' }), session(8, { state: 'paused' }), session(8, { seconds: NaN }), session(8, { started: 'invalid' })], now);
  expect(result.days).toHaveLength(28); expect(result.days[0].date).toBe('2026-09-11'); expect(result.days.at(-1).date).toBe('2026-10-08');
  expect(result.weekSeconds).toBe(600); expect(result.previousWeekSeconds).toBe(600); expect(result.seconds).toBe(1800);
  expect(result.activeDays).toBe(6); expect(result.sessions).toBe(6); expect(result.targetsReached).toBe(6);
});
it('keeps separate exercise/set identities, sums multiple same-day sessions and labels planned tempo ranges', () => {
  const result = practiceProgress([session(8), session(8, { bpm: 140, seconds: 60 }), session(8, { setName: 'Other', bpm: 100 }), session(7, { title: 'Lead', seconds: 600 })], now);
  expect(result.days.at(-1)).toMatchObject({ seconds: 660, sessions: 3, targetsReached: 2 });
  expect(result.exercises).toEqual([{ title: 'Lead', setName: 'Daily', seconds: 600, sessions: 1, minBpm: 80, maxBpm: 80 }, { title: 'Clean touch', setName: 'Daily', seconds: 360, sessions: 2, minBpm: 80, maxBpm: 140 }, { title: 'Clean touch', setName: 'Other', seconds: 300, sessions: 1, minBpm: 100, maxBpm: 100 }]);
});
it('uses calendar days through a DST boundary and never parses local day strings as UTC', () => {
  const date = new Date(2026, 10, 2, 0, 30), result = practiceProgress([], date);
  expect(result.days.at(-1).date).toBe('2026-11-02'); expect(result.days.at(-2).date).toBe('2026-11-01'); expect(new Set(result.days.map(day => day.date)).size).toBe(28);
  expect(localDay(new Date(2026, 0, 1, 0, 5))).toBe('2026-01-01'); expect(result.weekSeconds).toBe(0);
});
