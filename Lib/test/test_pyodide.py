from unittest import TestCase
from _pyodide_core import run_js, to_js, destroy_proxies
from _pyodide import jsnull

JsError = type(run_js("new Error()"))
Array = run_js("Array")


class ConversionTest(TestCase):
    def test_run_js(self):
        self.assertEqual(run_js("3 + 4"), 7)
        with self.assertRaisesRegex(JsError, "Error: Hi!"):
            run_js("throw new Error('Hi!')")

    def test_jsproxy_call(self):
        res = run_js("(x, y) => x + y")(1, 9)
        self.assertEqual(res, 10)

    def test_python2js(self):
        self.assertTrue(run_js("(x) => x === 7")(7))
        self.assertTrue(run_js("(x) => x === 2.3")(2.3))
        self.assertTrue(
            run_js("(x) => x === 77015781075109876017131518n")(
                77015781075109876017131518
            )
        )
        self.assertTrue(run_js("(x) => x === 'abc'")("abc"))
        self.assertTrue(run_js("(x) => x === undefined")(None))
        self.assertTrue(run_js("(x) => x === null")(jsnull))
        self.assertTrue(run_js("(x) => x === false")(False))
        self.assertTrue(run_js("(x) => x === true")(True))
        self.assertTrue(run_js("(x) => x === 'pyodidé'")("pyodidé"))
        self.assertTrue(run_js("(x) => x === '碘化物'")("碘化物"))
        self.assertTrue(run_js("(x) => x === '🐍'")("🐍"))

    def test_python2js_integers(self):
        x = 77015781075109876017131518
        js_str = run_js(f"(x) => x.toString()")
        js_typeof = run_js(f"(x) => typeof x")
        for _ in range(33):
            self.assertEqual(js_str(x), str(x))
            self.assertEqual(js_str(-x), str(-x))
            self.assertEqual(js_typeof(x), "bigint")
            self.assertEqual(js_typeof(-x), "bigint")
            x >>= 1
        for _ in range(32):
            self.assertEqual(js_str(x), str(x))
            self.assertEqual(js_str(-x), str(-x))
            self.assertEqual(js_typeof(x), "number")
            self.assertEqual(js_typeof(-x), "number")
            x >>= 1

    def test_js2python(self):
        self.assertEqual(run_js('"pyodidé"'), "pyodidé")
        self.assertEqual(run_js('"碘化物"'), "碘化物")
        self.assertEqual(run_js('"🐍"'), "🐍")
        self.assertEqual(run_js("2.3"), 2.3)
        self.assertEqual(
            run_js("77015781075109876017131518n"), 77015781075109876017131518
        )
        self.assertEqual(run_js("undefined"), None)
        self.assertEqual(run_js("null"), jsnull)
        self.assertEqual(run_js("false"), False)
        self.assertEqual(run_js("true"), True)

    def test_js2python_integers(self):
        x = 77015781075109876017131518
        while x != 0:
            self.assertEqual(run_js(f"{x}n"), x)
            self.assertEqual(run_js(f"-{x}n"), -x)
            x >>= 1

    def test_raise_through_js(self):
        def f():
            1 / 0

        with self.assertRaises(ZeroDivisionError):
            run_js("((f) => f())")(f)

    def test_to_py1(self):
        a = run_js(
            """
            let a = new Map([[1, [1,2,new Set([1,2,3])]], [2, new Map([[1,2],[2,7]])]]);
            a.get(2).set("a", a);
            a;
            """
        )
        result = [repr(a.to_py(depth=i)) for i in range(4)]
        self.assertEqual(
            result,
            [
                "[object Map]",
                "{1: 1,2,[object Set], 2: [object Map]}",
                "{1: [1, 2, [object Set]], 2: {1: 2, 2: 7, 'a': [object Map]}}",
                "{1: [1, 2, {1, 2, 3}], 2: {1: 2, 2: 7, 'a': {...}}}",
            ],
        )

    def test_to_py2(self):
        a = run_js(
            """
            let a = { "x" : 2, "y" : 7, "z" : [1,2] };
            a.z.push(a);
            a
            """
        )
        result = [repr(a.to_py(depth=i)) for i in range(4)]
        self.assertEqual(
            result,
            [
                "[object Object]",
                "{'x': 2, 'y': 7, 'z': 1,2,[object Object]}",
                "{'x': 2, 'y': 7, 'z': [1, 2, [object Object]]}",
                "{'x': 2, 'y': 7, 'z': [1, 2, {...}]}",
            ],
        )

    def test_to_py3(self):
        a = run_js(
            """
            class Temp {
                constructor(){
                    this.x = 2;
                    this.y = 7;
                }
            }
            new Temp();
            """
        )
        assert repr(type(a.to_py())) == "<class 'pyodide.ffi.JsProxy'>"

    def test_to_py4(self):
        for obj, msg in [
            (
                "Map([[[1,1], 2]])",
                "Cannot use key of type Array as a key to a Python dict",
            ),
            (
                "Set([[1,1]])",
                "Cannot use key of type Array as a key to a Python set",
            ),
            ("Map([[0, 2], [false, 3]])", "contains both 0 and false"),
            ("Set([0, false])", "contains both 0 and false"),
            ("Map([[1, 2], [true, 3]])", "contains both 1 and true"),
            ("Set([1, true])", "contains both 1 and true"),
        ]:
            a = run_js(f"new {obj}")
            with self.assertRaisesRegex(JsError, msg):
                a.to_py()

    def test_to_py_default_converter(self):
        [p1, p2] = run_js(
            """
            class Pair {
                constructor(first, second){
                    this.first = first;
                    this.second = second;
                }
            }
            const l = [1,2,3];
            const r1 = new Pair(l, [l]);
            const r2 = new Pair(l, [l]);
            r2.first = r2;
            [r1, r2]
            """
        )

        def default_converter(value, converter, cache):
            if value.constructor.name != "Pair":
                return value
            l = []
            cache(value, l)
            l.append(converter(value.first))
            l.append(converter(value.second))
            return l

        r1 = p1.to_py(default_converter=default_converter)
        self.assertIsInstance(r1, list)
        self.assertIs(r1[0], r1[1][0])
        self.assertEqual(r1[0], [1, 2, 3])

        r2 = p2.to_py(default_converter=default_converter)
        self.assertIs(r2[0], r2)

    def test_to_js_default_converter(self):
        import json

        JSON = run_js("JSON")

        class Pair:
            __slots__ = ("first", "second")

            def __init__(self, first, second):
                self.first = first
                self.second = second

        p1 = Pair(1, 2)
        p2 = Pair(1, 2)
        p2.first = p2

        def default_converter(value, convert, cacheConversion):
            result = Array.new()
            cacheConversion(value, result)
            result.push(convert(value.first))
            result.push(convert(value.second))
            return result

        p1js = to_js(p1, default_converter=default_converter)
        p2js = to_js(p2, default_converter=default_converter)

        self.assertEqual(json.loads(JSON.stringify(p1js)), [1, 2])

        with self.assertRaisesRegex(JsError, "TypeError"):
            JSON.stringify(p2js)

        self.assertTrue(run_js("(x) => x[0] === x")(p2js))
        self.assertTrue(run_js("(x) => x[1] === 2")(p2js))

    def test_to_js_eager_converter(self):
        recursive_list = []
        recursive_list.append(recursive_list)

        recursive_dict = {}
        recursive_dict[0] = recursive_dict

        a_thing = [{1: 2}, (2, 4, 6)]

        def normal(value, convert, cacheConversion):
            return convert(value)

        def reject_tuples(value, convert, cacheConversion):
            if isinstance(value, tuple):
                raise ValueError("We don't convert tuples!")
            return convert(value)

        def proxy_tuples(value, convert, cacheConversion):
            if isinstance(value, tuple):
                return value
            return convert(value)

        to_js(recursive_list, eager_converter=normal)
        to_js(recursive_dict, eager_converter=normal)
        to_js(a_thing, eager_converter=normal)

        to_js(recursive_list, eager_converter=reject_tuples)
        to_js(recursive_dict, eager_converter=reject_tuples)
        with self.assertRaisesRegex(ValueError, "We don't convert tuples"):
            to_js(a_thing, eager_converter=reject_tuples)

        to_js(recursive_list, eager_converter=proxy_tuples)
        to_js(recursive_dict, eager_converter=proxy_tuples)
        proxylist = Array.new()
        res = to_js(a_thing, eager_converter=proxy_tuples, pyproxies=proxylist)
        assert res[-1] == (2, 4, 6)
        assert len(proxylist) == 1
        destroy_proxies(proxylist)



