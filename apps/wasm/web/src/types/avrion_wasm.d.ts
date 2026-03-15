// TypeScript declarations for the Emscripten-generated Simulator module.
// The built avrion_wasm.js is placed in /public and loaded dynamically.

export interface PinState {
  index: number;
  is_output: boolean;
  level: boolean;
  pullup: boolean;
}

export interface PortState {
  port: string;
  pins: PinState[];
}

export interface SimulatorInstance {
  /** Load a .hex file given its full text content (result of FileReader.readAsText) */
  load_hex(hexContent: string): void;
  /** Reset the CPU to power-on state */
  reset(): void;
  /** Execute at most n CPU cycles */
  run_cycles(n: number): void;
  /** Returns pin state for PORTB / PORTC / PORTD */
  get_port_state(portName: 'PORTB' | 'PORTC' | 'PORTD'): PortState;
  /** Inject an external logic level on an input pin */
  set_pin_input(portName: 'PORTB' | 'PORTC' | 'PORTD', pin: number, high: boolean): void;
  /** Total simulated cycles elapsed since last reset (as double for JS safety) */
  total_cycles(): number;
  /** Current stack pointer (proxy read used for debug info) */
  get_pc(): number;
  /** Read a byte from the unified data space */
  read_data(addr: number): number;
  /** Drain all bytes transmitted by USART0 since the last call */
  poll_serial_output(): string;
  /** Queue bytes to inject into USART0 RX (consumed one-by-one as firmware reads) */
  send_serial_input(data: string): void;
  /** Free the C++ object */
  delete(): void;
}

export interface AvrionModule {
  Simulator: new () => SimulatorInstance;
}

declare const createModule: (opts?: { locateFile?: (f: string) => string }) => Promise<AvrionModule>;
export default createModule;
