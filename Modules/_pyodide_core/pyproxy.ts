declare var Tests: any;
declare var Module: any;
declare var API: {
  fatal_error: (e: any) => never;
  [k: string]: any;
};

declare function _Py_IncRef(ptr: number): void;
declare function _Py_DecRef(ptr: number): void;
declare function _PyErr_Occurred(): number;
declare function _PyObject_Size(ptr: number): number;
declare function _PyObject_GetIter(ptr: number): number;


declare function _pythonexc2js(): never;
declare function __pyproxy_type(ptr: number): string;
declare function __pyproxy_str(ptr: number): string;
declare function _pyproxy_getflags(ptr: number): number;

declare function __pyproxy_hasattr(ptr: number, key: any): number;
declare function __pyproxy_getattr(ptr: number, key: any, cache: Map<string, any>): any;
declare function __pyproxy_setattr(ptr: number, key: any, val: any): number;
declare function __pyproxy_delattr(ptr: number, key: any): number;
declare function __pyproxy_ownKeys(ptr: number): (string | symbol)[];

declare function __pyproxy_contains(ptr: number, key: any): number;
declare function __pyproxy_getitem(ptr: number, key: any): any;
declare function __pyproxy_setitem(ptr: number, key: any, val: any): number;
declare function __pyproxy_delitem(ptr: number, key: any): number;
declare function __pyproxy_apply(
  ptr: number,
  jsargs: any[],
  num_pos_args: number,
  kwargs_names: string[],
  num_kwargs: number,
): any;
declare function __pyproxy_iter_next(ptr: number): any;
declare function __pyproxyGen_Send(ptr: number, arg: any): IteratorResult<any>;
declare function __pyproxyGen_return(ptr: number, arg: any): IteratorResult<any>;
declare function __pyproxyGen_throw(ptr: number, arg: any): IteratorResult<any>;
declare function __pyproxy_pop(ptr: number, pop_start: boolean): number;
declare function __pyproxy_slice_assign(ptr: number, start: number, stop: number, value: any): any;

// pyodide-skip

// Just for this file, we implement a special "skip" pragma. These lines are
// skipped by the Makefile when producing pyproxy.gen.ts These are actually C
// macros, but we declare them to make typescript okay with processing the raw
// file. We need to process the raw file to generate the docs because the C
// preprocessor deletes comments which kills all the docstrings.

// These declarations make Typescript accept the raw file. However, if we macro
// preprocess these lines, we get a bunch of syntax errors so they need to be
// removed from the preprocessed version.

// This also has the benefit that it makes intellisense happy.
declare var HAS_CONTAINS: number;
declare var HAS_GET: number;
declare var HAS_LENGTH: number;
declare var HAS_SET: number;
declare var IS_CALLABLE: number;
declare var IS_DICT: number;
declare var IS_GENERATOR: number;
declare var IS_ITERABLE: number;
declare var IS_ITERATOR: number;
declare var IS_SEQUENCE: number;
declare var IS_MUTABLE_SEQUENCE: number;

declare function DEREF_U32(ptr: number, offset: number): number;
declare function Py_ENTER(): void;
declare function Py_EXIT(): void;
// end-pyodide-skip

Module.finalizationRegistry = new FinalizationRegistry(
  ({ ptr }: PyProxyShared) => {
    try {
      Py_ENTER();
      _Py_DecRef(ptr);
      Py_EXIT();
    } catch (e) {
      // I'm not really sure what happens if an error occurs inside of a
      // finalizer...
      API.fatal_error(e);
    }
  },
);

function isPyProxy(jsobj: any): jsobj is PyProxy {
  try {
    return jsobj instanceof PyProxy;
  } catch (e) {
    return false;
  }
}
API.isPyProxy = isPyProxy;

type PyProxyCache = {
  map: Map<string, any>;
  json_adaptor_map: Map<string, any>;
  refcnt: number;
  leaked?: boolean;
};
type PyProxyShared = {
  ptr: number;
  cache: PyProxyCache;
  destroyed_msg: string | undefined;
  gcRegistered: boolean;
};

type PyProxyProps = {
  /**
   * captureThis tracks whether this should be passed as the first argument to
   * the Python function or not. We keep it false by default. To make a PyProxy
   * where the ``this`` argument is included, call the :js:meth:`captureThis` method.
   */
  captureThis: boolean;
  /**
   * isBound tracks whether bind has been called
   */
  isBound: boolean;
  /**
   * the ``this`` value that has been bound to the PyProxy
   */
  boundThis?: any;
  /**
   * Any extra arguments passed to bind are used for partial function
   * application. These are stored here.
   */
  boundArgs: any[];
};

type PyProxyAttrs = {
  // shared between aliases but not between copies
  shared: PyProxyShared;
  // properties that may be different between aliases
  props: PyProxyProps;
};

const pyproxyAttrsSymbol = Symbol("pyproxy.attrs");

function pyproxy_getflags(ptrobj: number) {
  Py_ENTER();
  try {
    return _pyproxy_getflags(ptrobj);
  } finally {
    Py_EXIT();
  }
}

/**
 * Create a new PyProxy wrapping ptrobj which is a PyObject*.
 *
 * Two proxies are **aliases** if they share `shared` (they may have different
 * props). Aliases are created by `bind` and `captureThis`. Aliases share the
 * same lifetime: `destroy` destroys both of them, they are only registered with
 * the garbage collector once, they only own a single refcount.  An **alias** is
 * created by passing the shared option.
 *
 * Two proxies are **copies** if they share `shared.cache`. Two copies share
 * attribute caches but they otherwise have independent lifetimes. The attribute
 * caches are refcounted so that they can be cleaned up when all copies are
 * destroyed. A **copy** is made by passing the `cache` argument.
 *
 * In the case that the Python object is callable, PyProxy inherits from
 * Function so that PyProxy objects can be callable. In that case we MUST expose
 * certain properties inherited from Function, but we do our best to remove as
 * many as possible.
 */
function pyproxy_new(
  ptr: number,
  {
    flags: flags_arg,
    props,
    shared,
    cache,
    gcRegister,
  }: {
    flags?: number;
    cache?: PyProxyCache;
    shared?: PyProxyShared;
    props?: any;
    gcRegister?: boolean;
  } = {},
): PyProxy {
  if (gcRegister === undefined) {
    // register by default
    gcRegister = true;
  }
  const flags = flags_arg !== undefined ? flags_arg : pyproxy_getflags(ptr);
  if (flags === -1) {
    _pythonexc2js();
  }
  const is_sequence = flags & IS_SEQUENCE;
  const is_dict = flags & IS_DICT;
  const cls = getPyProxyClass(flags);
  let target: any;
  if (flags & IS_CALLABLE) {
    // In this case we are effectively subclassing Function in order to ensure
    // that the proxy is callable. With a Content Security Protocol that doesn't
    // allow unsafe-eval, we can't invoke the Function constructor directly. So
    // instead we create a function in the universally allowed way and then use
    // `setPrototypeOf`. The documentation for `setPrototypeOf` says to use
    // `Object.create` or `Reflect.construct` instead for performance reasons
    // but neither of those work here.
    target = function () {};
    Object.setPrototypeOf(target, cls.prototype);
    // Remove undesirable properties added by Function constructor. Note: we
    // can't remove "arguments" or "caller" because they are not configurable
    // and not writable
    // @ts-ignore
    delete target.length;
    // @ts-ignore
    delete target.name;
    // prototype isn't configurable so we can't delete it but it's writable.
    target.prototype = undefined;
  } else {
    target = Object.create(cls.prototype);
  }

  const isAlias = !!shared;
  if (!shared) {
    // Not an alias so we have to make `shared`.
    if (!cache) {
      // In this case it's not a copy.
      cache = { map: new Map(), json_adaptor_map: new Map(), refcnt: 0 };
    }
    cache.refcnt++;
    shared = {
      ptr,
      cache,
      destroyed_msg: undefined,
      gcRegistered: false,
    };
    _Py_IncRef(ptr);
  }

  props = Object.assign(
    { isBound: false, captureThis: false, boundArgs: [] },
    props,
  );
  let handlers;
  if (is_dict) {
    handlers = PyProxyDictHandlers;
  } else if (is_sequence) {
    handlers = PyProxySequenceHandlers;
  } else {
    handlers = PyProxyHandlers;
  }
  let proxy = new Proxy(target, handlers);
  if (!isAlias && gcRegister) {
    // we need to register only once for a set of aliases. we can't register the
    // proxy directly since that isn't shared between aliases. The aliases all
    // share $$ so we can register that. They also need access to the data in
    // $$, but we can't use $$ itself as the held object since that would keep
    // $$ from being gc'd ever. So we make a copy. To prevent double free, we
    // have to be careful to unregister when we destroy.
    // gc_register_proxy(shared);
  }
  const attrs = { shared, props };
  target[pyproxyAttrsSymbol] = attrs;
  return proxy;
}
Module.pyproxy_new = pyproxy_new;