class JsProxyTest(TestCase):
    def test_jsproxy(self):
        o = run_js("[7, 11, -1]")
        self.assertEqual(repr(o), "7,11,-1")
        self.assertEqual(o.length, 3)

        o = run_js("globalThis.o = {a: 7, b: 92}; o")
        self.assertEqual(repr(o), "[object Object]")
        self.assertEqual(o.a, 7)
        self.assertEqual(o.b, 92)
        o.a = 13
        self.assertEqual(run_js("o.a"), 13)

    def test_jsproxy_attr_error(self):
        o = run_js("({x: undefined, y: null})")
        self.assertEqual(o.x, None)
        self.assertEqual(o.y, jsnull)
        with self.assertRaisesRegex(AttributeError, "xyz"):
            self.assertEqual(o.xyz, jsnull)

    def test_jsproxy_bool(self):
        self.assertTrue(run_js("new TextDecoder()"))
        self.assertFalse(run_js("[]"))
        self.assertTrue(run_js("[1]"))
        self.assertTrue(run_js("() => {}"))
        self.assertTrue(run_js("(function(){})"))
        self.assertFalse(run_js("new Map()"))
        self.assertTrue(run_js("new Map([[1, 7]])"))
        self.assertFalse(run_js("new Set()"))
        self.assertTrue(run_js("new Set([1, 7])"))

    def test_jsproxy_js_id(self):
        o = run_js("let a = {}; let b = {}; ({a, b, c: a})")
        a = o.a
        b = o.b
        c = o.c
        self.assertEqual(a.js_id, c.js_id)
        self.assertIsNot(a, c)
        self.assertNotEqual(a.js_id, b.js_id)

    def test_jsproxy_compare(self):
        o = run_js("let a = {}; let b = {}; ({a, b, c: a})")
        a = o.a
        b = o.b
        c = o.c
        self.assertEqual(a, c)
        self.assertIsNot(a, c)
        self.assertNotEqual(a, b)
        self.assertNotEqual(a, {})
        with self.assertRaisesRegex(TypeError, "not supported"):
            a < b
        with self.assertRaisesRegex(TypeError, "not supported"):
            a > b
        with self.assertRaisesRegex(TypeError, "not supported"):
            a <= b
        with self.assertRaisesRegex(TypeError, "not supported"):
            a >= b

    def test_jsproxy_to_js(self):
        o = run_js("({x: 3, y: 7})")
        f = run_js("(o) => o.x + o.y")
        self.assertEqual(f(o), 10)

    def test_jsproxy_construct(self):
        URL = run_js("URL")
        with self.assertRaises(JsError):
            URL("http://example.com")
        r = URL.new("http://example.com/a/b?c=2")
        self.assertEqual(r.origin, "http://example.com")
        self.assertEqual(r.protocol, "http:")
        self.assertEqual(r.pathname, "/a/b")
        self.assertEqual(r.search, "?c=2")

    def test_jsproxy_iter(self):
        l = run_js("[9, 32, 12, 17]")
        self.assertEqual(list(l), [9, 32, 12, 17])

    def test_jsproxy_dir(self):
        a = run_js('({ x : 2, y : "9" })')
        b = run_js("(function(){})")
        dira = set(dir(a))
        dirb = set(dir(b))

        jsproxy_items = {
            "__bool__",
            "__class__",
            "__defineGetter__",
            "__defineSetter__",
            "__delattr__",
            "constructor",
            "toString",
            "valueOf",
        }
        a_items = {"x", "y"}
        callable_items = {"__call__", "new"}
        self.assertTrue(dira.issuperset(jsproxy_items))
        self.assertTrue(dira.isdisjoint(callable_items))
        self.assertTrue(dira.issuperset(a_items))
        self.assertTrue(dirb.issuperset(jsproxy_items))
        self.assertTrue(dirb.issuperset(callable_items))
        self.assertTrue(dirb.isdisjoint(a_items))

    def test_jsproxy_error(self):
        def f():
            raise run_js("new TypeError('hi!')")

        try:
            f()
        except JsError as e:
            err = e
        else:
            self.fail()

        self.assertEqual(err.name, "TypeError")
        self.assertEqual(err.message, "hi!")
        self.assertHasAttr(err, "stack")

    def test_jsproxy_get(self):
        o = run_js("({get(x) {return x + 1;}})")
        self.assertEqual(o[5], 6)
        self.assertEqual(o[77], 78)

    def test_jsproxy_has(self):
        o = run_js("({has(x) {return x === 3 || x === 5;}})")
        self.assertTrue(3 in o)
        self.assertTrue(5 in o)
        self.assertFalse(1 in o)
        self.assertFalse(7 in o)

    def test_jsproxy_includes(self):
        o = run_js("({includes(x) {return x === 3 || x === 5;}})")
        self.assertTrue(3 in o)
        self.assertTrue(5 in o)
        self.assertFalse(1 in o)
        self.assertFalse(7 in o)

    def test_jsproxy_prefer_has_over_includes(self):
        o = run_js(
            "({includes(x) {return x === 3;}, has(x) { return x === 5 }})"
        )
        self.assertTrue(5 in o)
        self.assertFalse(3 in o)

    def test_jsproxy_len(self):
        o = run_js("({length: 7})")
        self.assertEqual(len(o), 7)
        o = run_js("({size: 5})")
        self.assertEqual(len(o), 5)
        # Prefer size over length
        o = run_js("({length: 7, size: 5})")
        self.assertEqual(len(o), 5)
        # Even though functions have a .length in JS, there is a special case
        # that makes them not have a len.
        f = run_js("(function(){})")
        with self.assertRaisesRegex(
            TypeError, "object of type 'pyodide.ffi.JsProxy' has no len()"
        ):
            len(f)

    def test_jsproxy_len_errors(self):
        o = run_js(f"({{length : []}})")
        with self.assertRaises(
            TypeError, msg="object does not have a valid length"
        ):
            len(o)

        for n in [1 << 31, 1 << 32, 1 << 33, 1 << 63, 1 << 64, 1 << 65]:
            o = run_js(f"({{length : {n}}})")
            msg = f"length {n} of object is larger than INT_MAX (2147483647)"
            with self.assertRaises(OverflowError, msg=msg):
                len(o)

        for n in [
            -1,
            -2,
            -3,
            -100,
            -1 << 31,
            -1 << 32,
            -1 << 33,
            -1 << 63,
            -1 << 64,
            -1 << 65,
        ]:
            o = run_js(f"({{length : {n}}})")
            msg = f"length {n} of object is negative"
            with self.assertRaises(ValueError, msg=msg):
                len(o)

    def test_jsproxy_set(self):
        o = run_js("({set(key, val) {this['$' + key] = val;} })")
        o["x"] = 7
        self.assertEqual(run_js("(o) => o.$x")(o), 7)

    def test_jsproxy_map(self):
        o = run_js("new Map([[3, 1], [5, 2]])")
        self.assertEqual(o[3], 1)
        self.assertEqual(o[5], 2)
        o[7] = 9
        del o[3]
        l = set(run_js("(o) => o.keys()")(o))
        self.assertEqual(l, {5, 7})

    def test_js_proxy_array(self):
        a = Array.new()
        a.push(1)
        assert a.to_py() == [1]

    def test_array(self):
        pyl = [5, 1, 2, 3]
        jsl = run_js(str(pyl))
        pyl2 = [7, -1, 66]
        jsl2 = run_js(str(pyl2))
        self.assertEqual(jsl[0], pyl[0])
        self.assertEqual(jsl[-1], pyl[-1])
        self.assertEqual(list(jsl), pyl)
        jsl[0] = 25
        pyl[0] = 25
        self.assertEqual(list(jsl), pyl)
        self.assertEqual(list(jsl[1:-2]), pyl[1:-2])
        self.assertEqual(list(jsl[::2]), pyl[::2])
        self.assertEqual(list(reversed(jsl)), list(reversed(pyl)))
        jsl[3:3] = [1, 2, 3]
        pyl[3:3] = [1, 2, 3]
        self.assertEqual(list(jsl), pyl)
        jsl += [7, 6, 5]
        pyl += [7, 6, 5]
        self.assertEqual(list(jsl), pyl)
        self.assertEqual(list(jsl + jsl2), pyl + pyl2)
        self.assertEqual(list(jsl * 2), pyl * 2)
        jsl *= 2
        pyl *= 2
        self.assertEqual(list(jsl), pyl)
        self.assertEqual(len(jsl), len(pyl))

        def gen():
            yield 6
            yield -3
            yield 52

        jsl.extend(gen())
        pyl.extend(gen())
        self.assertEqual(list(jsl), pyl)
        self.assertEqual(jsl.pop(), pyl.pop())
        self.assertEqual(list(jsl), pyl)
        jsl.append(-3)
        pyl.append(-3)
        self.assertEqual(list(jsl), pyl)
        jsl.reverse()
        pyl.reverse()
        self.assertEqual(list(jsl), pyl)
        jsl.insert(5, 22)
        pyl.insert(5, 22)
        self.assertEqual(list(jsl), pyl)
        # TODO:
        # index, count, remove

    def test_gen_close_throw(self):
        f = run_js(
            """
            (function *() {
                try {
                    yield 1;
                } finally {
                    yield 2;
                    console.log("finally");
                }
            })
            """
        )

        g = f()
        self.assertEqual(next(g), 1)
        self.assertEqual(g.throw(TypeError("hi")), 2)
        with self.assertRaises(TypeError, msg="hi"):
            next(g)

        g = f()
        self.assertEqual(next(g), 1)
        self.assertEqual(g.throw(TypeError, "hi"), 2)
        with self.assertRaises(TypeError, msg="hi"):
            next(g)

        f = run_js(
            """
            (function *() {
                yield 1;
                yield 2;
                yield 3;
            })
            """
        )
        g = f()
        self.assertEqual(next(g), 1)
        g.close()


