import { useEffect, useRef } from 'react';
export default function UtilityDialog({title, onClose, notice, children}) {
  const panel = useRef(null);
  useEffect(() => { const previous = document.activeElement; panel.current?.querySelector('button')?.focus(); return () => previous?.focus(); }, []);
  return <div className="utility-overlay" onKeyDown={e => {
    if (e.key === 'Escape') { e.stopPropagation(); onClose(); }
    if (e.key === 'Tab') {
      const controls = [...panel.current.querySelectorAll('button,input,select,textarea,[tabindex="0"]')].filter(x => !x.disabled);
      const first = controls[0], last = controls.at(-1);
      if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last?.focus(); }
      else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first?.focus(); }
    }
  }}><section ref={panel} className="utility-dialog" role="dialog" aria-modal="true" aria-label={title}><div className="utility-heading"><h2>{title}</h2><button className="text-button" onClick={onClose}>Close {title.toLowerCase()}</button></div>{notice && <p className="practice-error" role="alert">{notice.title}: {notice.text}</p>}{children}</section></div>;
}
