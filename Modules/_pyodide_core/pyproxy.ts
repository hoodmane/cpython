declare var Tests: any;
declare var Module: any;
declare var API: {
  fatal_error: (e: any) => never;
  [k: string]: any;
};

declare function _Py_IncRef(ptr: number): void;
declare function _Py_DecRef(ptr: number): void;
declare function _pythonexc2js(): never;
declare function __pyproxy_type(ptr: number): string;
declare function __pyproxy_str(ptr: number): string;

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
declare var IS_CALLABLE: number;
declare var HAS_LENGTH: number;
declare var HAS_GET: number;
declare var HAS_SET: number;
declare var HAS_CONTAINS: number;
declare var IS_ITERABLE: number;
declare var IS_ITERATOR: number;
declare var IS_AWAITABLE: number;
declare var IS_BUFFER: number;
declare var IS_ASYNC_ITERABLE: number;
declare var IS_ASYNC_ITERATOR: number;
declare var IS_GENERATOR: number;
declare var IS_ASYNC_GENERATOR: number;
declare var IS_SEQUENCE: number;
declare var IS_MUTABLE_SEQUENCE: number;
declare var IS_JSON_ADAPTOR_DICT: number;
declare var IS_JSON_ADAPTOR_SEQUENCE: number;

declare function DEREF_U32(ptr: number, offset: number): number;
declare function Py_ENTER(): void;
declare function Py_EXIT(): void;
// end-pyodide-skip

if (globalThis.FinalizationRegistry) {
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
  // For some unclear reason this code screws up selenium FirefoxDriver. Works
  // fine in chrome and when I test it in browser. It seems to be sensitive to
  // changes that don't make a difference to the semantics.
  // TODO: after 0.18.0, fix selenium issues with this code.
  // Module.bufferFinalizationRegistry = new FinalizationRegistry((ptr) => {
  //   try {
  //     _PyBuffer_Release(ptr);
  //     _PyMem_Free(ptr);
  //   } catch (e) {
  //     API.fatal_error(e);
  //   }
  // });
} else {
  Module.finalizationRegistry = { register() {}, unregister() {} };
  // Module.bufferFinalizationRegistry = finalizationRegistry;
}

function isPyProxy(jsobj: any): jsobj is PyProxy {
  try {
    return jsobj instanceof PyProxy;
  } catch (e) {
    return false;
  }
}
API.isPyProxy = isPyProxy;


type PyProxyShared = {
  ptr: number;
  destroyed_msg: string | undefined;
  gcRegistered: boolean;
};

type PyProxyAttrs = {
  // shared between aliases but not between copies
  shared: PyProxyShared;
};

const pyproxyAttrsSymbol = Symbol("pyproxy.attrs");

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
    props,
    shared,
    gcRegister,
  }: {
    shared?: PyProxyShared;
    props?: any;
    gcRegister?: boolean;
  } = {},
): PyProxy {
  const cls = PyProxy;
  let target: any;
  target = Object.create(cls.prototype);

  const isAlias = !!shared;
  if (!shared) {
    // Not an alias so we have to make `shared`.
    shared = {
      ptr,
      destroyed_msg: undefined,
      gcRegistered: false,
    };
    _Py_IncRef(ptr);
  }

  props = Object.assign(
    { isBound: false, captureThis: false, boundArgs: [], roundtrip: false },
    props,
  );
  let handlers;
  handlers = PyProxyHandlers;
  let proxy = new Proxy(target, handlers);
  if (!isAlias && gcRegister) {
    // we need to register only once for a set of aliases. we can't register the
    // proxy directly since that isn't shared between aliases. The aliases all
    // share $$ so we can register that. They also need access to the data in
    // $$, but we can't use $$ itself as the held object since that would keep
    // $$ from being gc'd ever. So we make a copy. To prevent double free, we
    // have to be careful to unregister when we destroy.
    gc_register_proxy(shared);
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
Module.PyProxy_getAttrs = _getAttrs;

function _getPtr(jsobj: any) {
  return _getAttrs(jsobj).shared.ptr;
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
  destroy(options: { message?: string; } = {}) {
    options = Object.assign({ message: "" }, options);
    const { message: m } = options;
    Module.pyproxy_destroy(this, m);
  }
}


function pyproxy_destroy(
  proxy: PyProxy,
  destroyed_msg: string,
) {
  const { shared } = _getAttrsQuiet(proxy);
  if (!shared.ptr) {
    // already destroyed
    return;
  }
  shared.destroyed_msg = destroyed_msg;
  // Maybe the destructor will call JavaScript code that will somehow try
  // to use this proxy. Mark it deleted before decrementing reference count
  // just in case!
  const ptr = shared.ptr;
  shared.ptr = 0;
  if (shared.gcRegistered) {
    Module.finalizationRegistry.unregister(shared);
  }

  try {
    Py_ENTER();
    _Py_DecRef(ptr);
    Py_EXIT();
  } catch (e) {
    API.fatal_error(e);
  }
};
Module.pyproxy_destroy = pyproxy_destroy;


const PyProxyHandlers = {};