class PyProxyTest(TestCase):
    def test_pyproxy(self):
        d = {1: 7}
        self.assertEqual(run_js("(x) => x.toString()")(d), str(d))
        self.assertEqual(run_js("(x) => x.type")(d), "dict")

    def test_pyproxy_to_py(self):
        x = [1, 2, 3]
        self.assertIs(run_js("(x) => x")(x), x)

    def test_pyproxy_getattr(self):
        class T:
            a = 7
            b = "zz"

        self.assertEqual(run_js("(o) => o.a")(T), 7)
        self.assertEqual(run_js("(o) => o.b")(T), "zz")
        self.assertEqual(run_js("(o) => o.c")(T), None)
        run_js("(o) => {delete o.a}")(T)
        self.assertFalse(hasattr(T, "a"))
        run_js("(o) => {o.a = 72}")(T)
        self.assertTrue(T.a == 72)

    def test_pyproxy_ownkeys(self):
        class T:
            a = 7
            b = "zz"

        l = set(
            x
            for x in run_js("(o) => Reflect.ownKeys(o)")(T)
            if isinstance(x, str)
        )
        self.assertGreaterEqual(l, {"a", "b"})

    def test_pyproxy_dict(self):
        d = {"a": 7, "b": 2, 3: "99"}
        l = set(
            x
            for x in run_js("(o) => Reflect.ownKeys(o)")(d)
            if isinstance(x, str)
        )
        self.assertGreaterEqual(l, {"a", "b", "3"})

        self.assertEqual(run_js("(o) => o.get('a')")(d), 7)
        self.assertEqual(run_js("(o) => o.get(3)")(d), "99")
        self.assertEqual(run_js("(o) => o.get('3')")(d), None)
        self.assertEqual(run_js("(o) => o.a")(d), 7)
        self.assertEqual(run_js("(o) => o['a']")(d), 7)
        self.assertEqual(run_js("(o) => o[3]")(d), "99")

        run_js("(o) => delete o[3]")(d)
        self.assertNotIn(3, d)
        run_js("(o) => o.delete('b')")(d)
        self.assertNotIn("b", d)
        run_js("(o) => o.z = 9")(d)
        self.assertEqual(d["z"], 9)
        run_js("(o) => o.set('q', 32)")(d)
        self.assertEqual(d["q"], 32)

        # TODO:
        # self.assertEqual(list(run_js("(o) => o.items()")(d)), [])

    def test_pyproxy_call_simple(self):
        def f(x):
            return x * x + 7

        self.assertEqual(run_js("(f) => f(7)")(f), f(7))

    def test_pyproxy_call_kwargs(self):
        def f(*, x, y):
            return x * x + y * y

        self.assertEqual(
            run_js("(f) => f.callKwargs({x: 7, y: 3})")(f), f(x=7, y=3)
        )

    def test_pyproxy_call_full(self):
        def f(x=2, y=3):
            return [x, y]

        self.assertEqual(list(run_js("(f) => f()")(f)), [2, 3])
        self.assertEqual(list(run_js("(f) => f(7)")(f)), [7, 3])
        self.assertEqual(list(run_js("(f) => f(7, -1)")(f)), [7, -1])
        self.assertEqual(list(run_js("(f) => f.callKwargs({})")(f)), [2, 3])
        self.assertEqual(list(run_js("(f) => f.callKwargs(7, {})")(f)), [7, 3])
        self.assertEqual(
            list(run_js("(f) => f.callKwargs(7, -1, {})")(f)), [7, -1]
        )
        self.assertEqual(
            list(run_js("(f) => f.callKwargs({ y : 4 })")(f)), [2, 4]
        )
        self.assertEqual(
            list(run_js("(f) => f.callKwargs({ y : 4, x : 9 })")(f)), [9, 4]
        )
        self.assertEqual(
            list(run_js("(f) => f.callKwargs(8, { y : 4 })")(f)), [8, 4]
        )

        with self.assertRaisesRegex(
            JsError, "TypeError: callKwargs requires at least one argument"
        ):
            run_js("(f) => f.callKwargs()")(f)

        with self.assertRaisesRegex(
            TypeError, r"f\(\) got an unexpected keyword argument 'z'"
        ):
            run_js("(f) => f.callKwargs({z : 6})")(f)

        with self.assertRaisesRegex(
            TypeError, r"f\(\) got multiple values for argument 'x'"
        ):
            run_js("(f) => f.callKwargs(76, {x : 6})")(f)

    def test_pyproxy_this1(self):
        def f(self, x):
            return getattr(self, x)

        res = run_js(
            """
            (f) => {
                const x = {};
                x.f = f.captureThis();
                x.a = 7;
                return x.f("a");
            }
            """
        )(f)
        self.assertEqual(res, 7)

    def test_pyproxy_bind(self):
        self.skipTest("TODO")

    def test_pyproxy_contains(self):
        d = {1, "a"}
        f = run_js("(x) => [x.has(1), x.has(2), x.has('a'), x.has('b')]")
        resjs = f(d)
        self.assertEqual(list(resjs), [True, False, True, False])

        with self.assertRaisesRegex(TypeError, "unhashable type"):
            run_js("(x) => x.has({})")(d)

    def test_pyproxy_get(self):
        d = {1: 2, "a": "q"}
        f = run_js("(x) => [x.get(1), x.get(2), x.get('a'), x.get('b')]")
        resjs = f(d)
        self.assertEqual(list(resjs), [2, None, "q", None])

    def test_pyproxy_length(self):
        f = run_js("(x) => x.length")
        for x in [{"x", "y", "z"}, [1, 2, 3], (1, "x"), {"x": 2}]:
            self.assertEqual(f(x), len(x))

    def test_pyproxy_set_del(self):
        d = {"x": 2}
        run_js("(d) => d.set('y', 3)")(d)
        self.assertEqual(d["x"], 2)
        self.assertEqual(d["y"], 3)
        run_js("(d) => d.set('x', 7)")(d)
        self.assertEqual(d["x"], 7)
        self.assertEqual(d["y"], 3)
        run_js("(d) => d.delete('x')")(d)
        self.assertNotIn("x", d)

    def test_pyproxy_iterable(self):
        d = [7, 21, 39]
        res = list(run_js("(d) => Array.from(d)")(d))
        self.assertEqual(res, d)

    def test_pyproxy_iterator(self):
        d = [7, 21, 39]
        res = list(
            run_js("(d) => [d.next(), d.next(), d.next(), d.next()]")(iter(d))
        )
        self.assertFalse(res[0].done)
        self.assertFalse(res[1].done)
        self.assertFalse(res[2].done)
        self.assertTrue(res[3].done)
        self.assertEqual(res[0].value, 7)
        self.assertEqual(res[1].value, 21)
        self.assertEqual(res[2].value, 39)
        self.assertEqual(res[3].value, None)

    def test_gen_return(self):
        def g1():
            yield 1
            yield 2

        p = run_js(
            """
            (g) => {
                let r = [];
                r.push(g.next());
                r.push(g.return(5));
                return r;
            }
        """
        )(g1())
        self.assertFalse(p[0].done)
        self.assertTrue(p[1].done)
        self.assertEqual(p[0].value, 1)
        self.assertEqual(p[1].value, 5)

        def g2():
            try:
                yield 1
                yield 2
            finally:
                yield 3
                return 5

        p = run_js(
            """
            (g) => {
                let r = [];
                r.push(g.next());
                r.push(g.return(5));
                r.push(g.next());
                return r;
            }
        """
        )(g2())
        self.assertFalse(p[0].done)
        self.assertFalse(p[1].done)
        self.assertTrue(p[2].done)
        self.assertEqual(p[0].value, 1)
        self.assertEqual(p[1].value, 3)
        self.assertEqual(p[2].value, 5)

        def g3():
            try:
                yield 1
                yield 2
            finally:
                return 3

        p = run_js(
            """
            (g) => {
                let r = [];
                r.push(g.next());
                r.push(g.return(5));
                return r;
            }
        """
        )(g3())
        self.assertFalse(p[0].done)
        self.assertTrue(p[1].done)
        self.assertEqual(p[0].value, 1)
        self.assertEqual(p[1].value, 3)

    def test_gen_throw(self):
        def g1():
            yield 1
            yield 2

        p = run_js(
            """
            (g) => {
                g.next();
                g.throw(new TypeError('hi'));
            }
        """
        )
        with self.assertRaisesRegex(JsError, "hi"):
            p(g1())

        def g2():
            try:
                yield 1
                yield 2
            finally:
                yield 3
                return 5

        p = run_js(
            """
            (g) => {
                let r = [];
                r.push(g.next());
                r.push(g.throw(new TypeError('hi')));
                r.push(g.next());
                return r;
            }
        """
        )(g2())
        self.assertFalse(p[0].done)
        self.assertFalse(p[1].done)
        self.assertTrue(p[2].done)
        self.assertEqual(p[0].value, 1)
        self.assertEqual(p[1].value, 3)
        self.assertEqual(p[2].value, 5)

        def g3():
            try:
                yield 1
                yield 2
            finally:
                return 3

        p = run_js(
            """
            (g) => {
                let r = [];
                r.push(g.next());
                r.push(g.throw(new TypeError('hi')));
                return r;
            }
        """
        )(g3())
        self.assertFalse(p[0].done)
        self.assertTrue(p[1].done)
        self.assertEqual(p[0].value, 1)
        self.assertEqual(p[1].value, 3)

    def test_pyproxy_of_list_index(self):
        pylist = [9, 8, 7]
        jslist = run_js(
            """
            (p) => {
                return [p[0], p[1], p[2]]
            }
            """
        )(pylist)
        self.assertEqual(list(jslist), pylist)

    def test_pyproxy_of_list_join(self):
        a = ["Wind", "Water", "Fire"]
        ajs = Array.from_(a)
        func = run_js("((a, k) => a.join(k))")

        self.assertEqual(func(a, None), func(ajs, None))
        self.assertEqual(func(a, ", "), func(ajs, ", "))
        self.assertEqual(func(a, " "), func(ajs, " "))

    def test_pyproxy_of_list_slice(self):
        a = ["ant", "bison", "camel", "duck", "elephant"]
        ajs = Array.from_(a)

        func_strs = [
            "a.slice(2)",
            "a.slice(2, 4)",
            "a.slice(1, 5)",
            "a.slice(-2)",
            "a.slice(2, -1)",
            "a.slice()",
        ]
        for func_str in func_strs:
            func = run_js(f"(a) => {func_str}")
            self.assertEqual(list(func(a)), list(func(ajs)))

    def test_pyproxy_of_list_indexOf(self):
        a = ["ant", "bison", "camel", "duck", "bison"]
        ajs = Array.from_(a)

        func_strs = [
            "beasts.indexOf('bison')",
            "beasts.indexOf('bison', 2)",
            "beasts.indexOf('bison', -4)",
            "beasts.indexOf('bison', 3)",
            "beasts.indexOf('giraffe')",
        ]
        for func_str in func_strs:
            func = run_js(f"(beasts) => {func_str}")
            self.assertEqual(func(a), func(ajs))

    def test_pyproxy_of_list_lastIndexOf(self):
        a = ["ant", "bison", "camel", "duck", "bison"]
        ajs = Array.from_(a)

        func_strs = [
            "beasts.lastIndexOf('bison')",
            "beasts.lastIndexOf('bison', 2)",
            "beasts.lastIndexOf('bison', -4)",
            "beasts.lastIndexOf('bison', 3)",
            "beasts.lastIndexOf('giraffe')",
        ]
        for func_str in func_strs:
            func = run_js(f"(beasts) => {func_str}")
            self.assertEqual(func(a), func(ajs))

    def test_pyproxy_of_list_forEach(self):
        a = ["a", "b", "c"]
        ajs = Array.from_(a)

        func = run_js(
            """
            ((a) => {
                let s = "";
                a.forEach((elt, idx, list) => {
                    s += "::";
                    s += idx;
                    s += elt;
                    s += this[elt];
                },
                    {a: 6, b: 9, c: 22}
                );
                return s;
            })
            """
        )

        self.assertEqual(func(a), func(ajs))

    def test_pyproxy_of_list_map(self):
        a = ["a", "b", "c"]
        ajs = Array.from_(a)
        func = run_js(
            """
            (a) => a.map(
                function (elt, idx, list){
                    return [elt, idx, this[elt]]
                },
                {a: 6, b: 9, c: 22}
            )
            """
        )
        self.assertEqual(
            [list(x) for x in func(a)], [list(x) for x in func(ajs)]
        )

    def test_pyproxy_of_list_filter(self):
        a = list(range(20, 0, -2))
        ajs = Array.from_(a)
        func = run_js(
            """
            (a) => a.filter(
                function (elt, idx){
                    return elt + idx > 12
                }
            )
            """
        )
        self.assertEqual(list(func(a)), list(func(ajs)))

    def test_pyproxy_of_list_reduce(self):
        a = list(range(20, 0, -2))
        ajs = Array.from_(a)
        func = run_js(
            """
            (a) => a.reduce((l, r) => l + 2*r)
            """
        )
        self.assertEqual(func(a), func(ajs))

    def test_pyproxy_of_list_reduceRight(self):
        a = list(range(20, 0, -2))
        ajs = Array.from_(a)
        func = run_js(
            """
            (a) => a.reduceRight((l, r) => l + 2*r)
            """
        )
        self.assertEqual(func(a), func(ajs))

    def test_pyproxy_of_list_some(self):
        func = run_js(
            "(a) => a.some((element, idx) => (element + idx) % 2 === 0)"
        )
        for a in [
            [1, 2, 3, 4, 5],
            [2, 3, 4, 5],
            [1, 3, 5],
            [1, 4, 5],
            [4, 5],
        ]:
            self.assertEqual(func(a), func(Array.from_(a)))

    def test_pyproxy_of_list_every(self):
        func = run_js(
            "(a) => a.every((element, idx) => (element + idx) % 2 === 0)"
        )
        for a in [
            [1, 2, 3, 4, 5],
            [2, 3, 4, 5],
            [1, 3, 5],
            [1, 4, 5],
            [4, 5],
        ]:
            self.assertEqual(func(a), func(Array.from_(a)))

    def test_pyproxy_of_list_at(self):
        a = [5, 12, 8, 130, 44]
        ajs = Array.from_(a)

        func = run_js("(a, idx) => a.at(idx)")
        for idx in [2, 3, 4, -2, -3, -4, 5, 7, -7]:
            self.assertEqual(func(a, idx), func(ajs, idx))

    def test_pyproxy_of_list_concat(self):
        a = [[5, 12, 8], [130, 44], [6, 7, 7]]
        ajs = to_js(a)

        func = run_js("(a, b, c) => a.concat(b, c)")
        self.assertEqual(func(*a).to_py(), func(*ajs).to_py())

    def test_pyproxy_of_list_includes(self):
        a = [5, 12, 8, 130, 44, 6, 7, 7]
        ajs = Array.from_(a)

        func = run_js("(a, n) => a.includes(n)")
        for n in range(4, 10):
            self.assertEqual(func(a, n), func(ajs, n))

    def test_pyproxy_of_list_entries(self):
        a = [5, 12, 8, 130, 44, 6, 7, 7]
        ajs = Array.from_(a)

        func = run_js("(a, k) => Array.from(a[k]())")
        for k in ["entries", "keys", "values"]:
            self.assertEqual(func(a, k).to_py(), func(ajs, k).to_py())

    def test_pyproxy_of_list_find(self):
        a = [5, 12, 8, 130, 44, 6, 7, 7]
        ajs = Array.from_(a)

        func = run_js("(a, k) => a[k](element => element > 10)")
        for k in ["find", "findIndex"]:
            self.assertEqual(func(a, k), func(ajs, k))

    def test_pyproxy_of_list_sort(self):
        self.skipTest("TODO: Implement sort")
        # from
        # https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/sort#creating_displaying_and_sorting_an_array
        # Yes, JavaScript sort is weird.

        stringArray = ["Blue", "Humpback", "Beluga"]
        numberArray = [40, None, 1, 5, 200]
        numericStringArray = ["80", "9", "700"]
        mixedNumericArray = ["80", "9", "700", 40, 1, 5, 200]

        run_js("globalThis.compareNumbers = (a, b) => a - b")

        self.assertEqual(
            run_js("((a) => a.join())")(stringArray), "Blue,Humpback,Beluga"
        )
        self.assertIs(run_js("((a) => a.sort())")(stringArray), stringArray)
        self.assertEqual(stringArray, ["Beluga", "Blue", "Humpback"])

        self.assertEqual(
            run_js("((a) => a.join())")(numberArray), "40,,1,5,200"
        )
        self.assertEqual(
            run_js("((a) => a.sort())")(numberArray), [1, 200, 40, 5, None]
        )
        self.assertEqual(
            run_js("((a) => a.sort(compareNumbers))")(numberArray),
            [
                1,
                5,
                40,
                200,
                None,
            ],
        )

        self.assertEqual(
            run_js("((a) => a.join())")(numericStringArray), "80,9,700"
        )
        self.assertEqual(
            run_js("((a) => a.sort())")(numericStringArray), ["700", "80", "9"]
        )
        self.assertEqual(
            run_js("((a) => a.sort(compareNumbers))")(numericStringArray),
            [
                "9",
                "80",
                "700",
            ],
        )

        self.assertEqual(
            run_js("((a) => a.join())")(mixedNumericArray),
            "80,9,700,40,1,5,200",
        )
        self.assertEqual(
            run_js("((a) => a.sort())")(mixedNumericArray),
            [
                1,
                200,
                40,
                5,
                "700",
                "80",
                "9",
            ],
        )
        self.assertEqual(
            run_js("((a) => a.sort(compareNumbers))")(mixedNumericArray),
            [
                1,
                5,
                "9",
                40,
                "80",
                200,
                "700",
            ],
        )

    def test_pyproxy_of_list_reverse(self):
        a = [3, 2, 4, 1, 5]
        ajs = Array.from_(a)

        func = run_js("((a) => a.reverse())")
        self.assertIs(func(a), a)
        func(ajs)
        self.assertEqual(list(ajs), a)

    def test_pyproxy_of_list_splice(self):
        for func in [
            'splice(2, 0, "drum")',
            'splice(2, 0, "drum", "guitar")',
            "splice(3, 1)",
            'splice(2, 1, "trumpet")',
            'splice(0, 2, "parrot", "anemone", "blue")',
            "splice(2, 2)",
            "splice(-2, 1)",
            "splice(2)",
            "splice()",
        ]:
            a = ["angel", "clown", "mandarin", "sturgeon"]
            ajs = Array.from_(a)

            func = run_js(f"((a) => a.{func})")
            self.assertEqual(func(a).to_py(), func(ajs).to_py())
            self.assertEqual(a, list(ajs))

    def test_pyproxy_of_list_push(self):
        a = [4, 5, 6]
        ajs = Array.from_(a)

        func = run_js("(a) => a.push(1, 2, 3)")
        self.assertEqual(func(a), func(ajs))
        self.assertEqual(list(ajs), a)

        a = [4, 5, 6]
        ajs = Array.from_(a)
        func = run_js(
            """
            (a) => {
                a.push(1);
                a.push(2);
                return a.push(3);
            }
            """
        )
        self.assertEqual(func(a), func(ajs))
        self.assertEqual(list(ajs), a)

    def test_pyproxy_of_list_pop(self):
        func = run_js("((a) => a.pop())")

        for a in [
            [],
            ["broccoli", "cauliflower", "cabbage", "kale", "tomato"],
        ]:
            ajs = Array.from_(a)
            self.assertEqual(func(a), func(ajs))
            self.assertEqual(list(ajs), a)

    def test_pyproxy_of_list_shift(self):
        a = ["Andrew", "Tyrone", "Paul", "Maria", "Gayatri"]
        ajs = Array.from_(a)

        func = run_js(
            """
            (a) => {
                let result = [];
                while (typeof (i = a.shift()) !== "undefined") {
                    result.push(i);
                }
                return result;
            }
            """
        )
        self.assertEqual(list(func(a)), list(func(ajs)))
        self.assertEqual(a, [])
        self.assertEqual(list(ajs), [])

    def test_pyproxy_of_list_unshift(self):
        a = [4, 5, 6]
        ajs = Array.from_(a)

        func = run_js("(a) => a.unshift(1, 2, 3)")
        self.assertEqual(func(a), func(ajs))
        self.assertEqual(list(ajs), a)

        a = [4, 5, 6]
        ajs = Array.from_(a)
        func = run_js(
            """
            (a) => {
                a.unshift(1);
                a.unshift(2);
                return a.unshift(3);
            }
            """
        )
        self.assertEqual(func(a), func(ajs))
        self.assertEqual(list(ajs), a)

    def test_pyproxy_of_list_copyWithin(self):
        for func in [
            "copyWithin(-2)",
            "copyWithin(0, 3)",
            "copyWithin(0, 3, 4)",
            "copyWithin(-2, -3, -1)",
        ]:
            a = ["a", "b", "c", "d", "e"]
            ajs = Array.from_(a)
            func = run_js(f"(a) => a.{func}")
            self.assertIs(func(a), a)
            func(ajs)
            self.assertEqual(a, list(ajs))

    def test_pyproxy_of_list_fill(self):
        for func in [
            "fill(0, 2, 4)",
            "fill(5, 1)",
            "fill(6)",
        ]:
            a = ["a", "b", "c", "d", "e"]
            ajs = Array.from_(a)
            func = run_js(f"(a) => a.{func}")
            self.assertIs(func(a), a)
            func(ajs)
            self.assertEqual(a, list(ajs))