function gc_register_proxy(shared: PyProxyShared) {
  const shared_copy = Object.assign({}, shared);
  shared.gcRegistered = true;
  Module.finalizationRegistry.register(shared, shared_copy, shared);
}
Module.gc_register_proxy = gc_register_proxy;

function _getAttrsQuiet(jsobj: any): PyProxyAttrs {
  return jsobj[pyproxyAttrsSymbol];
}
Module.PyProxy_getAttrsQuiet = _getAttrsQuiet;
function _getAttrs(jsobj: any): PyProxyAttrs {
  const attrs = _getAttrsQuiet(jsobj);
  if (!attrs.shared.ptr) {
    throw new Error(attrs.shared.destroyed_msg);
  }
  return attrs;
}
API.PyProxy_getAttrs = _getAttrs;

function _getPtr(jsobj: any) {
  return _getAttrs(jsobj).shared.ptr;
}
Module.PyProxy_getPtr = _getPtr;

function _getFlags(jsobj: any): number {
  return Object.getPrototypeOf(jsobj).$$flags;
}

let pyproxyClassMap = new Map();
/**
 * Retrieve the appropriate mixins based on the features requested in flags.
 * Used by pyproxy_new. The "flags" variable is produced by the C function
 * pyproxy_getflags. Multiple PyProxies with the same set of feature flags
 * will share the same prototype, so the memory footprint of each individual
 * PyProxy is minimal.
 */
function getPyProxyClass(flags: number) {
  let result = pyproxyClassMap.get(flags);
  if (result) {
    return result;
  }
  let descriptors: any = {};

  const FLAG_TYPE_PAIRS: [number, any][] = [
    [HAS_CONTAINS, PyContainsMethods],
    [HAS_GET, PyGetItemMethods],
    [HAS_LENGTH, PyLengthMethods],
    [HAS_SET, PySetItemMethods],
    [IS_CALLABLE, PyCallableMethods],
    [IS_GENERATOR, PyGeneratorMethods],
    [IS_ITERABLE, PyIterableMethods],
    [IS_ITERATOR, PyIteratorMethods],
    [IS_SEQUENCE, PySequenceMethods],
    [IS_MUTABLE_SEQUENCE, PyMutableSequenceMethods],
  ];
  for (let [feature_flag, methods] of FLAG_TYPE_PAIRS) {
    if (flags & feature_flag) {
      Object.assign(
        descriptors,
        Object.getOwnPropertyDescriptors(methods.prototype),
      );
    }
  }
  // Use base constructor (just throws an error if construction is attempted).
  descriptors.constructor = Object.getOwnPropertyDescriptor(
    PyProxyProto,
    "constructor",
  );
  Object.assign(
    descriptors,
    Object.getOwnPropertyDescriptors({ $$flags: flags }),
  );
  const super_proto = flags & IS_CALLABLE ? PyProxyFunctionProto : PyProxyProto;
  const sub_proto = Object.create(super_proto, descriptors);
  function NewPyProxyClass() {}
  NewPyProxyClass.prototype = sub_proto;
  pyproxyClassMap.set(flags, NewPyProxyClass);
  return NewPyProxyClass;
}

class PyProxy {
  /** @private */
  $$flags: number;

  /** @private */
  static [Symbol.hasInstance](obj: any): obj is PyProxy {
    return [PyProxy].some((cls) =>
      Function.prototype[Symbol.hasInstance].call(cls, obj),
    );
  }

  /**
   * @hideconstructor
   */
  constructor() {
    throw new TypeError("PyProxy is not a constructor");
  }

  /** @hidden */
  get [Symbol.toStringTag]() {
    return "PyProxy";
  }
  /**
   * The name of the type of the object.
   *
   * Usually the value is ``"module.name"`` but for builtins or
   * interpreter-defined types it is just ``"name"``. As pseudocode this is:
   *
   * .. code-block:: python
   *
   *    ty = type(x)
   *    if ty.__module__ == 'builtins' or ty.__module__ == "__main__":
   *        return ty.__name__
   *    else:
   *        ty.__module__ + "." + ty.__name__
   *
   */
  get type(): string {
    let ptrobj = _getPtr(this);
    return __pyproxy_type(ptrobj);
  }
  /**
   * Returns `str(o)` (unless `pyproxyToStringRepr: true` was passed to
   * :js:func:`~globalThis.loadPyodide` in which case it will return `repr(o)`)
   */
  toString(): string {
    let ptrobj = _getPtr(this);
    let result;
    try {
      Py_ENTER();
      result = __pyproxy_str(ptrobj);
      Py_EXIT();
    } catch (e) {
      API.fatal_error(e);
    }
    if (result === null) {
      _pythonexc2js();
    }
    return result;
  }
  /**
   * Destroy the :js:class:`~pyodide.ffi.PyProxy`. This will release the memory. Any further attempt
   * to use the object will raise an error.
   *
   * In a browser supporting :js:data:`FinalizationRegistry`, Pyodide will
   * automatically destroy the :js:class:`~pyodide.ffi.PyProxy` when it is garbage collected, however
   * there is no guarantee that the finalizer will be run in a timely manner so
   * it is better to destroy the proxy explicitly.
   *
   * @param options
   * @param options.message The error message to print if use is attempted after
   *        destroying. Defaults to "Object has already been destroyed".
   *
   */
  destroy(options: { message?: string } = {}) {
    options = Object.assign({ message: "" }, options);
    const { message: m } = options;
    Module.pyproxy_destroy(this, m);
  }
}
const PyProxyProto = PyProxy.prototype;

const pyproxy_cache_destroyed_msg =
  "This borrowed attribute proxy was automatically destroyed in the " +
  "process of destroying the proxy it was borrowed from. Try using the 'copy' method.";

function pyproxy_decref_cache(cache: PyProxyCache) {
  if (!cache) {
    return;
  }
  cache.refcnt--;
  if (cache.leaked) {
    return;
  }
  if (cache.refcnt === 0) {
    for (const proxy of cache.map.values()) {
      Module.pyproxy_destroy(proxy, pyproxy_cache_destroyed_msg, true);
    }
    for (const proxy of cache.json_adaptor_map.values()) {
      Module.pyproxy_destroy(proxy, pyproxy_cache_destroyed_msg, true);
    }
  }
}

function pyproxy_destroy(proxy: PyProxy, destroyed_msg: string) {
  const { shared } = _getAttrsQuiet(proxy);
  if (!shared.ptr) {
    // already destroyed
    return;
  }
  shared.destroyed_msg = destroyed_msg ?? "Object has already been destroyed";
  // Maybe the destructor will call JavaScript code that will somehow try
  // to use this proxy. Mark it deleted before decrementing reference count
  // just in case!
  const ptr = shared.ptr;
  shared.ptr = 0;
  if (shared.gcRegistered) {
    Module.finalizationRegistry.unregister(shared);
  }
  pyproxy_decref_cache(shared.cache);


  try {
    Py_ENTER();
    _Py_DecRef(ptr);
    Py_EXIT();
  } catch (e) {
    API.fatal_error(e);
  }
}
Module.pyproxy_destroy = pyproxy_destroy;

const filteredHasKeySet: Set<string | symbol> = new Set([
  "name",
  "length",
  "caller",
  "arguments",
]);

