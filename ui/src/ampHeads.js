import cleanArt from './assets/head-clean.webp';
import crunchArt from './assets/head-crunch.webp';
import metalArt from './assets/head-metal.webp';
import classicalArt from './assets/head-classical.webp';
import { boardId, byId } from './parameters.js';

// These are visual families and complete starting rigs, not additional DSP models.
// The actual engine/capture identity remains visible on every head.
export const ampHeads = [
  { id: 'clean', name: 'Lumen', voice: 'Clean', finish: 'Ivory / nickel', art: cleanArt, rig: 'factory.prism-clean' },
  { id: 'crunch', name: 'Rubicon', voice: 'Crunch', finish: 'Oxblood / brass', art: crunchArt, rig: 'factory.classic-rock' },
  { id: 'metal', name: 'Ferrum', voice: 'Metal', finish: 'Graphite / steel', art: metalArt, rig: 'factory.iron-rhythm' },
  { id: 'classical', name: 'Aurelia', voice: 'Classical', finish: 'Walnut / linen', art: classicalArt, rig: 'factory.natural-nylon' },
];

export function headForTone(source, clean, rig, values = {}, board = {}) {
  // Ignore an old rig's category after switching its amp engine manually.
  const rigSource = rig?.parameters?.AMP_SOURCE ?? (rig?.starter ? 1 : source);
  if (rig && source === rigSource && ['crunch', 'breakup'].includes(rig.gain)) return ampHeads[1];
  if (rig && source === rigSource && rig.gain === 'high-gain') return ampHeads[2];
  const live = type => board.serial && board.blocks?.some(block => block.type === type && values[block.enabledId] >= .5);
  if (live('distortion')) return ampHeads[2];
  if (live('overdrive') && clean) return ampHeads[1];
  if (source === 4) return ampHeads[3];
  if (source === 2 && values.DRIVE_GAIN === 0) return ampHeads[1];
  return ampHeads[clean ? 0 : 2];
}

export function headDriveControl(source, values = {}, board = {}, previewRig) {
  if (source !== 4) return 'DRIVE_GAIN';
  // Natural DI bypasses amp saturation: expose its driving pedal if present,
  // otherwise expose the working amp output trim. Automation IDs stay intact.
  const block = board.serial && board.blocks?.find(row => row.type === 'distortion' && values[row.enabledId] >= .5);
  if (block) {
    const id = boardId(10, Number(block.automationSlot), 'DIST_DRIVE');
    if (byId[id]) return id;
  }
  if (previewRig?.board?.some(row => row.type === 'distortion')) return boardId(10, 0, 'DIST_DRIVE');
  return 'AMP_OUT';
}

export function headSpaceControl(values = {}, board = {}, previewRig) {
  const kinds = {reverb: [7, 'REVERB_MIX'], plate: [11, 'PLATE_MIX'], spring: [12, 'SPRING_MIX'], ambience: [8, 'AMBIENCE_MIX']};
  if (board.serial) {
    const candidates = board.blocks?.filter(row => kinds[row.type] && values[row.enabledId] >= .5) ?? [];
    const block = candidates.find(row => row.lane === 'post') ?? candidates[0];
    if (!block) return 'PRESENCE';
    const [kind, base] = kinds[block.type], id = boardId(kind, Number(block.automationSlot), base);
    return byId[id] ? id : 'PRESENCE';
  }
  if (previewRig?.board) {
    const block = previewRig.board.find(row => kinds[row.type] && row.lane === 'post') ?? previewRig.board.find(row => kinds[row.type]);
    if (!block) return 'PRESENCE';
    const [kind, base] = kinds[block.type];
    return boardId(kind, 0, base);
  }
  return 'REVERB_MIX';
}
