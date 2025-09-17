const API = {};
Module.API = API;

const getTypeTag = (x) => {
  try {
    return Object.prototype.toString.call(x);
  } catch (e) {
    return "";
  }
};

/**
 * Observe whether a method exists or not
 *
 * Invokes getters but catches any error produced by a getter and throws it away.
 * Never throws an error
 *
 * obj: an object
 * prop: a string or symbol
 */
const hasMethod = (obj, prop) => {
  try {
    return typeof obj[prop] === "function";
  } catch (e) {
    return false;
  }
};

const hasProperty = (obj, prop) => {
  try {
    while (obj) {
      if (Object.getOwnPropertyDescriptor(obj, prop)) {
        return true;
      }
      obj = Object.getPrototypeOf(obj);
    }
  } catch (e) {}
  return false;
};

function hexStringToUTF8Array(hex) {
  const bytes = [];
  for (let i = 0; i < hex.length; i += 2) {
    bytes.push(parseInt(hex.substr(i, 2), 16));
  }
  return new Uint8Array(bytes);
}

class PythonError extends Error {
  /**
   * The address of the error we are wrapping. We may later compare this
   * against sys.last_exc.
   * WARNING: we don't own a reference to this pointer, dereferencing it
   * may be a use-after-free error!
   * @private
   */
  __error_address;
  /**
   * The name of the Python error class, e.g, :py:exc:`RuntimeError` or
   * :py:exc:`KeyError`.
   */
  type;
  constructor(type, message, error_address) {
    super(message);
    this.type = type;
    this.__error_address = error_address;
  }
}

API.fatal_error = function (e) {
  console.log("Fatal error", e);
  throw e;
};


const { get, getOwnPropertyDescriptor, ownKeys } = Reflect;

const getPropertyDescriptor = (value) => ({
  value,
  enumerable: true,
  writable: true,
  configurable: true,
});

const _ = Symbol();
const prototype = "prototype";

const handler = {
  deleteProperty: (map, k) => (map.has(k) ? map.delete(k) : delete map[k]),
  get(map, k, proxy) {
    if (k === _) return map;
    let v = map[k];
    if (typeof v === "function" && k !== "constructor") {
      v = v.bind(map);
    }
    v ||= map.get(k);
    return v;
  },
  getOwnPropertyDescriptor(map, k) {
    if (map.has(k)) return getPropertyDescriptor(map.get(k));
    if (k in map) return getOwnPropertyDescriptor(map, k);
  },
  has: (map, k) => map.has(k) || k in map,
  ownKeys: (map) =>
    [...map.keys(), ...ownKeys(map)].filter((x) =>
      ["string", "symbol"].includes(typeof x),
    ),
  set: (map, k, v) => (map.set(k, v), true),
};

const LiteralMap = new Proxy(
  class LiteralMap extends Map {
    constructor(...args) {
      return new Proxy(super(...args), handler);
    }
  },
  {
    get(Class, k, ...rest) {
      return k !== prototype && k in Class[prototype]
        ? (proxy, ...args) => {
            const map = proxy[_];
            let value = map[k];
            if (typeof value === "function") value = value.apply(map, args);
            // prevent leaking the internal map elsewhere
            return value === map ? proxy : value;
          }
        : get(Class, k, ...rest);
    },
  },
);

API.LiteralMap = LiteralMap;

const PyProxy_IsAlive = (px) => !!Module.PyProxy_getAttrsQuiet(px).shared.ptr;