function filteredHasKey(
  jsobj: PyProxy,
  jskey: string | symbol,
  filterProto: boolean,
) {
  if (jsobj instanceof Function) {
    // If we are a PyProxy of a callable we have to subclass function so that if
    // someone feature detects callables with `instanceof Function` it works
    // correctly. But the callable might have attributes `name` and `length` and
    // we don't want to shadow them with the values from `Function.prototype`.
    return (
      jskey in jsobj &&
      !(
        filteredHasKeySet.has(jskey) ||
        // we are required by JS law to return `true` for `"prototype" in pycallable`
        // but we are allowed to return the value of `getattr(pycallable, "prototype")`.
        // So we filter prototype out of the "get" trap but not out of the "has" trap
        (filterProto && jskey === "prototype")
      )
    );
  } else {
    return jskey in jsobj;
  }
}

const PyProxyHandlers = {
  isExtensible(): boolean {
    return true;
  },
  has(jsobj: PyProxy, jskey: string | symbol): boolean {
    // Note: must report "prototype" in proxy when we are callable.
    // (We can return the wrong value from "get" handler though.)
    if (filteredHasKey(jsobj, jskey, false)) {
      return true;
    }
    // python_hasattr will crash if given a Symbol.
    if (typeof jskey === "symbol") {
      return false;
    }
    if (jskey.startsWith("$")) {
      jskey = jskey.slice(1);
    }
    return python_hasattr(jsobj, jskey);
  },
  get(jsobj: PyProxy, jskey: string | symbol): any {
    // Preference order:
    // 1. stuff from JavaScript
    // 2. the result of Python getattr
    // python_getattr will crash if given a Symbol.
    if (typeof jskey === "symbol" || filteredHasKey(jsobj, jskey, true)) {
      return Reflect.get(jsobj, jskey);
    }
    // If keys start with $ remove the $. User can use initial $ to
    // unambiguously ask for a key on the Python object.
    if (jskey.startsWith("$")) {
      jskey = jskey.slice(1);
    }
    // 2. The result of getattr
    return python_getattr(jsobj, jskey);
  },
  set(jsobj: PyProxy, jskey: string | symbol, jsval: any): boolean {
    let descr = Object.getOwnPropertyDescriptor(jsobj, jskey);
    if (descr && !descr.writable && !descr.set) {
      return false;
    }
    // python_setattr will crash if given a Symbol.
    if (typeof jskey === "symbol" || filteredHasKey(jsobj, jskey, true)) {
      return Reflect.set(jsobj, jskey, jsval);
    }
    if (jskey.startsWith("$")) {
      jskey = jskey.slice(1);
    }
    python_setattr(jsobj, jskey, jsval);
    return true;
  },
  deleteProperty(jsobj: PyProxy, jskey: string | symbol): boolean {
    let descr = Object.getOwnPropertyDescriptor(jsobj, jskey);
    if (descr && !descr.configurable) {
      // Must return "false" if "jskey" is a nonconfigurable own property.
      // Otherwise JavaScript will throw a TypeError.
      // Strict mode JS will throw an error here saying that the property cannot
      // be deleted. It's good to leave everything alone so that the behavior is
      // consistent with the error message.
      return false;
    }
    if (typeof jskey === "symbol" || filteredHasKey(jsobj, jskey, true)) {
      return Reflect.deleteProperty(jsobj, jskey);
    }
    if (jskey.startsWith("$")) {
      jskey = jskey.slice(1);
    }
    python_delattr(jsobj, jskey);
    return true;
  },
  ownKeys(jsobj: PyProxy): (string | symbol)[] {
    let ptrobj = _getPtr(jsobj);
    let result;
    try {
      Py_ENTER();
      result = __pyproxy_ownKeys(ptrobj);
      Py_EXIT();
    } catch (e) {
      API.fatal_error(e);
    }
    if (result === Module.error) {
      _pythonexc2js();
    }
    result.push(...Reflect.ownKeys(jsobj));
    return result;
  },
  apply(jsobj: PyProxy & Function, jsthis: any, jsargs: any): any {
    return jsobj.apply(jsthis, jsargs);
  },
};



const PyProxyDictHandlersSet = new Set([
  "copy",
  "constructor",
  "$$flags",
  "toString",
  "destroy",
]);

interface PythonError {
  type: string;
}

function isPythonError(e: any): e is PythonError {
  return (
    e &&
    typeof e === "object" &&
    e.constructor &&
    e.constructor.name === "PythonError"
  );
}


const PyProxySequenceHandlers = {
  isExtensible(): boolean {
    return true;
  },
  has(jsobj: PyProxy & {length: number}, jskey: any): boolean {
    if (typeof jskey === "string" && /^[0-9]+$/.test(jskey)) {
      return Number(jskey) < jsobj.length;
    }
    return PyProxyHandlers.has(jsobj, jskey);
  },
  get(jsobj: PyProxy & {length: number}, jskey: any): any {
    if (jskey === "length") {
      return jsobj.length;
    }
    if (typeof jskey === "string" && /^[0-9]+$/.test(jskey)) {
      try {
        return PyGetItemMethods.prototype.get.call(jsobj, Number(jskey));
      } catch (e) {
        if (isPythonError(e) && e.type == "IndexError") {
          return undefined;
        }
        throw e;
      }
    }
    return PyProxyHandlers.get(jsobj, jskey);
  },
  set(jsobj: PyProxy, jskey: any, jsval: any): boolean {
    if (typeof jskey === "string" && /^[0-9]+$/.test(jskey)) {
      try {
        PySetItemMethods.prototype.set.call(jsobj, Number(jskey), jsval);
        return true;
      } catch (e) {
        if (isPythonError(e) && e.type == "IndexError") {
          return false;
        }
        throw e;
      }
    }
    return PyProxyHandlers.set(jsobj, jskey, jsval);
  },
  deleteProperty(jsobj: PyProxy, jskey: any): boolean {
    if (typeof jskey === "string" && /^[0-9]+$/.test(jskey)) {
      try {
        PySetItemMethods.prototype.delete.call(jsobj, Number(jskey));
        return true;
      } catch (e) {
        if (isPythonError(e) && e.type == "IndexError") {
          return false;
        }
        throw e;
      }
    }
    return PyProxyHandlers.deleteProperty(jsobj, jskey);
  },
  ownKeys(jsobj: PyProxy & {length: number}): (string | symbol)[] {
    const result = PyProxyHandlers.ownKeys(jsobj);
    result.push(
      ...Array.from({ length: jsobj.length }, (_, k) => k.toString()),
    );
    result.push("length");
    return result;
  },
};

const PyProxyDictHandlers = {
  isExtensible(): boolean {
    return true;
  },
  has(jsobj: PyProxy, jskey: string | symbol): boolean {
    if (PyContainsMethods.prototype.has.call(jsobj, jskey)) {
      return true;
    }
    if (typeof jskey === "string" && /^[0-9]+$/.test(jskey)) {
      return PyContainsMethods.prototype.has.call(jsobj, Number(jskey));
    }
    return false;
  },
  get(jsobj: PyProxy, jskey: string | symbol): any {
    if (
      typeof jskey === "symbol" ||
      PyProxyDictHandlersSet.has(jskey)
    ) {
      // @ts-ignore
      return Reflect.get(...arguments);
    }
    const result = PyGetItemMethods.prototype.get.call(jsobj, jskey);
    if (
      result !== undefined ||
      PyContainsMethods.prototype.has.call(jsobj, jskey)
    ) {
      return result;
    }
    if (typeof jskey === "string" && /^[0-9]+$/.test(jskey)) {
      return PyGetItemMethods.prototype.get.call(jsobj, Number(jskey));
    }
    // @ts-ignore
    return Reflect.get(...arguments);
  },
  set(jsobj: PyProxy, jskey: string | symbol | number, jsval: any): boolean {
    if (typeof jskey === "symbol") {
      return false;
    }
    if (
      !PyContainsMethods.prototype.has.call(jsobj, jskey) &&
      typeof jskey === "string" &&
      /^[0-9]+$/.test(jskey)
    ) {
      jskey = Number(jskey);
    }
    try {
      PySetItemMethods.prototype.set.call(jsobj, jskey, jsval);
      return true;
    } catch (e) {
      if (isPythonError(e) && e.type === "KeyError") {
        return false;
      }
      throw e;
    }
  },
  deleteProperty(jsobj: PyProxy, jskey: string | symbol | number): boolean {
    if (typeof jskey === "symbol") {
      return false;
    }
    if (
      !PyContainsMethods.prototype.has.call(jsobj, jskey) &&
      typeof jskey === "string" &&
      /^[0-9]+$/.test(jskey)
    ) {
      jskey = Number(jskey);
    }
    try {
      PySetItemMethods.prototype.delete.call(jsobj, jskey);
      return true;
    } catch (e) {
      if (isPythonError(e) && e.type === "KeyError") {
        return false;
      }
      throw e;
    }
  },
  getOwnPropertyDescriptor(jsobj: PyProxy, prop: any) {
    if (!PyProxyDictHandlers.has(jsobj, prop)) {
      return undefined;
    }
    const value = PyProxyDictHandlers.get(jsobj, prop);
    return {
      configurable: true,
      enumerable: true,
      value,
      writable: true,
    };
  },
  ownKeys(jsobj: PyProxy): (string | symbol)[] {
    const result: Set<string | symbol> = new Set();
    dictOwnKeysHelper(jsobj, result);
    return Array.from(result);
  },
};

