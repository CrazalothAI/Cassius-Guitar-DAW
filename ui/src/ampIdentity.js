export function ampIdentity(source, clean, status) {
  if (source === 4) return 'Natural DI';
  if (source === 1 || (source === 0 && clean)) return 'Lumen · built-in clean';
  if (source === 3 || (source === 0 && status.model)) return status.model || 'NAM · no capture loaded';
  return 'Ferrum · built-in high gain';
}
