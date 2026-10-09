// Targets describe intent, never a measured playing score. Group finished work
// by local start date; calendar arithmetic keeps DST days intact.
export const localDay = value => {
  const d = new Date(value);
  return Number.isNaN(d.getTime()) ? '' : `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
};
export function practiceProgress(sessions, now = new Date()) {
  const days = Array.from({ length: 28 }, (_, i) => {
    const d = new Date(now.getFullYear(), now.getMonth(), now.getDate() - 27 + i, 12);
    return { date: localDay(d), seconds: 0, sessions: 0, targetsReached: 0 };
  });
  const byDay = new Map(days.map(day => [day.date, day])), exercises = new Map();
  for (const row of sessions) {
    if (row.state !== 'finished' || !Number.isFinite(row.seconds) || row.seconds < 0) continue;
    const day = byDay.get(localDay(row.started)); if (!day) continue;
    day.seconds += row.seconds; day.sessions++;
    if (row.seconds >= row.minutes * 60) day.targetsReached++;
    const key = JSON.stringify([row.setName, row.title]);
    const exercise = exercises.get(key) || { title: row.title, setName: row.setName, seconds: 0, sessions: 0, minBpm: row.bpm, maxBpm: row.bpm };
    exercise.seconds += row.seconds; exercise.sessions++;
    exercise.minBpm = Math.min(exercise.minBpm, row.bpm); exercise.maxBpm = Math.max(exercise.maxBpm, row.bpm); exercises.set(key, exercise);
  }
  const total = rows => rows.reduce((sum, row) => sum + row.seconds, 0);
  return { days, seconds: total(days), weekSeconds: total(days.slice(-7)), previousWeekSeconds: total(days.slice(-14, -7)),
    sessions: days.reduce((sum, day) => sum + day.sessions, 0), activeDays: days.filter(day => day.sessions > 0).length,
    targetsReached: days.reduce((sum, day) => sum + day.targetsReached, 0),
    exercises: [...exercises.values()].sort((a, b) => b.seconds - a.seconds || a.title.localeCompare(b.title) || a.setName.localeCompare(b.setName)) };
}
