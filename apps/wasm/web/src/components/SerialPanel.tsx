import React, { useRef, useEffect, useState } from 'react';

interface Props {
  output: string;
  onSend: (text: string) => void;
  onClear: () => void;
  /** Disables the send input when the simulator is not running */
  disabled?: boolean;
}

export function SerialPanel({ output, onSend, onClear, disabled }: Props) {
  const [input, setInput] = useState('');
  const outputRef = useRef<HTMLTextAreaElement>(null);

  // Auto-scroll to the bottom whenever new output arrives
  useEffect(() => {
    const el = outputRef.current;
    if (el) el.scrollTop = el.scrollHeight;
  }, [output]);

  const handleSend = () => {
    const text = input.trim();
    if (!text || disabled) return;
    onSend(text + '\n');
    setInput('');
  };

  return (
    <div>
      {/* Terminal output */}
      <textarea
        ref={outputRef}
        readOnly
        value={output}
        placeholder="Serial output will appear here once the simulator is running…"
        spellCheck={false}
        style={{
          display: 'block',
          width: '100%',
          height: 220,
          background: '#010409',
          color: '#3fb950',
          border: '1px solid #30363d',
          borderRadius: 6,
          padding: '10px 12px',
          fontFamily: "'Consolas', 'Fira Mono', monospace",
          fontSize: 13,
          lineHeight: 1.55,
          resize: 'vertical',
          outline: 'none',
        }}
      />

      {/* Input row */}
      <div style={{ display: 'flex', gap: 8, marginTop: 8 }}>
        <input
          type="text"
          value={input}
          onChange={e => setInput(e.target.value)}
          onKeyDown={e => e.key === 'Enter' && handleSend()}
          placeholder={disabled ? 'Start the simulator to send…' : 'Type a message and press Enter…'}
          disabled={disabled}
          style={{
            flex: 1,
            background: '#0d1117',
            color: '#e6edf3',
            border: '1px solid #30363d',
            borderRadius: 6,
            padding: '7px 12px',
            fontFamily: "'Consolas', 'Fira Mono', monospace",
            fontSize: 13,
            outline: 'none',
            opacity: disabled ? 0.5 : 1,
          }}
        />
        <button
          onClick={handleSend}
          disabled={disabled || !input.trim()}
          style={btnStyle(!disabled && !!input.trim(), '#238636', '#2ea043')}
        >
          Send
        </button>
        <button
          onClick={onClear}
          style={btnStyle(true, '#21262d', '#30363d')}
        >
          Clear
        </button>
      </div>

      {/* Legend */}
      <p style={{ fontSize: 11, color: '#8b949e', marginTop: 8 }}>
        Sends the text + <code style={{ fontSize: 11 }}>{'\\n'}</code> to USART0 RX.
        Each byte is injected one-by-one as the firmware reads them.
      </p>
    </div>
  );
}

function btnStyle(active: boolean, bg: string, hoverBg: string): React.CSSProperties {
  void hoverBg; // used in real CSS; kept for symmetry
  return {
    background: active ? bg : '#21262d',
    color: active ? '#e6edf3' : '#8b949e',
    border: '1px solid #30363d',
    borderRadius: 6,
    padding: '7px 14px',
    fontSize: 13,
    cursor: active ? 'pointer' : 'default',
    opacity: active ? 1 : 0.5,
    whiteSpace: 'nowrap',
  };
}
