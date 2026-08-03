// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

// RUN: %slang %s --diag-json %t --error-limit=0 2>&1 || true
// CHECK-DIAGS: %t

// Assignment pattern - invalid default
module m_assignment_pattern_invalid_default;
    int i[] = '{default, 5};
//              ^^^^^^^ ExpectedExpression expected expression
endmodule

// Invalid assigment pattern lvalues
module m_invalid_assigment_pattern_lvalues;
    typedef struct { real r; string s; } U;
    function automatic void f1;
        real r;
        string s;
        U'{r:r, s:s} = '{3.14, "Hello World"};
//      ^^^^^^^^^^^^ ExpressionNotAssignable expression is not assignable
    endfunction

    function automatic void f2;
        int i[];
        int j, k;
        '{j, k} = i;
//      ^^^^^^^ AssignmentPatternLValueDynamic lvalue assignment patterns cannot be assigned from dynamic arrays
    endfunction
endmodule

// Bare associative array pattern -- diagnostic emitted with option
package P_bare_associative_array_pattern_diagnostic_emitted_with_optio;
    typedef int int_queue[$];
    task automatic T();
        int_queue q[string] = {"k":{0,1,2}};
//                            ^ BareAssociativePattern associative array literal is missing the required apostrophe prefix
    endtask
endpackage

// Bare associative pattern in unpacked array concat -- without option
module top_bare_associative_pattern_in_unpacked_array_concat_without_op;
    class C;
        string s[][int];
        function f();
            s = {
                {0: "sv1", 1: "sv2"},
//              ^ BareAssociativePattern associative array literal is missing the required apostrophe prefix
//              ^^^^^^^^^^^^^^^^^^^^ AssignmentPatternNoContext assignment pattern target type cannot be deduced in this context
                {1: "sv2",  2: "sv4"}
//              ^ BareAssociativePattern associative array literal is missing the required apostrophe prefix
//              ^^^^^^^^^^^^^^^^^^^^^ AssignmentPatternNoContext assignment pattern target type cannot be deduced in this context
            };
        endfunction
    endclass
endmodule

// Assignment pattern unused default is still error checked
typedef logic [7:0] RT[2];

function RT f1;
    return '{0:8, 1:9, default:'{default:foo}};
//                                       ^^^ UndeclaredIdentifier use of undeclared identifier 'foo'
endfunction

function RT f2;
    return '{0:8, 1:9, default:'{-1{foo}}};
//                               ^^ ValueMustBePositive value must be positive
//                                  ^^^ UndeclaredIdentifier use of undeclared identifier 'foo'
endfunction

$static_assert($sformatf("%p", f1()) == "'{8'd8, 8'd9}");
$static_assert($sformatf("%p", f2()) == "'{8'd8, 8'd9}");
