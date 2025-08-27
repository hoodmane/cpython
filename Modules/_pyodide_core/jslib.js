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

const hasProperty = (obj, prop)  => {
  try {
    while (obj) {
      if (Object.getOwnPropertyDescriptor(obj, prop)) {
        return true;
      }
      obj = Object.getPrototypeOf(obj);
    }
  } catch (e) {}
  return false;
}

function hexStringToUTF8Array(hex) {
  const bytes = [];
  for (let i = 0; i < hex.length; i += 2) {
    bytes.push(parseInt(hex.substr(i, 2), 16));
  }
  return new Uint8Array(bytes);
}
