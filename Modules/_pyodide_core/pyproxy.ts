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
declare function _pyproxy_getflags(ptr: number): number;
declare function __pyproxy_apply(
  ptr: number,
  jsargs: any[],
  num_pos_args: number,
  kwargs_names: string[],
  num_kwargs: number,
): any;

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
declare var HAS_GET: number;
declare var HAS_HAS: number;
declare var HAS_INCLUDES: number;
declare var HAS_LENGTH: number;
declare var HAS_SET: number;
declare var IS_ARRAY: number;
declare var IS_CALLABLE: number;
declare var IS_ERROR: number;
declare var IS_GENERATOR: number;
declare var IS_ITERABLE: number;
declare var IS_ITERATOR: number;

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

type PyProxyShared = {
  ptr: number;
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
    gcRegister,
  }: {
    flags?: number;
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
    shared = {
      ptr,
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
  handlers = PyProxyHandlers;
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

  const FLAG_TYPE_PAIRS: [number, any][] = [[IS_CALLABLE, PyCallableMethods]];
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

function pyproxy_destroy(proxy: PyProxy, destroyed_msg: string) {
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
}
Module.pyproxy_destroy = pyproxy_destroy;

const PyProxyHandlers = {
  apply(jsobj: PyProxy & Function, jsthis: any, jsargs: any): any {
    return jsobj.apply(jsthis, jsargs);
  },
};

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
