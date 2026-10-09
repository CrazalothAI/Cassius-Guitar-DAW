export default function PedalArtwork({type}) {
  const drive = ['overdrive', 'neural-pedal', 'distortion'].includes(type);
  const space = ['delay', 'reverb', 'plate', 'spring', 'ambience'].includes(type);
  return <svg className="pedal-artwork" aria-hidden="true" focusable="false" viewBox="0 0 120 40" fill="none" stroke="currentColor" strokeWidth="1">
    <path d="M1 1h118v38H1z" opacity=".3"/>
    {drive ? <><path d="m6 30 18-20 18 20 18-20 18 20 18-20 18 20M6 35l18-20 18 20 18-20 18 20 18-20 18 20"/><path d="M6 5h108" opacity=".5"/></>
      : space ? <>{[9, 14, 19].map(r => <ellipse key={r} cx="60" cy="20" rx={r*2.2} ry={r}/>)}</>
      : <>{[18, 32, 46, 60, 74, 88, 102].map((x, i) => <g key={x}><path d={`M${x} 7v26`}/><rect x={x-3} y={11+i%3*5} width="6" height="5" fill="currentColor"/></g>)}</>}
  </svg>;
}
