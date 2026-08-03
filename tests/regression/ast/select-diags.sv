// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

// RUN: %slang %s --diag-json %t --error-limit=0 2>&1 || true
// CHECK-DIAGS: %t

// String out-of-bounds accesses
module m_string_out_of_bounds_accesses;
    localparam string s = "asdf";
    int i = s[2:3];
//          ^ BadSliceType value of type 'string' cannot be sliced
//            ^^^ - for BadSliceType

    localparam byte j = foo();
//                      ^ NoteInCallTo in call to 'foo()' - for ConstEvalDynamicArrayIndex(string, 11)
//                      ^ NoteInCallTo in call to 'foo()' - for ConstEvalDynamicArrayIndex(string, 12)
    function byte foo;
        automatic string t = s;
        t[11] += 4;
//      ^^^^^ ConstEvalDynamicArrayIndex invalid index 11 for 'string' of length 4
        return t[12];
//             ^^^^^ ConstEvalDynamicArrayIndex invalid index 12 for 'string' of length 4
    endfunction
endmodule

// Out-of-bounds element selects in consteval
module m_out_of_bounds_element_selects_in_consteval;
    localparam string s[integer] = '{0: "hello", 2: "world"};
    localparam string t = s[1];
//                        ^ ConstEvalAssociativeElementNotFound element 1 does not exist in associative array
//                          ^ - for ConstEvalAssociativeElementNotFound
    localparam logic[6:1] u = 4;

    localparam int i = foo();
//                     ^ NoteInCallTo in call to 'foo()' - for ConstEvalAssociativeIndexInvalid('x)
//                     ^ NoteInCallTo in call to 'foo()' - for IndexOOB(string)
//                     ^ NoteInCallTo in call to 'foo()' - for IndexOOB(logic[6:1], 9)
//                     ^ NoteInCallTo in call to 'foo()' - for ConstEvalFunctionIdentifiersMustBeLocal(bar)

    int bar;