function dictOwnKeysHelper(jsobj: PyProxy, result: Set<string | symbol>): void {
  const dictKeysView: Iterable<any> & PyProxy = PyProxyHandlers.get(
    jsobj,
    "keys",
  )();
  for (const key of dictKeysView) {
    if (typeof key === "string") {
      result.add(key);
    } else if (typeof key === "number") {
      result.add(key.toString());
    }
  }
  dictKeysView.destroy();
}

// Another layer of boilerplate. The PyProxyHandlers have some annoying logic to
// deal with straining out the spurious "Function" properties "prototype",
// "arguments", and "length", to deal with correctly satisfying the Proxy
// invariants, and to deal with the mro
function python_hasattr(jsobj: PyProxy, jskey: any) {
  let ptrobj = _getPtr(jsobj);
  let result;
  try {
    Py_ENTER();
    result = __pyproxy_hasattr(ptrobj, jskey);
    Py_EXIT();
  } catch (e) {
    API.fatal_error(e);
  }
  if (result === -1) {
    _pythonexc2js();
  }
  return result !== 0;
}

// Returns a JsRef in order to allow us to differentiate between "not found"
// (in which case we return 0) and "found 'None'" (in which case we return
// undefined).
function python_getattr(jsobj: PyProxy, key: any) {
  const { shared } = _getAttrs(jsobj);
  let cache = shared.cache.map;
  let result;
  try {
    Py_ENTER();
    result = __pyproxy_getattr(shared.ptr, key, cache);
    Py_EXIT();
  } catch (e) {
    API.fatal_error(e);
  }
  if (result === Module.error) {
    if (_PyErr_Occurred()) {
      _pythonexc2js();
    }
    return undefined;
  }
  return result;
}

function python_setattr(jsobj: PyProxy, jskey: any, jsval: any) {
  let ptrobj = _getPtr(jsobj);
  let err;
  try {
    Py_ENTER();
    err = __pyproxy_setattr(ptrobj, jskey, jsval);
    Py_EXIT();
  } catch (e) {
    API.fatal_error(e);
  }
  if (err === -1) {
    _pythonexc2js();
  }
}

function python_delattr(jsobj: PyProxy, jskey: any) {
  let ptrobj = _getPtr(jsobj);
  let err;
  try {
    Py_ENTER();
    err = __pyproxy_delattr(ptrobj, jskey);
    Py_EXIT();
  } catch (e) {
    API.fatal_error(e);
  }
  if (err === -1) {
    _pythonexc2js();
  }
}

class PyProxyWithHas extends PyProxy {
  /** @private */
  static [Symbol.hasInstance](obj: any): obj is PyProxy {
    return API.isPyProxy(obj) && !!(_getFlags(obj) & HAS_CONTAINS);
  }
}

interface PyProxyWithHas extends PyContainsMethods {}

// Controlled by HAS_CONTAINS flag, appears for any class with __contains__ or
// sq_contains
class PyContainsMethods {
  /**
   * This translates to the Python code ``key in obj``.
   *
   * @param key The key to check for.
   * @returns Is ``key`` present?
   */
  has(key: any): boolean {
    let ptrobj = _getPtr(this);
    let result;
    try {
      Py_ENTER();
      result = __pyproxy_contains(ptrobj, key);
      Py_EXIT();
    } catch (e) {
      API.fatal_error(e);
    }
    if (result === -1) {
      _pythonexc2js();
    }
    return result === 1;
  }
}

class PyProxyWithGet extends PyProxy {
  /** @private */
  static [Symbol.hasInstance](obj: any): obj is PyProxy {
    return API.isPyProxy(obj) && !!(_getFlags(obj) & HAS_GET);
  }
}

interface PyProxyWithGet extends PyGetItemMethods {}


// Controlled by HAS_GET, appears for any class with __getitem__,
// mp_subscript, or sq_item methods
export class PyGetItemMethods {
  /**
   * This translates to the Python code ``obj[key]``.
   *
   * @param key The key to look up.
   * @returns The corresponding value.
   */
  get(key: any): any {
    const { shared } = _getAttrs(this);
    let result;
    try {
      Py_ENTER();
      // Cache is only used if isJsonAdaptor is true.
      result = __pyproxy_getitem(
        shared.ptr,
        key,
      );
      Py_EXIT();
    } catch (e) {
      API.fatal_error(e);
    }
    if (result === Module.error) {
      if (_PyErr_Occurred()) {
        _pythonexc2js();
      } else {
        return undefined;
      }
    }
    return result;
  }
}

class PyProxyWithLength extends PyProxy {
  /** @private */
  static [Symbol.hasInstance](obj: any): obj is PyProxy {
    return API.isPyProxy(obj) && !!(_getFlags(obj) & HAS_LENGTH);
  }
}

interface PyProxyWithLength extends PyLengthMethods {}

// Controlled by HAS_LENGTH, appears for any object with __len__ or sq_length
// or mp_length methods
class PyLengthMethods {
  /**
   * The length of the object.
   */
  get length(): number {
    let ptrobj = _getPtr(this);
    let length;
    try {
      Py_ENTER();
      length = _PyObject_Size(ptrobj);
      Py_EXIT();
    } catch (e) {
      API.fatal_error(e);
    }
    if (length === -1) {
      _pythonexc2js();
    }
    return length;
  }
}



interface PyProxyWithSet extends PySetItemMethods {}
// Controlled by HAS_SET, appears for any class with __setitem__, __delitem__,
// mp_ass_subscript,  or sq_ass_item.
class PySetItemMethods {
  /**
   * This translates to the Python code ``obj[key] = value``.
   *
   * @param key The key to set.
   * @param value The value to set it to.
   */
  set(key: any, value: any) {
    let ptrobj = _getPtr(this);
    let err;
    try {
      Py_ENTER();
      err = __pyproxy_setitem(ptrobj, key, value);
      Py_EXIT();
    } catch (e) {
      API.fatal_error(e);
    }
    if (err === -1) {
      _pythonexc2js();
    }
  }
  /**
   * This translates to the Python code ``del obj[key]``.
   *
   * @param key The key to delete.
   */
  delete(key: any) {
    let ptrobj = _getPtr(this);
    let err;
    try {
      Py_ENTER();
      err = __pyproxy_delitem(ptrobj, key);
      Py_EXIT();
    } catch (e) {
      API.fatal_error(e);
    }
    if (err === -1) {
      _pythonexc2js();
    }
  }
}

function _adjustArgs(proxyobj: any, jsthis: any, jsargs: any[]): any[] {
  const { captureThis, boundArgs, boundThis, isBound } =
    _getAttrs(proxyobj).props;
  if (captureThis) {
    if (isBound) {
      return [boundThis].concat(boundArgs, jsargs);
    } else {
      return [jsthis].concat(jsargs);
    }
  }
  if (isBound) {
    return boundArgs.concat(jsargs);
  }
  return jsargs;
}

