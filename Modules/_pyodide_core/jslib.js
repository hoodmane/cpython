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
