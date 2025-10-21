from _pyodide import jsnull
from _pyodide_core import to_js, destroy_proxies, create_proxy, run_js as _run_js

JsProxy = type(_run_js("({})"))
JsError = type(_run_js("new Error()"))


__all__ = ["jsnull", "create_proxy", "destroy_proxies", "to_js", "JsProxy", "JsError"]