const PyProxyFunctionProto = Object.create(
  Function.prototype,
  Object.getOwnPropertyDescriptors(PyProxyProto),
);
function PyProxyFunction() {}
PyProxyFunction.prototype = PyProxyFunctionProto;

export class PyCallableMethods {
  /**
   * The ``apply()`` method calls the specified function with a given this
   * value, and arguments provided as an array (or an array-like object). Like
   * :js:meth:`Function.apply`.
   *
   * @param thisArg The ``this`` argument. Has no effect unless the
   * :js:class:`~pyodide.ffi.PyCallable` has :js:meth:`captureThis` set. If
   * :js:meth:`captureThis` is set, it will be passed as the first argument to
   * the Python function.
   * @param jsargs The array of arguments
   * @returns The result from the function call.
   */
  apply(thisArg: any, jsargs: any) {
    // Convert jsargs to an array using ordinary .apply in order to match the
    // behavior of .apply very accurately.
    jsargs = function (...args: any) {
      return args;
    }.apply(undefined, jsargs);
    jsargs = _adjustArgs(this, thisArg, jsargs);
    return callPyObject(_getPtr(this), jsargs);
  }
  /**
   * Calls the function with a given this value and arguments provided
   * individually. See :js:meth:`Function.call`.
   *
   * @param thisArg The ``this`` argument. Has no effect unless the
   * :js:class:`~pyodide.ffi.PyCallable` has :js:meth:`captureThis` set. If
   * :js:meth:`captureThis` is set, it will be passed as the first argument to
   * the Python function.
   * @param jsargs The arguments
   * @returns The result from the function call.
   */
  call(thisArg: any, ...jsargs: any) {
    jsargs = _adjustArgs(this, thisArg, jsargs);
    return callPyObject(_getPtr(this), jsargs);
  }

  /**
   * Call the Python function. The first parameter controls various parameters
   * that change the way the call is performed.
   *
   * @param options
   * @param options.kwargs If true, the last argument is treated as a collection
   *                       of keyword arguments.
   * @param jsargs Arguments to the Python function.
   * @returns
   */
  callWithOptions({ kwargs }: { kwargs?: boolean }, ...jsargs: any) {
    let kwarg = {};
    if (kwargs) {
      if (jsargs.length === 0) {
        throw new TypeError(
          "callWithOptions with 'kwargs: true' requires at least one argument (the key word argument object)",
        );
      }
      kwarg = jsargs.pop();
      if (
        kwarg.constructor !== undefined &&
        kwarg.constructor.name !== "Object"
      ) {
        throw new TypeError("kwargs argument is not an object");
      }
    }
    return callPyObjectKwargs(_getPtr(this), jsargs, kwarg);
  }

  /**
   * Call the function with keyword arguments. The last argument must be an
   * object with the keyword arguments.
   */
  callKwargs(...jsargs: any) {
    if (jsargs.length === 0) {
      throw new TypeError(
        "callKwargs requires at least one argument (the key word argument object)",
      );
    }
    let kwargs = jsargs.pop();
    if (
      kwargs.constructor !== undefined &&
      kwargs.constructor.name !== "Object"
    ) {
      throw new TypeError("kwargs argument is not an object");
    }
    return callPyObjectKwargs(_getPtr(this), jsargs, kwargs);
  }

  /**
   * The ``bind()`` method creates a new function that, when called, has its
   * ``this`` keyword set to the provided value, with a given sequence of
   * arguments preceding any provided when the new function is called. See
   * :js:meth:`Function.bind`.
   *
   * If the :js:class:`~pyodide.ffi.PyCallable` does not have
   * :js:meth:`captureThis` set, the ``this`` parameter will be discarded. If it
   * does have :js:meth:`captureThis` set, ``thisArg`` will be set to the first
   * argument of the Python function. The returned proxy and the original proxy
   * have the same lifetime so destroying either destroys both.
   *
   * @param thisArg The value to be passed as the ``this`` parameter to the
   * target function ``func`` when the bound function is called.
   * @param jsargs Extra arguments to prepend to arguments provided to the bound
   * function when invoking ``func``.
   * @returns
   */
  bind(thisArg: any, ...jsargs: any) {
    let { shared, props } = _getAttrs(this);
    const { boundArgs: boundArgsOld, boundThis: boundThisOld, isBound } = props;
    let boundThis = thisArg;
    if (isBound) {
      boundThis = boundThisOld;
    }
    let boundArgs = boundArgsOld.concat(jsargs);
    props = Object.assign({}, props, {
      boundArgs,
      isBound: true,
      boundThis,
    });
    return pyproxy_new(shared.ptr, {
      shared,
      flags: _getFlags(this),
      props,
    });
  }

  /**
   * Returns a :js:class:`~pyodide.ffi.PyProxy` that passes ``this`` as the first argument to the
   * Python function. The returned :js:class:`~pyodide.ffi.PyProxy` has the internal ``captureThis``
   * property set.
   *
   * It can then be used as a method on a JavaScript object. The returned proxy
   * and the original proxy have the same lifetime so destroying either destroys
   * both.
   *
   * For example:
   *
   * .. code-block:: pyodide
   *
   *    let obj = { a : 7 };
   *    pyodide.runPython(`
   *      def f(self):
   *        return self.a
   *    `);
   *    // Without captureThis, it doesn't work to use f as a method for obj:
   *    obj.f = pyodide.globals.get("f");
   *    obj.f(); // raises "TypeError: f() missing 1 required positional argument: 'self'"
   *    // With captureThis, it works fine:
   *    obj.f = pyodide.globals.get("f").captureThis();
   *    obj.f(); // returns 7
   *
   * @returns The resulting :js:class:`~pyodide.ffi.PyProxy`. It has the same lifetime as the
   * original :js:class:`~pyodide.ffi.PyProxy` but passes ``this`` to the wrapped function.
   *
   */
  captureThis(): PyProxy {
    let { props, shared } = _getAttrs(this);
    props = Object.assign({}, props, {
      captureThis: true,
    });
    return pyproxy_new(shared.ptr, {
      shared,
      flags: _getFlags(this),
      props,
    });
  }
}
// @ts-ignore
PyCallableMethods.prototype.prototype = Function.prototype;

// Now a lot of boilerplate to wrap the abstract Object protocol wrappers
// defined in pyproxy.c in JavaScript functions.

function callPyObjectKwargs(ptrobj: number, jsargs: any[], kwargs: any) {
  // We don't do any checking for kwargs, checks are in PyProxy.callKwargs
  // which only is used when the keyword arguments come from the user.
  const num_pos_args = jsargs.length;
  const kwargs_names = Object.keys(kwargs);
  const kwargs_values = Object.values(kwargs);
  const num_kwargs = kwargs_names.length;
  jsargs.push(...kwargs_values);

  let result;
  try {
    Py_ENTER();
    result = __pyproxy_apply(
      ptrobj,
      jsargs,
      num_pos_args,
      kwargs_names,
      num_kwargs,
    );
    Py_EXIT();
  } catch (e) {
    API.fatal_error(e);
    return;
  }
  if (result === Module.error) {
    _pythonexc2js();
  }
  return result;
}

function callPyObject(ptrobj: number, jsargs: any) {
  return callPyObjectKwargs(ptrobj, jsargs, {});
}


/**
 * A helper for [Symbol.iterator].
 *
 * Because "it is possible for a generator to be garbage collected without
 * ever running its finally block", we take extra care to try to ensure that
 * we don't leak the iterator. We register it with the finalizationRegistry,
 * but if the finally block is executed, we decref the pointer and unregister.
 *
 * In order to do this, we create the generator with this inner method,
 * register the finalizer, and then return it.
 *
 * Quote from:
 * https://hacks.mozilla.org/2015/07/es6-in-depth-generators-continued/
 *
 */