//      ^ NoteDeclarationHere declared here - for ConstEvalFunctionIdentifiersMustBeLocal(bar)
    function automatic int foo;
        string k = s['x];
//                   ^^ ConstEvalAssociativeIndexInvalid index 32'sdx has x or z bits which is invalid
        logic l1 = 'x;
        byte b = t[l1];
//               ^^^^^ IndexOOB cannot refer to element 1'bx of 'string'
        int q = 9;
        logic l2 = u[q];
//                 ^^^^ IndexOOB cannot refer to element 9 of 'logic[6:1]'

        k[bar] = 1;
//        ^^^ ConstEvalFunctionIdentifiersMustBeLocal all identifiers that are not parameters or enums must be declared locally to a constant function
    endfunction
endmodule

// Out-of-bounds range selects in consteval
module m_out_of_bounds_range_selects_in_consteval;
    string s[integer] = '{0: "hello", 2: "world"};
    string t = s[2:1];
//             ^^^^^^ RangeSelectAssociative cannot take a slice of an associative array

    real r = 1.0;
    logic [7:0] u = 0;
//              ^ NoteDeclarationHere declared here - for ConstEvalNonConstVariable(u)
//              ^ NoteDeclarationHere declared here - for ConstEvalFunctionIdentifiersMustBeLocal(u)
    logic [1:0] v1 = u[r:0];
//                     ^ ExprMustBeIntegral expression type 'real' is not integral
    logic [1:0] v2 = u[0:r];
//                       ^ ExprMustBeIntegral expression type 'real' is not integral
    logic [1:0] v3 = u[1:2];
//                     ^^^ RangeSelectReversed range of selection [1:2] from 'logic[7:0]' is reversed
    logic [1:0] v4 = u[1+:-1];
//                        ^^ ValueMustBePositive value must be positive
    logic [9:0] v5 = u[1+:10];
//                     ^^^^^ RangeOOB cannot select range of [10:1] from 'logic[7:0]'
    logic [1:0] v6 = u['x+:2];
//                     ^^ IndexValueInvalid cannot refer to element 1'bx of 'logic[7:0]'
    logic [9:0] v7 = u[u+:10];
//                        ^^ RangeWidthOOB cannot select range of 10 elements from 'logic[7:0]'

    int w[] = {1};
//      ^ NoteDeclarationHere declared here - for ConstEvalFunctionIdentifiersMustBeLocal(w)
    int x1[2] = w[u:1];
//                ^ ConstEvalNonConstVariable reference to non-constant variable 'u' is not allowed in a constant expression
    int x2[2] = w[u+:-1];
//                   ^^ ValueMustBePositive value must be positive

    localparam int y[5] = {1,2,3,4,5};

    localparam int i1 = f1();
//                      ^ NoteInCallTo in call to 'f1()' - for RangeOOB(int$[5], -1)
//                      ^ NoteInCallTo in call to 'f1()' - for ConstEvalFunctionIdentifiersMustBeLocal(w)
    localparam int i2 = f2();
//                      ^ NoteInCallTo in call to 'f2()' - for ConstEvalFunctionIdentifiersMustBeLocal(u)
    localparam int i3 = f3();
//                      ^ NoteInCallTo in call to 'f3()' - for IndexValueInvalid(int$[])
    localparam int i4 = f4();
//                      ^ NoteInCallTo in call to 'f4()' - for IndexValueInvalid(int$[$])
    localparam int i5 = f5();
//                      ^ NoteInCallTo in call to 'f5()' - for IndexValueInvalid(int$[3])
    localparam int i6 = f6();
//                      ^ NoteInCallTo in call to 'f6()' - for ConstEvalDynamicArrayRange(int$[], -10)

    function automatic int f1;
        int a = -1;
        int b[] = y[a+:3];
//                ^^^^^^^ RangeOOB cannot select range of [-1:1] from 'int$[5]'
        w[0:1] = {1,1};
//      ^ ConstEvalFunctionIdentifiersMustBeLocal all identifiers that are not parameters or enums must be declared locally to a constant function
    endfunction

    function automatic int f2;
        int c[];
        c[u+:2] = {1,2};
//        ^ ConstEvalFunctionIdentifiersMustBeLocal all identifiers that are not parameters or enums must be declared locally to a constant function
    endfunction

    function automatic int f3;
        integer a = 'x;
        int c[];
        c[a+:2] = {1,2};
//        ^ IndexValueInvalid cannot refer to element 32'sdx of 'int$[]'
    endfunction

    function automatic int f4;
        integer a = 'x;
        int c[$];
        c[2+:a] = {1,2};
//           ^ IndexValueInvalid cannot refer to element 32'sdx of 'int$[$]'
    endfunction

    function automatic int f5;
        integer a = 'x;
        int c[3];
        int d[2] = c[a+:2];
//                   ^ IndexValueInvalid cannot refer to element 32'sdx of 'int$[3]'
    endfunction

    function automatic int f6;
        int c[];
        int d[20] = c[-10+:20];
//                  ^^^^^^^^^^ ConstEvalDynamicArrayRange invalid range [-10:9] for 'int$[]' of length 0
    endfunction
endmodule

// No range select ordering error for single bit value
module m_no_range_select_ordering_error_for_single_bit_value;
    logic [0:0] a;
    initial $display(a[-1:0]);
//                     ^^^^ RangeOOB cannot select range of [-1:0] from 'logic[0:0]'
endmodule

// Indexing with unknowns has reasonable diagnostic printing
logic [7:0] a;
logic b = a['dx];
//          ^^^ IndexOOB cannot refer to element 32'dx of 'logic[7:0]'

// Range select out of bounds
module m_range_select_out_of_bounds;
    logic [3:0] x;
    logic [2:0] y;
    initial y = x[7:5];
//                ^^^ RangeOOB cannot select range of [7:5] from 'logic[3:0]'
endmodule

// Range select out of bounds during constant eval
module m_range_select_out_of_bounds_during_constant_eval;
    function automatic int f;
        logic [3:0] v = 0;
        logic [1:0] w;
        w = v[-1:-2];
//            ^^^^^ RangeOOB cannot select range of [-1:-2] from 'logic[3:0]'
        return 0;
    endfunction
    localparam int p = f();
endmodule

// Index out of bounds during constant eval
module m_index_out_of_bounds_during_constant_eval;
    function automatic int f;
        int arr[4];
        int i = -1;
        int y;
        y = arr[i];
//          ^^^^^^ IndexOOB cannot refer to element -1 of 'int$[4]'
        return 0;
    endfunction
    localparam int p = f();
//                     ^ NoteInCallTo in call to 'f()' - for IndexOOB(int$[4], -1)
endmodule
