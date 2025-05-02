#include <emscripten.h>

/**
 * EM_JS Wrappers
 * Wrap EM_JS so that it produces functions that follow the Python return
 * conventions. We catch javascript errors and proxy them and use
 * `PyErr_SetObject` to hand them off to python. We need two variants, one
 * for functions that return pointers / references (return 0)
 * the other for functions that return numbers (return -1).
 *
 * WARNING: These wrappers around EM_JS cause macros in body to be expanded,
 * where this would be prevented by the ordinary EM_JS macro.
 * This causes trouble with true and false.
 * In types.h we provide nonstandard definitions:
 * false ==> (!!0)
 * true ==> (!!1)
 * These work as expected in both C and javascript.
 *
 * Note: this change in expansion behavior is unavoidable unless we copy the
 * definition of macro EM_JS into our code due to limitations of the C macro
 * engine. It is useful to be able to use macros in the EM_JS, but it might lead
 * to some unpleasant surprises down the road...
 */

// clang-format off
#ifdef DEBUG_F
// Yes, the "do {} while(0)" trick solves the same problem in the same way in
// javascript!
#define LOG_EM_JS_ERROR(__funcname__, err)                                              \
  do {                                                                                  \
    console.error(                                                                      \
      `EM_JS raised exception on line __LINE__ in func __funcname__ in file __FILE__`); \
    console.error("Error was:", err);                                                   \
  } while (0)
#else
#define LOG_EM_JS_ERROR(__funcname__, err)
#endif

// Need an extra layer to expand LOG_EM_JS_ERROR.
#define EM_JS_MACROS(ret, func_name, args, body...)                            \
  EM_JS(ret, func_name, args, body)

#define WARN_UNUSED __attribute__((warn_unused_result))

#define EM_JS_REF(ret, func_name, args, body...)                               \
  EM_JS_MACROS(ret WARN_UNUSED, func_name, args, {                             \
    try    /* intentionally no braces, body already has them */                \
      body /* <== body of func */                                              \
    catch (e) {                                                                \
        LOG_EM_JS_ERROR(func_name, e);                                         \
        Module.handle_js_error(e);                                             \
        return 0;                                                              \
    }                                                                          \
    errNoRet();                                                                \
  })

#define EM_JS_VAL(ret, func_name, args, body...)                               \
  EM_JS_MACROS(ret WARN_UNUSED, func_name, args, {                             \
    try    /* intentionally no braces, body already has them */                \
      body /* <== body of func */                                              \
    catch (e) {                                                                \
        LOG_EM_JS_ERROR(func_name, e);                                         \
        Module.handle_js_error(e);                                             \
        return null;                                                           \
    }                                                                          \
    errNoRet();                                                                \
  })

#define EM_JS_NUM(ret, func_name, args, body...)                               \
  EM_JS_MACROS(ret WARN_UNUSED, func_name, args, {                             \
    try    /* intentionally no braces, body already has them */                \
      body /* <== body of func */                                              \
    catch (e) {                                                                \
        LOG_EM_JS_ERROR(func_name, e);                                         \
        Module.handle_js_error(e);                                             \
        return -1;                                                             \
    }                                                                          \
    return 0;  /* some of these were void */                                   \
  })
