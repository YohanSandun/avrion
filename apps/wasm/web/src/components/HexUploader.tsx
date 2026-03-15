import React, { useRef } from 'react';

interface Props {
  onLoad: (content: string) => void;
  disabled?: boolean;
}

const s: Record<string, React.CSSProperties> = {
  zone: {
    border: '2px dashed #30363d',
    borderRadius: 8,
    padding: '24px 32px',
    textAlign: 'center',
    cursor: 'pointer',
    transition: 'border-color 0.2s, background 0.2s',
    background: '#161b22',
  },
  input: { display: 'none' },
  hint: { fontSize: 13, color: '#8b949e', marginTop: 6 },
};

export function HexUploader({ onLoad, disabled }: Props) {
  const inputRef = useRef<HTMLInputElement>(null);

  const readFile = (file: File) => {
    if (!file.name.endsWith('.hex')) {
      alert('Please select an Intel HEX (.hex) file.');
      return;
    }
    const reader = new FileReader();
    reader.onload = e => {
      const content = e.target?.result as string;
      if (content) onLoad(content);
    };
    reader.readAsText(file);
  };

  const onInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (file) readFile(file);
    e.target.value = '';
  };

  const onDrop = (e: React.DragEvent<HTMLDivElement>) => {
    e.preventDefault();
    const file = e.dataTransfer.files?.[0];
    if (file) readFile(file);
  };

  const onDragOver = (e: React.DragEvent<HTMLDivElement>) => e.preventDefault();

  return (
    <div
      style={{ ...s.zone, opacity: disabled ? 0.5 : 1, pointerEvents: disabled ? 'none' : 'auto' }}
      onClick={() => inputRef.current?.click()}
      onDrop={onDrop}
      onDragOver={onDragOver}
    >
      <input
        ref={inputRef}
        type="file"
        accept=".hex"
        style={s.input}
        onChange={onInputChange}
      />
      <div style={{ fontSize: 15, color: '#c9d1d9' }}>
        Drop a <strong>.hex</strong> file here, or click to browse
      </div>
      <div style={s.hint}>
        Compiled AVR Intel HEX — e.g. from avr-gcc / Arduino IDE
      </div>
    </div>
  );
}
