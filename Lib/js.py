from pyodide.code import run_js
from pyodide.ffi import JsProxy
from importlib.util import module_from_spec, spec_from_loader
import sys


class JsLoader:
    def __init__(self, jsproxy):
        self.jsproxy = jsproxy

    def create_module(self, spec):
        return self.jsproxy

    def exec_module(self, module):
        pass

    def is_package(self, fullname):
        return True


class JsFinder:
    """Resolve ``js.a.b`` by looking up ``b`` on the ``js.a`` JsProxy."""

    @staticmethod
    def find_spec(fullname, path, target=None):
        parent, _, child = fullname.rpartition(".")
        if not parent:
            return None
        parent_module = sys.modules.get(parent)
        if not isinstance(parent_module, JsProxy):
            return None
        try:
            jsproxy = getattr(parent_module, child)
        except AttributeError:
            return None
        if not isinstance(jsproxy, JsProxy):
            raise ModuleNotFoundError(
                f"No module named {fullname!r}: {fullname!r} is not a JS object",
                name=fullname,
            )
        return spec_from_loader(fullname, JsLoader(jsproxy))


globalThis = run_js("globalThis")

sys.modules["js"] = module_from_spec(spec_from_loader("js", JsLoader(globalThis)))
sys.meta_path.append(JsFinder)
