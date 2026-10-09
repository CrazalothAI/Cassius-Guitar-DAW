// Original outline graphics. Labels remain real text; decoration is hidden from AT.
const paths = {
  Tone: <><rect x="3" y="5" width="18" height="14" rx="3"/><path d="M7 9h10M7 15h.01M12 15h.01M17 15h.01M9 3h6"/></>,
  Board: <><rect x="3" y="5" width="7" height="14" rx="2"/><rect x="14" y="5" width="7" height="14" rx="2"/><path d="M6.5 9h.01M17.5 9h.01M6 15h1M17 15h1M10 12h4"/></>,
  Practice: <><path d="m9 5 11 7-11 7zM4 5v14"/></>,
  Takes: <><path d="M3 10v4M7 6v12M11 9v6M15 3v18M19 7v10M23 10v4"/></>,
};
export default function WorkspaceIcon({name}) {
  return <svg className="workspace-icon" aria-hidden="true" focusable="false" viewBox="0 0 26 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">{paths[name]}</svg>;
}