function* iter_helper(
  iterptr: number,
  token: {},
): Generator<any> {
  const to_destroy = [];
  try {
    while (true) {
      Py_ENTER();
      const item = __pyproxy_iter_next(iterptr);
      Py_EXIT();
      if (item === Module.error) {
        break;
      }
      yield item;
      // This is necessary to get JSON.stringify to work correctly.
      if (API.isPyProxy(item)) {
        to_destroy.push(item);
      }
    }
  } catch (e) {
    API.fatal_error(e);
  } finally {
    Module.finalizationRegistry.unregister(token);
    _Py_DecRef(iterptr);
  }
  try {
    to_destroy.forEach((e) =>
      Module.pyproxy_destroy(
        e,
        "This borrowed proxy was automatically destroyed when an iterator was exhausted.",
      ),
    );
  } catch (e) {}
  if (_PyErr_Occurred()) {
    _pythonexc2js();
  }
}


/**
 * A :js:class:`~pyodide.ffi.PyProxy` whose proxied Python object is a :std:term:`generator`
 * (i.e., it is an instance of :py:class:`~collections.abc.Generator`).
 */
class PyGenerator extends PyProxy {
  /** @private */
  static [Symbol.hasInstance](obj: any): obj is PyProxy {
    return API.isPyProxy(obj) && !!(_getFlags(obj) & IS_GENERATOR);
  }
}

interface PyGenerator extends PyGeneratorMethods {}

class PyGeneratorMethods {
  /**
   * Throws an exception into the Generator.
   *
   * See the documentation for :js:meth:`Generator.throw`.
   *
   * @param exc Error The error to throw into the generator. Must be an
   * instanceof ``Error``.
   * @returns An Object with two properties: ``done`` and ``value``. When the
   * generator yields ``some_value``, ``return`` returns ``{done : false, value
   * : some_value}``. When the generator raises a
   * ``StopIteration(result_value)`` exception, ``return`` returns ``{done :
   * true, value : result_value}``.
   */
  throw(exc: any): IteratorResult<any, any> {
    let result;
    try {
      Py_ENTER();
      result = __pyproxyGen_throw(_getPtr(this), exc);
      Py_EXIT();
    } catch (e) {
      API.fatal_error(e);
    }
    if (result === Module.error) {
      _pythonexc2js();
    }
    return result;
  }

  /**
   * Throws a :py:exc:`GeneratorExit` into the generator and if the
   * :py:exc:`GeneratorExit` is not caught returns the argument value ``{done:
   * true, value: v}``. If the generator catches the :py:exc:`GeneratorExit` and
   * returns or yields another value the next value of the generator this is
   * returned in the normal way. If it throws some error other than
   * :py:exc:`GeneratorExit` or :py:exc:`StopIteration`, that error is propagated. See
   * the documentation for :js:meth:`Generator.return`.
   *
   * @param v The value to return from the generator.
   * @returns An Object with two properties: ``done`` and ``value``. When the
   * generator yields ``some_value``, ``return`` returns ``{done : false, value
   * : some_value}``. When the generator raises a
   * ``StopIteration(result_value)`` exception, ``return`` returns ``{done :
   * true, value : result_value}``.
   */
  return(v: any): IteratorResult<any, any> {
    // Note: arg is optional, if arg is not supplied, it will be undefined
    // which gets converted to "Py_None". This is as intended.
    let result: IteratorResult<any, any>;
    try {
      Py_ENTER();
      result = __pyproxyGen_return(_getPtr(this), v);
      Py_EXIT();
    } catch (e) {
      API.fatal_error(e);
    }
    if (result === Module.error) {
      _pythonexc2js();
    }
    return result;
  }
}

/**
 * A :js:class:`~pyodide.ffi.PyProxy` whose proxied Python object is :std:term:`iterable`
 * (i.e., it has an :meth:`~object.__iter__` method).
 */
export class PyIterable extends PyProxy {
  /** @private */
  static [Symbol.hasInstance](obj: any): obj is PyProxy {
    return (
      API.isPyProxy(obj) && !!(_getFlags(obj) & (IS_ITERABLE | IS_ITERATOR))
    );
  }
}

export interface PyIterable extends PyIterableMethods {}

// Controlled by IS_ITERABLE, appears for any object with __iter__ or tp_iter,
// unless they are iterators. See: https://docs.python.org/3/c-api/iter.html
// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Iteration_protocols
// This avoids allocating a PyProxy wrapper for the temporary iterator.
export class PyIterableMethods {
  /**
   * This translates to the Python code ``iter(obj)``. Return an iterator
   * associated to the proxy. See the documentation for
   * :js:data:`Symbol.iterator`.
   *
   * This will be used implicitly by ``for(let x of proxy){}``.
   */
  [Symbol.iterator](): Iterator<any, any, any> {
    const { shared } = _getAttrs(this);
    let token = {};
    let iterptr;
    try {
      Py_ENTER();
      iterptr = _PyObject_GetIter(shared.ptr);
      Py_EXIT();
    } catch (e) {
      API.fatal_error(e);
    }
    if (iterptr === 0) {
      _pythonexc2js();
    }

    // Cache is only used if isJsonAdaptor is true.
    let result = iter_helper(
      iterptr,
      token,
    );
    Module.finalizationRegistry.register(result, [iterptr, undefined], token);
    return result;
  }
}


/**
 * A :js:class:`~pyodide.ffi.PyProxy` whose proxied Python object is an :term:`iterator`
 * (i.e., has a :meth:`~generator.send` or :meth:`~iterator.__next__` method).
 */
class PyIterator extends PyProxy {
  /** @private */
  static [Symbol.hasInstance](obj: any): obj is PyProxy {
    return API.isPyProxy(obj) && !!(_getFlags(obj) & IS_ITERATOR);
  }
}

interface PyIterator extends PyIteratorMethods {}

// Controlled by IS_ITERATOR, appears for any object with a __next__ or
// tp_iternext method.
class PyIteratorMethods {
  /** @private */
  [Symbol.iterator]() {
    return this;
  }
  /**
   * This translates to the Python code ``next(obj)``. Returns the next value of
   * the generator. See the documentation for :js:meth:`Generator.next` The
   * argument will be sent to the Python generator.
   *
   * This will be used implicitly by ``for(let x of proxy){}``.
   *
   * @param arg The value to send to the generator. The value will be assigned
   * as a result of a yield expression.
   * @returns An Object with two properties: ``done`` and ``value``. When the
   * generator yields ``some_value``, ``next`` returns ``{done : false, value :
   * some_value}``. When the generator raises a :py:exc:`StopIteration`
   * exception, ``next`` returns ``{done : true, value : result_value}``.
   */
  next(arg: any = undefined): IteratorResult<any, any> {
    // Note: arg is optional, if arg is not supplied, it will be undefined
    // which gets converted to "Py_None". This is as intended.
    let result;
    let done;
    try {
      Py_ENTER();
      result = __pyproxyGen_Send(_getPtr(this), arg);
      Py_EXIT();
    } catch (e) {
      API.fatal_error(e);
    }
    if (result === Module.error) {
      _pythonexc2js();
    }
    return result;
  }
}



/**
 * A :js:class:`~pyodide.ffi.PyProxy` whose proxied Python object is an
 * :py:class:`~collections.abc.Sequence` (i.e., a :py:class:`list`)
 */
class PySequence extends PyProxy {
  /** @private */
  static [Symbol.hasInstance](obj: any): obj is PyProxy {
    return API.isPyProxy(obj) && !!(_getFlags(obj) & IS_SEQUENCE);
  }
}

interface PySequence extends PySequenceMethods {}

