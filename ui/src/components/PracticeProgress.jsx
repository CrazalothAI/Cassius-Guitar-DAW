import { useMemo } from 'react';
import { practiceProgress, localDay } from '../practiceProgress.js';
import { time } from '../practiceTime.js';

export default function PracticeProgress({ sessions }) {
  const today = localDay(new Date());
  const progress = useMemo(() => practiceProgress(sessions), [sessions, today]);
  const peak = Math.max(60, ...progress.days.map(day => day.seconds));
  return <section className="journal-progress" aria-label="Practice progress">
    <h3>Your last 28 days</h3>
    <div className="journal-stats"><p><strong>{time(progress.weekSeconds)}</strong><span>Last 7 days</span></p><p><strong>{time(progress.previousWeekSeconds)}</strong><span>Previous 7 days</span></p><p><strong>{progress.activeDays} / 28</strong><span>Days with finished sessions</span></p><p><strong>{progress.targetsReached} / {progress.sessions}</strong><span>Session time targets reached</span></p></div>
    <ol className="journal-calendar" aria-label="Daily finished practice time">{progress.days.map(day => <li key={day.date} title={`${day.date}: ${time(day.seconds)} · ${day.sessions} finished sessions`} aria-label={`${day.date}: ${time(day.seconds)} · ${day.sessions} finished sessions`}><span style={{ height: `${day.seconds ? Math.max(3, day.seconds / peak * 100) : 0}%` }}/></li>)}</ol>
    <div className="journal-calendar-dates"><span>{progress.days[0].date}</span><span>{today}</span></div>
    <p className="practice-note">Finished sessions only, grouped by their start date in your current time zone. Time and BPM targets describe your plan; they do not measure notes played, accuracy or achieved speed.</p>
    {progress.exercises.length > 0 && <details><summary>Exercise totals · last 28 days</summary><ul className="journal-exercise-totals">{progress.exercises.map(row => <li key={JSON.stringify([row.setName, row.title])}><strong>{row.title}</strong><span>{row.setName} · {time(row.seconds)} · {row.sessions} sessions · {row.minBpm === row.maxBpm ? row.minBpm : `${row.minBpm}–${row.maxBpm}`} BPM targets</span></li>)}</ul></details>}
  </section>;
}
