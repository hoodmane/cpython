JSFILE(() => {
  0, 0;
  const PropagateError = Module.PropagateError;

  function js2python_string(value) {
    // The general idea here is to allocate a Python string and then
    // have JavaScript write directly into its buffer.  We first need
    // to determine if is needs to be a 1-, 2- or 4-byte string, since
    // Python handles all 3.
    let max_code_point = 0;
    let num_code_points = 0;
    for (let c of value) {
      num_code_points++;
      let code_point = c.codePointAt(0);
      max_code_point =
        code_point > max_code_point ? code_point : max_code_point;
    }

    let result = _PyUnicode_New(num_code_points, max_code_point);
    if (result === 0) {
      throw new PropagateError();
    }

    let ptr = _PyUnicode_Data(result);
    if (max_code_point > 0xffff) {
      for (let c of value) {
        ASSIGN_U32(ptr, 0, c.codePointAt(0));
        ptr += 4;
      }
    } else if (max_code_point > 0xff) {
      for (let c of value) {
        ASSIGN_U16(ptr, 0, c.codePointAt(0));
        ptr += 2;
      }
    } else {
      for (let c of value) {
        ASSIGN_U8(ptr, 0, c.codePointAt(0));
        ptr += 1;
      }
    }

    return result;
  }

  function js2python_bigint(value) {
    let value_orig = value;
    let length = 0;
    if (value < 0) {
      value = -value;
    }
    value <<= BigInt(1);
    while (value) {
      length++;
      value >>= BigInt(32);
    }
    return withStackSave(() => {
        let ptr = stackAlloc(length * 4);
        value = value_orig;
        for (let i = 0; i < length; i++) {
            ASSIGN_U32(ptr, i, Number(value & BigInt(0xffffffff)));
            value >>= BigInt(32);
        }
        return __PyLong_FromByteArray(
            ptr,
            length * 4 /* length in bytes */,
            true /* little endian */,
            true /* signed? */,
        );
    });
  }

  /**
   * This function converts immutable types. numbers, bigints, strings,
   * booleans, undefined, and null are converted. PyProxies are unwrapped.
   *
   * If `value` is of any other type then `undefined` is returned.
   *
   * If `value` is one of those types but an error is raised during conversion,
   * we throw a PropagateError to propagate the error out to C. This causes
   * special handling in the EM_JS wrapper.
   */
  function js2python_convertImmutable(value) {
    let result = js2python_convertImmutableInner(value);
    if (result === 0) {
      throw new PropagateError();
    }
    return result;
  }
  // js2python_convertImmutable is used from js2python.c so we need to add it
  // to Module.
  Module.js2python_convertImmutable = js2python_convertImmutable;

  /**
   * Returns a pointer to a Python object, 0, or undefined.
   *
   * If we return 0 it means we tried to convert but an error occurred, if we
   * return undefined, no conversion was attempted.
   */
  function js2python_convertImmutableInner(value) {
    let type = typeof value;
    if (type === "string") {
      return js2python_string(value);
    } else if (type === "number") {
      if (Number.isSafeInteger(value)) {
        return _PyLong_FromDouble(value);
      } else {
        return _PyFloat_FromDouble(value);
      }
    } else if (type === "bigint") {
      return js2python_bigint(value);
    } else if (value === undefined || value === null) {
      return __js2python_none();
    } else if (value === true) {
      return __js2python_true();
    } else if (value === false) {
      return __js2python_false();
    }
    return undefined;
  }
});
