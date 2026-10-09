# SPDX-FileCopyrightText: Michael Popoloski
# SPDX-License-Identifier: MIT

from pyslang.ast import Compilation, ErrorType
from pyslang.syntax import SyntaxTree


def test_array_recovery_requires_opt_in():
    comp = Compilation()
    comp.addSyntaxTree(
        SyntaxTree.fromText("""
module m;
    typedef struct packed { int field; } item_t;
    item_t items[MISSING];
    int valid_items[2];
endmodule
""")
    )
    assert len(comp.getAllDiagnostics()) == 1
    body = comp.getRoot().lookupName("m").body
    error = body.find("items").type.canonicalType
    assert isinstance(error, ErrorType)
    assert error.isError
    assert not error.isArray
    assert error.arrayElementType is None
    assert error.getArrayElementType() is None
    assert error.child is None
    assert not error.arrayDimension.isPacked
    assert error.arrayDimension.range is None
    element = error.getArrayElementType(allowError=True)
    assert element is error.arrayDimension.elementType
    assert element.canonicalType.find("field") is not None

    valid = body.find("valid_items").type
    assert valid.arrayElementType is valid.getArrayElementType()
    assert valid.arrayElementType is valid.getArrayElementType(allowError=True)


def test_invalid_array_elements_preserve_known_bounds():
    comp = Compilation()
    comp.addSyntaxTree(
        SyntaxTree.fromText("""
module m;
    typedef struct packed { real field; } item_t;
    item_t [3:0] items;
endmodule
""")
    )
    assert len(comp.getAllDiagnostics()) == 1
    error = comp.getRoot().lookupName("m.items").type.canonicalType
    assert error.arrayDimension.isPacked
    assert error.arrayDimension.range.left == 3
    assert error.arrayDimension.range.right == 0
    element = error.getArrayElementType(allowError=True).canonicalType
    assert element.isError
    assert element.child.find("field") is not None
