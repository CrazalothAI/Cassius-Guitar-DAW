import packageInfo from '../package.json';
import releaseInfo from './release.json';
export function releaseLabel(status, native) {
  const info=native ? {channel:status.releaseChannel,candidate:status.releaseCandidate} : releaseInfo;
  return `${status.appVersion || packageInfo.version}${info.channel === 'candidate' ? ` RC${info.candidate}` : info.channel === 'preview' ? ' Preview' : ''}`;
}
