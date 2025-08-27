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