// Missing:
// flatMap, flat,
class PySequenceMethods {
  /** @hidden */
  get [Symbol.isConcatSpreadable]() {
    return true;
  }
  /**
   * See :js:meth:`Array.join`. The :js:meth:`Array.join` method creates and
   * returns a new string by concatenating all of the elements in the
   * :py:class:`~collections.abc.Sequence`.
   *
   * @param separator A string to separate each pair of adjacent elements of the
   * Sequence.
   *
   * @returns  A string with all Sequence elements joined.
   */
  join(separator?: string) {
    return Array.prototype.join.call(this, separator);
  }
  /**
   * See :js:meth:`Array.slice`. The :js:meth:`Array.slice` method returns a
   * shallow copy of a portion of a :py:class:`~collections.abc.Sequence` into a
   * new array object selected from ``start`` to ``stop`` (`stop` not included)
   * @param start Zero-based index at which to start extraction. Negative index
   * counts back from the end of the Sequence.
   * @param stop Zero-based index at which to end extraction. Negative index
   * counts back from the end of the Sequence.
   * @returns A new array containing the extracted elements.
   */
  slice(start?: number, stop?: number): any {
    return Array.prototype.slice.call(this, start, stop);
  }
  /**
   * See :js:meth:`Array.lastIndexOf`. Returns the last index at which a given
   * element can be found in the Sequence, or -1 if it is not present.
   * @param elt Element to locate in the Sequence.
   * @param fromIndex Zero-based index at which to start searching backwards,
   * converted to an integer. Negative index counts back from the end of the
   * Sequence.
   * @returns The last index of the element in the Sequence; -1 if not found.
   */
  lastIndexOf(elt: any, fromIndex?: number) {
    if (fromIndex === undefined) {
      fromIndex = (this as any).length;
    }
    return Array.prototype.lastIndexOf.call(this, elt, fromIndex);
  }
  /**
   * See :js:meth:`Array.indexOf`. Returns the first index at which a given
   * element can be found in the Sequence, or -1 if it is not present.
   * @param elt Element to locate in the Sequence.
   * @param fromIndex Zero-based index at which to start searching, converted to
   * an integer. Negative index counts back from the end of the Sequence.
   * @returns The first index of the element in the Sequence; -1 if not found.
   */
  indexOf(elt: any, fromIndex?: number) {
    return Array.prototype.indexOf.call(this, elt, fromIndex);
  }
  /**
   * See :js:meth:`Array.forEach`. Executes a provided function once for each
   * ``Sequence`` element.
   * @param callbackfn A function to execute for each element in the ``Sequence``. Its
   * return value is discarded.
   * @param thisArg A value to use as ``this`` when executing ``callbackFn``.
   */
  forEach(callbackfn: (elt: any) => void, thisArg?: any) {
    Array.prototype.forEach.call(this, callbackfn, thisArg);
  }
  /**
   * See :js:meth:`Array.map`. Creates a new array populated with the results of
   * calling a provided function on every element in the calling ``Sequence``.
   * @param callbackfn A function to execute for each element in the ``Sequence``. Its
   * return value is added as a single element in the new array.
   * @param thisArg A value to use as ``this`` when executing ``callbackFn``.
   */
  map<U>(
    callbackfn: (elt: any, index: number, array: any) => U,
    thisArg?: any,
  ): U[] {
    // @ts-ignore
    return Array.prototype.map.call(this, callbackfn, thisArg);
  }
  /**
   * See :js:meth:`Array.filter`. Creates a shallow copy of a portion of a given
   * ``Sequence``, filtered down to just the elements from the given array that pass
   * the test implemented by the provided function.
   * @param predicate A function to execute for each element in the array. It
   * should return a truthy value to keep the element in the resulting array,
   * and a falsy value otherwise.
   * @param thisArg A value to use as ``this`` when executing ``predicate``.
   */
  filter(
    predicate: (elt: any, index: number, array: any) => boolean,
    thisArg?: any,
  ) {
    return Array.prototype.filter.call(this, predicate, thisArg);
  }
  /**
   * See :js:meth:`Array.some`. Tests whether at least one element in the
   * ``Sequence`` passes the test implemented by the provided function.
   * @param predicate A function to execute for each element in the
   * ``Sequence``. It should return a truthy value to indicate the element
   * passes the test, and a falsy value otherwise.
   * @param thisArg A value to use as ``this`` when executing ``predicate``.
   */
  some(
    predicate: (value: any, index: number, array: any[]) => unknown,
    thisArg?: any,
  ): boolean {
    return Array.prototype.some.call(this, predicate, thisArg);
  }
  /**
   * See :js:meth:`Array.every`. Tests whether every element in the ``Sequence``
   * passes the test implemented by the provided function.
   * @param predicate A function to execute for each element in the
   * ``Sequence``. It should return a truthy value to indicate the element
   * passes the test, and a falsy value otherwise.
   * @param thisArg A value to use as ``this`` when executing ``predicate``.
   */
  every(
    predicate: (value: any, index: number, array: any[]) => unknown,
    thisArg?: any,
  ): boolean {
    return Array.prototype.every.call(this, predicate, thisArg);
  }
  /**
   * See :js:meth:`Array.reduce`. Executes a user-supplied "reducer" callback
   * function on each element of the Sequence, in order, passing in the return
   * value from the calculation on the preceding element. The final result of
   * running the reducer across all elements of the Sequence is a single value.
   * @param callbackfn A function to execute for each element in the ``Sequence``. Its
   * return value is discarded.
   */
  reduce(
    callbackfn: (
      previousValue: any,
      currentValue: any,
      currentIndex: number,
      array: any,
    ) => any,
    initialValue?: any,
  ): any;
  reduce(...args: any[]) {
    // @ts-ignore
    return Array.prototype.reduce.apply(this, args);
  }
  /**
   * See :js:meth:`Array.reduceRight`. Applies a function against an accumulator
   * and each value of the Sequence (from right to left) to reduce it to a
   * single value.
   * @param callbackfn A function to execute for each element in the Sequence.
   * Its return value is discarded.
   */
  reduceRight(
    callbackfn: (
      previousValue: any,
      currentValue: any,
      currentIndex: number,
      array: any,
    ) => any,
    initialValue: any,
  ): any;
  reduceRight(...args: any[]) {
    // @ts-ignore
    return Array.prototype.reduceRight.apply(this, args);
  }
  /**
   * See :js:meth:`Array.at`. Takes an integer value and returns the item at
   * that index.
   * @param index Zero-based index of the Sequence element to be returned,
   * converted to an integer. Negative index counts back from the end of the
   * Sequence.
   * @returns The element in the Sequence matching the given index.
   */
  at(index: number) {
    return Array.prototype.at.call(this, index);
  }
  /**
   * The :js:meth:`Array.concat` method is used to merge two or more arrays.
   * This method does not change the existing arrays, but instead returns a new
   * array.
   * @param rest Arrays and/or values to concatenate into a new array.
   * @returns A new Array instance.
   */
  concat(...rest: ConcatArray<any>[]) {
    return Array.prototype.concat.apply(this, rest);
  }
  /**
   * The  :js:meth:`Array.includes` method determines whether a Sequence
   * includes a certain value among its entries, returning true or false as
   * appropriate.
   * @param elt
   * @returns
   */
  includes(elt: any) {
    // @ts-ignore
    return this.has(elt);
  }
  /**
   * The :js:meth:`Array.entries` method returns a new iterator object that
   * contains the key/value pairs for each index in the ``Sequence``.
   * @returns A new iterator object.
   */
  entries(): IterableIterator<[number, any]> {
    return Array.prototype.entries.call(this);
  }
  /**
   * The :js:meth:`Array.keys` method returns a new iterator object that
   * contains the keys for each index in the ``Sequence``.
   * @returns A new iterator object.
   */
  keys(): IterableIterator<number> {
    return Array.prototype.keys.call(this);
  }
  /**
   * The :js:meth:`Array.values` method returns a new iterator object that
   * contains the values for each index in the ``Sequence``.
   * @returns A new iterator object.
   */
  values(): IterableIterator<any> {
    return Array.prototype.values.call(this);
  }
  /**
   * The :js:meth:`Array.find` method returns the first element in the provided
   * array that satisfies the provided testing function.
   * @param predicate A function to execute for each element in the
   * ``Sequence``. It should return a truthy value to indicate a matching
   * element has been found, and a falsy value otherwise.
   * @param thisArg A value to use as ``this`` when executing ``predicate``.
   * @returns The first element in the ``Sequence`` that satisfies the provided
   * testing function.
   */
  find(
    predicate: (value: any, index: number, obj: any[]) => any,
    thisArg?: any,
  ) {
    return Array.prototype.find.call(this, predicate, thisArg);
  }
  /**
   * The :js:meth:`Array.findIndex` method returns the index of the first
   * element in the provided array that satisfies the provided testing function.
   * @param predicate A function to execute for each element in the
   * ``Sequence``. It should return a truthy value to indicate a matching
   * element has been found, and a falsy value otherwise.
   * @param thisArg A value to use as ``this`` when executing ``predicate``.
   * @returns The index of the first element in the ``Sequence`` that satisfies
   * the provided testing function.
   */
  findIndex(
    predicate: (value: any, index: number, obj: any[]) => any,
    thisArg?: any,
  ): number {
    return Array.prototype.findIndex.call(this, predicate, thisArg);
  }

