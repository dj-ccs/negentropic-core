// TypeScript bindings for emscripten-generated code.  Automatically generated at compile time.
declare namespace RuntimeExports {
    /**
     * @param {string|null=} returnType
     * @param {Array=} argTypes
     * @param {Array=} args
     * @param {Object=} opts
     */
    function ccall(ident: any, returnType?: (string | null) | undefined, argTypes?: any[] | undefined, args?: any[] | undefined, opts?: any | undefined): any;
    /**
     * @param {string=} returnType
     * @param {Array=} argTypes
     * @param {Object=} opts
     */
    function cwrap(ident: any, returnType?: string | undefined, argTypes?: any[] | undefined, opts?: any | undefined): any;
}
interface WasmModule {
  _free(_0: number): void;
  _malloc(_0: number): number;
  _neg_create(_0: number): number;
  _neg_destroy(_0: number): void;
  _neg_step(_0: number, _1: number): number;
  _neg_reset_from_binary(_0: number, _1: number, _2: number): number;
  _neg_get_state_binary(_0: number, _1: number, _2: number): number;
  _neg_get_state_binary_size(_0: number): number;
  _neg_get_state_json(_0: number, _1: number, _2: number): number;
  _neg_get_state_hash(_0: number): BigInt;
}

export type MainModule = WasmModule & typeof RuntimeExports;
export default function MainModuleFactory (options?: unknown): Promise<MainModule>;
