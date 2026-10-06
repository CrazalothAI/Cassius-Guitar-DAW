export const gainLabels = {'': 'Unknown', clean: 'Clean', breakup: 'Breakup', crunch: 'Crunch', 'high-gain': 'High gain', drive: 'Drive', fuzz: 'Fuzz'};
export const styleNames = ['acoustic', 'ambient', 'blues', 'classical', 'clean', 'country', 'funk', 'jazz', 'lead', 'metal', 'metalcore', 'pop', 'rock', 'thrash'];
export const title = value => value ? value[0].toUpperCase() + value.slice(1) : '';
const tokens = value => String(value || '').toLowerCase().split(/[\s,;]+/).filter(Boolean);
// Literal filename/tag hints help old libraries without rewriting owner metadata.
// Explicit fields always win, including an explicit Unknown gain.
export function catalogRow(row) {
  const hint = `${row.name || ''} ${row.sourceName || ''} ${row.tags || ''}`.toLowerCase();
  const styles = row.styles != null ? (Array.isArray(row.styles) ? row.styles : tokens(row.styles)) : styleNames.filter(style => tokens(hint).includes(style));
  const inferredGain = /\b(fuzz|muff)\b/.test(hint) ? 'fuzz' : /\b(clean|jazz)\b/.test(hint) ? 'clean'
    : /\b(breakup)\b/.test(hint) ? 'breakup' : /\b(crunch)\b/.test(hint) ? 'crunch'
    : /\b(high[- ]gain|metalcore|thrash)\b/.test(hint) ? 'high-gain'
    : row.kind === 'pedal' && /\b(drive|overdrive|klon|timmy|screamer|sd[- ]?1|bd[- ]?2)\b/.test(hint) ? 'drive' : '';
  const candidateGain = row.gain != null ? row.gain : inferredGain;
  const gain = Object.hasOwn(gainLabels, candidateGain) ? candidateGain : '';
  const speaker = row.speaker != null ? row.speaker : /\bv\s?30\b/i.test(hint) ? 'V30' : /\bjensen\b/i.test(hint) ? 'Jensen' : /\balnico blue\b/i.test(hint) ? 'Alnico Blue' : '';
  return {...row, styles, gain, speaker, inferred: (row.gain == null && !!gain) || (row.styles == null && styles.length > 0) || (row.speaker == null && !!speaker)};
}
export function matchesCatalog(row, filters, favorite) {
  const haystack = [row.name, row.sourceName, row.gear, row.creator, row.tags, row.notes, row.pack, row.speaker, ...row.styles].join(' ').toLowerCase();
  return (filters.ownership === 'All' || row.ownership === filters.ownership)
    && (!filters.favorites || favorite) && haystack.includes(filters.search.toLowerCase())
    && (!filters.style || row.styles.includes(filters.style))
    && (filters.gain === 'all' || row.gain === filters.gain)
    && (!filters.speaker || row.speaker === filters.speaker) && (!filters.pack || row.pack === filters.pack)
    && (!filters.rigType || (filters.rigType === 'starter' ? !!row.starter : filters.rigType === 'controls' ? !!row.preset : !row.starter && !row.preset));
}