  toJSON(this: any) {
    return Array.from(this);
  }
}


/**
 * A :js:class:`~pyodide.ffi.PyProxy` whose proxied Python object is an
 * :py:class:`~collections.abc.MutableSequence` (i.e., a :py:class:`list`)
 */
class PyMutableSequence extends PyProxy {
  /** @private */
  static [Symbol.hasInstance](obj: any): obj is PyProxy {
    return API.isPyProxy(obj) && !!(_getFlags(obj) & IS_SEQUENCE);
  }
}

interface PyMutableSequence extends PyMutableSequenceMethods {}


// JS default comparison is to convert to strings and compare lexicographically
function defaultCompareFunc(a: any, b: any): number {
  const astr = a.toString();
  const bstr = b.toString();
  if (astr === bstr) {
    return 0;
  }
  if (astr < bstr) {
    return -1;
  }
  return 1;
}



// function python_slice_assign(
//   jsobj: any,
//   start: number,
//   stop: number,
//   val: any,
// ): any[] {
//   let ptrobj = _getPtr(jsobj);
//   let res;
//   try {
//     Py_ENTER();
//     res = __pyproxy_slice_assign(ptrobj, start, stop, val);
//     Py_EXIT();
//   } catch (e) {
//     API.fatal_error(e);
//   }
//   if (res === Module.error) {
//     _pythonexc2js();
//   }
//   return res;
// }

function python_pop(jsobj: any, pop_start: boolean): any {
  let ptrobj = _getPtr(jsobj);
  let res;
  try {
    Py_ENTER();
    res = __pyproxy_pop(ptrobj, pop_start);
    Py_EXIT();
  } catch (e) {
    API.fatal_error(e);
  }
  if (res === Module.error) {
    _pythonexc2js();
  }
  return res;
}

class PyMutableSequenceMethods {
  /**
   * The :js:meth:`Array.reverse` method reverses a :js:class:`PyMutableSequence` in
   * place.
   * @returns A reference to the same :js:class:`PyMutableSequence`
   */
  reverse(): PyMutableSequence {
    // @ts-ignore
    this.$reverse();
    // @ts-ignore
    return this;
  }
  // TODO: make sort() work
  // /**
  //  * The :js:meth:`Array.sort` method sorts the elements of a
  //  * :js:class:`PyMutableSequence` in place.
  //  * @param compareFn A function that defines the sort order.
  //  * @returns A reference to the same :js:class:`PyMutableSequence`
  //  */
  // sort(compareFn?: (a: any, b: any) => number): PyMutableSequence {
  //   // Copy the behavior of sort described here:
  //   // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/sort#creating_displaying_and_sorting_an_array
  //   // Yes JS sort is weird.

  //   // We need this adaptor to convert from js comparison function to Python key
  //   // function.
  //   const functools = API.public_api.pyimport("functools");
  //   const cmp_to_key = functools.cmp_to_key;
  //   let cf: (a: any, b: any) => number;
  //   if (compareFn) {
  //     cf = compareFn;
  //   } else {
  //     cf = defaultCompareFunc;
  //   }
  //   // spec says arguments to compareFunc "Will never be undefined."
  //   // and undefined values should get sorted to end of list.
  //   // Make wrapper to ensure this
  //   function wrapper(a: any, b: any) {
  //     if (a === undefined && b === undefined) {
  //       return 0;
  //     }
  //     if (a === undefined) {
  //       return 1;
  //     }
  //     if (b === undefined) {
  //       return -1;
  //     }
  //     return cf(a, b);
  //   }
  //   let key;
  //   try {
  //     key = cmp_to_key(wrapper);
  //     // @ts-ignore
  //     this.$sort.callKwargs({ key });
  //   } finally {
  //     key?.destroy();
  //     cmp_to_key.destroy();
  //     functools.destroy();
  //   }
  //   // @ts-ignore
  //   return this;
  // }
  // TODO: make slice_assign work
  // /**
  //  * The :js:meth:`Array.splice` method changes the contents of a
  //  * :js:class:`PyMutableSequence` by removing or replacing existing elements and/or
  //  * adding new elements in place.
  //  * @param start Zero-based index at which to start changing the
  //  * :js:class:`PyMutableSequence`.
  //  * @param deleteCount An integer indicating the number of elements in the
  //  * :js:class:`PyMutableSequence` to remove from ``start``.
  //  * @param items The elements to add to the :js:class:`PyMutableSequence`, beginning from
  //  * ``start``.
  //  * @returns An array containing the deleted elements.
  //  */
  // splice(start: number, deleteCount?: number, ...items: any[]) {
  //   if (deleteCount === undefined) {
  //     // Max ssize
  //     deleteCount = 1 << (31 - 1);
  //   }
  //   return python_slice_assign(this, start, start + deleteCount, items);
  // }
  /**
   * The :js:meth:`Array.push` method adds the specified elements to the end of
   * a :js:class:`PyMutableSequence`.
   * @param elts The element(s) to add to the end of the :js:class:`PyMutableSequence`.
   * @returns The new length property of the object upon which the method was
   * called.
   */
  push(...elts: any[]) {
    for (let elt of elts) {
      // @ts-ignore
      this.append(elt);
    }
    // @ts-ignore
    return this.length;
  }
  /**
   * The :js:meth:`Array.pop` method removes the last element from a
   * :js:class:`PyMutableSequence`.
   * @returns The removed element from the :js:class:`PyMutableSequence`; undefined if the
   * :js:class:`PyMutableSequence` is empty.
   */
  pop() {
    return python_pop(this, false);
  }
  /**
   * The :js:meth:`Array.shift` method removes the first element from a
   * :js:class:`PyMutableSequence`.
   * @returns The removed element from the :js:class:`PyMutableSequence`; undefined if the
   * :js:class:`PyMutableSequence` is empty.
   */
  shift() {
    return python_pop(this, true);
  }
  /**
   * The :js:meth:`Array.unshift` method adds the specified elements to the
   * beginning of a :js:class:`PyMutableSequence`.
   * @param elts The elements to add to the front of the :js:class:`PyMutableSequence`.
   * @returns The new length of the :js:class:`PyMutableSequence`.
   */
  unshift(...elts: any[]) {
    elts.forEach((elt, idx) => {
      // @ts-ignore
      this.insert(idx, elt);
    });
    // @ts-ignore
    return this.length;
  }
  /**
   * The :js:meth:`Array.copyWithin` method shallow copies part of a
   * :js:class:`PyMutableSequence` to another location in the same :js:class:`PyMutableSequence`
   * without modifying its length.
   * @param target Zero-based index at which to copy the sequence to.
   * @param start Zero-based index at which to start copying elements from.
   * @param end Zero-based index at which to end copying elements from.
   * @returns The modified :js:class:`PyMutableSequence`.
   */
  copyWithin(target: number, start?: number, end?: number): any;
  copyWithin(...args: number[]): any {
    // @ts-ignore
    Array.prototype.copyWithin.apply(this, args);
    return this;
  }
  /**
   * The :js:meth:`Array.fill` method changes all elements in an array to a
   * static value, from a start index to an end index.
   * @param value Value to fill the array with.
   * @param start Zero-based index at which to start filling. Default 0.
   * @param end Zero-based index at which to end filling. Default
   * ``list.length``.
   * @returns
   */
  fill(value: any, start?: number, end?: number): any;
  fill(...args: any[]): any {
    // @ts-ignore
    Array.prototype.fill.apply(this, args);
    return this;
  }
}
