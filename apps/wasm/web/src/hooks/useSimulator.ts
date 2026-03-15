import { useState, useEffect, useRef, useCallback } from 'react';
import type { SimulatorInstance, PortState } from '../types/avrion_wasm';

export type SimStatus = 'loading' | 'idle' | 'ready' | 'running' | 'paused' | 'error';

const PORT_NAMES = ['PORTB', 'PORTC', 'PORTD'] as const;

// ATmega328P clock frequency
const CLOCK_HZ = 16_000_000;
// Max wall-clock ms to catch up per frame — prevents a "frozen tab" burst
const MAX_CATCHUP_MS = 50;

export interface SimulatorState {
  status: SimStatus;
  errorMsg: string;
  ports: PortState[];
  totalCycles: number;
  hexLoaded: boolean;
}

export interface SimulatorControls {
  loadHex: (content: string) => void;
  reset: () => void;
  start: () => void;
  pause: () => void;
  step: (n?: number) => void;
  setPinInput: (port: typeof PORT_NAMES[number], pin: number, high: boolean) => void;
}

export function useSimulator(): SimulatorState & SimulatorControls {
  const simRef = useRef<SimulatorInstance | null>(null);
  const rafRef = useRef<number | null>(null);
  const lastFrameTimeRef = useRef<number | null>(null);

  const [status, setStatus] = useState<SimStatus>('loading');
  const [errorMsg, setErrorMsg] = useState('');
  const [ports, setPorts] = useState<PortState[]>([]);
  const [totalCycles, setTotalCycles] = useState(0);
  const [hexLoaded, setHexLoaded] = useState(false);

  // Snapshot all port states from the C++ instance
  const refreshPorts = useCallback(() => {
    const sim = simRef.current;
    if (!sim) return;
    setPorts(PORT_NAMES.map(name => sim.get_port_state(name)));
    setTotalCycles(sim.total_cycles());
  }, []);

  // Load the Emscripten module once on mount
  useEffect(() => {
    let instance: SimulatorInstance | null = null;

    (async () => {
      try {
        // avrion_wasm.js is a UMD script served from /public (not an ES module).
        // We load it by injecting a <script> tag so Vite's import-analysis is
        // never involved, then access the factory via window.createAvrionModule.
        await new Promise<void>((resolve, reject) => {
          if ((window as unknown as Record<string, unknown>)['createAvrionModule']) {
            resolve(); // already loaded from a previous HMR cycle
            return;
          }
          const script = document.createElement('script');
          script.src = '/avrion_wasm.js';
          script.onload = () => resolve();
          script.onerror = () => reject(new Error('Failed to load /avrion_wasm.js — run the Emscripten build first.'));
          document.head.appendChild(script);
        });

        type FactoryFn = (opts?: { locateFile?: (f: string) => string }) => Promise<{ Simulator: new () => SimulatorInstance }>;
        const factory = (window as unknown as Record<string, FactoryFn>)['createAvrionModule'];
        const mod = await factory({ locateFile: (f: string) => '/' + f });
        instance = new mod.Simulator();
        simRef.current = instance;
        refreshPorts();
        setStatus('idle');
      } catch (e) {
        setErrorMsg(String(e));
        setStatus('error');
      }
    })();

    return () => {
      if (rafRef.current) cancelAnimationFrame(rafRef.current);
      instance?.delete?.();
    };
  }, [refreshPorts]);

  // RAF loop for continuous mode
  const runLoop = useCallback((timestamp: number) => {
    const sim = simRef.current;
    if (!sim) return;

    // Compute elapsed wall-clock time since last frame
    const prev = lastFrameTimeRef.current ?? timestamp;
    lastFrameTimeRef.current = timestamp;
    const elapsedMs = Math.min(timestamp - prev, MAX_CATCHUP_MS);

    // Run exactly as many cycles as a 16 MHz clock ticks in that time
    const cycles = Math.round(elapsedMs * (CLOCK_HZ / 1000));
    if (cycles > 0) sim.run_cycles(cycles);

    refreshPorts();
    rafRef.current = requestAnimationFrame(runLoop);
  }, [refreshPorts]);

  const loadHex = useCallback((content: string) => {
    const sim = simRef.current;
    if (!sim) return;
    try {
      sim.load_hex(content);
      sim.reset();
      setHexLoaded(true);
      setStatus('ready');
      refreshPorts();
    } catch (e) {
      setErrorMsg(String(e));
      setStatus('error');
    }
  }, [refreshPorts]);

  const reset = useCallback(() => {
    const sim = simRef.current;
    if (!sim) return;
    if (rafRef.current) { cancelAnimationFrame(rafRef.current); rafRef.current = null; }
    sim.reset();
    setStatus(hexLoaded ? 'ready' : 'idle');
    refreshPorts();
  }, [hexLoaded, refreshPorts]);

  const start = useCallback(() => {
    if (!simRef.current) return;
    if (rafRef.current) return; // already running
    lastFrameTimeRef.current = null; // reset so first frame doesn't over-run
    setStatus('running');
    rafRef.current = requestAnimationFrame(runLoop);
  }, [runLoop]);

  const pause = useCallback(() => {
    if (rafRef.current) { cancelAnimationFrame(rafRef.current); rafRef.current = null; }
    setStatus('paused');
    refreshPorts();
  }, [refreshPorts]);

  const step = useCallback((n = 1000) => {
    const sim = simRef.current;
    if (!sim) return;
    sim.run_cycles(n);
    refreshPorts();
  }, [refreshPorts]);

  const setPinInput = useCallback((
    port: typeof PORT_NAMES[number],
    pin: number,
    high: boolean,
  ) => {
    const sim = simRef.current;
    if (!sim) return;
    sim.set_pin_input(port, pin, high);
    refreshPorts();
  }, [refreshPorts]);

  return {
    status, errorMsg, ports, totalCycles, hexLoaded,
    loadHex, reset, start, pause, step, setPinInput,
  };
}
