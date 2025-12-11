js_flags = {}
from functools import reduce
from abc import ABCMeta
from collections.abc import (
    Generator,
    Iterator,
    Iterable,
    Mapping,
    MutableMapping,
    MutableSequence,
)

def _binor_reduce(l) -> int:
    return reduce(lambda x, y: x | y, l)


def _process_flag_expression(e: str) -> int:
    return _binor_reduce(js_flags[x.strip()] for x in e.split("|"))

class _JsProxyMetaClass(type):
    def __instancecheck__(cls, instance):
        return cls.__subclasscheck__(type(instance))

    def __subclasscheck__(cls, subclass):
        if type.__subclasscheck__(cls, subclass):
            return True
        if not hasattr(subclass, "_js_type_flags"):
            return False
        # For the "synthetic" subtypes defined in this file, we define
        # _js_type_flags as a string. We look these up in the _js_flags dict to
        # convert to a number.
        cls_flags = cls._js_type_flags  # type:ignore[attr-defined]
        if isinstance(cls_flags, int):
            cls_flags = [cls_flags]
        else:
            cls_flags = [_process_flag_expression(f) for f in cls_flags]

        subclass_flags = subclass._js_type_flags
        if not isinstance(subclass_flags, int):
            subclass_flags = _binor_reduce(js_flags[f] for f in subclass_flags)

        return any(cls_flag & subclass_flags == cls_flag for cls_flag in cls_flags)

class _ABCMeta(_JsProxyMetaClass, ABCMeta):
    pass

class JsProxy(metaclass=_JsProxyMetaClass):
    """A proxy to make a JavaScript object behave like a Python object"""

    _js_type_flags = 0

    def __new__(cls, arg=None, *args, **kwargs):
        raise TypeError(f"{cls.__name__} cannot be instantiated.")

class JsDoubleProxy(JsProxy):
    """A double proxy created with :py:func:`create_proxy`."""

    _js_type_flags = ["IS_DOUBLE_PROXY"]


class JsIterator(JsProxy, Iterator, metaclass=_ABCMeta):
    """A JsProxy of a JavaScript iterator.

    An object is a :py:class:`JsAsyncIterator` if it has a :js:meth:`~Iterator.next` method and either has a
    :js:data:`Symbol.iterator` or has no :js:data:`Symbol.asyncIterator`.
    """

    _js_type_flags = ["IS_ITERATOR"]

class JsIterable(JsProxy, Iterable, metaclass=_ABCMeta):
    """A JavaScript iterable object

    A JavaScript object is iterable if it has a :js:data:`Symbol.iterator` method.
    """

    _js_type_flags = ["IS_ITERABLE"]

class JsGenerator(JsIterable, Generator, metaclass=_ABCMeta):
    """A JavaScript generator

    A JavaScript object is treated as a generator if its
    Symbol.toStringTag is "Generator". Most likely this will be
    because it is a true Generator produced by the JavaScript
    runtime, but it may be a custom object trying hard to pretend to be a
    generator. It should have Generator.next,
    Generator.return and Generator.throw methods.
    """

    _js_type_flags = ["IS_GENERATOR", "IS_ITERATOR"]

class JsCallable(JsProxy):
    """A JavaScript callable

    A JavaScript object is a callable if typeof x returns
    "function".
    """

    _js_type_flags = ["IS_CALLABLE"]

class JsArray(JsIterable, MutableSequence, metaclass=_ABCMeta):
    """A JsProxy of an Array"""

    _js_type_flags = ["IS_ARRAY"]

class JsMap(JsIterable, Mapping, metaclass=_ABCMeta):
    """A JavaScript Map

    To be considered a map, a JavaScript object must have a get method, it
    must have a size or a length property which is a number
    (idiomatically it should be called size) and it must be iterable.
    """

    _js_type_flags = [
        "HAS_GET | HAS_LENGTH | IS_ITERABLE",
        "IS_PY_JSON_DICT",
    ]

class JsMutableMap(
    JsMap, MutableMapping, metaclass=_ABCMeta
):
    """A JavaScript mutable map

    To be considered a mutable map, a JavaScript object must have a get
    method, a has method, a size or a length property which is a
    number (idiomatically it should be called size) and it must be iterable.

    Instances of the JavaScript builtin Map class are JsMutableMap s.
    Also proxies returned by JsProxy.as_object_map are instances of
    JsMap .
    """

    _js_type_flags = [
        "HAS_GET | HAS_SET | HAS_LENGTH | IS_ITERABLE",
        "IS_PY_JSON_DICT",
    ]


class JsNull:
    """The type of the Python representation of the JavaScript null object"""

    def __new__(cls):
        return jsnull

    def __repr__(self):
        return "jsnull"

    def __bool__(self):
        return False

    typeof = "object"


#: The Python representation of the JavaScript null object.
jsnull: JsNull = object.__new__(JsNull)


def _int_to_bigint(x):
    if isinstance(x, int):
        return JsBigInt(x)
    return x


class JsBigInt(int):
    """The Python representation of a bigint.

    This is a subclass of ``int`` that behaves in all ways like a normal int
    except that it is converted to a bigint instead of a number when converted
    to JavaScript.

    All standard binary and unary operations are supported. When used with
    ``+``, ``-``, ``*``, ``&``, ``|``, or ``^`` a normal ``int`` is returned if
    either operand is an int. If both operands are a ``JsBigInt`` then a
    ``JsBigInt`` will be returned.

    When used with ``//``, ``<<``, ``>>``, ``**``, or ``%``, the type of the
    return value depends only on the left operand, so if the left operand is a
    ``JsBigInt``, a ``JsBigInt`` is returned whereas if the left operand is an
    ``int`` an ``int`` is returned.
    """

    def __abs__(self) -> "JsBigInt":
        return JsBigInt(int.__abs__(self))

    def __add__(self, other: int) -> int:
        return _int_to_bigint(int.__add__(self, other))

    def __and__(self, other: int) -> int:
        return _int_to_bigint(int.__and__(self, other))

    def __floordiv__(self, other: int) -> "JsBigInt":
        return _int_to_bigint(int.__floordiv__(self, other))

    def __invert__(self) -> "JsBigInt":
        return JsBigInt(int.__invert__(self))

    def __lshift__(self, other: int) -> "JsBigInt":
        return _int_to_bigint(int.__lshift__(self, other))

    def __mod__(self, other: int) -> "JsBigInt":
        return _int_to_bigint(int.__mod__(self, other))

    def __neg__(self) -> "JsBigInt":
        return JsBigInt(int.__neg__(self))

    def __or__(self, other: int) -> int:
        return _int_to_bigint(int.__or__(self, other))

    def __pow__(self, other: int, modulus: int | None = None) -> "JsBigInt":  # type: ignore[override]
        return _int_to_bigint(int.__pow__(self, other, modulus))

    def __pos__(self) -> "JsBigInt":
        return JsBigInt(int.__pos__(self))

    def __rshift__(self, other: int) -> "JsBigInt":
        return _int_to_bigint(int.__rshift__(self, other))

    def __sub__(self, other: int) -> int:
        return _int_to_bigint(int.__sub__(self, other))

    def __xor__(self, other: int) -> int:
        return _int_to_bigint(int.__xor__(self, other))
