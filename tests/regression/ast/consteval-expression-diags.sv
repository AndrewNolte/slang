// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

// RUN: %slang %s --diag-json %t --error-limit=0 2>&1 || true
// CHECK-DIAGS: %t

// String replication count invalid in constant expression
module m_string_replication_count_invalid_in_constant_expression;
    function automatic string f(int n);
        return {n{"x"}};
//              ^ ConstEvalReplicationCountInvalid string replication count -1 is invalid in a constant expression
    endfunction
    localparam string s = f(-1);
//                        ^ NoteInCallTo in call to 'f(-1)' - for ConstEvalReplicationCountInvalid(-1)
endmodule

// Invalid dynamic array size in constant expression
module m_invalid_dynamic_array_size_in_constant_expression;
    function automatic int f;
        int a[];
        a = new[-1];
//              ^^ InvalidArraySize -1 is not a valid size for an array
        return 0;
    endfunction
    localparam int p = f();
//                     ^ NoteInCallTo in call to 'f()' - for InvalidArraySize(-1)
endmodule

// Class type not allowed in constant expression via copy
class ClassCopyArgument;
    int x;
endclass

module m_class_type_not_allowed_in_constant_expression_via_copy;
    function automatic int f(ClassCopyArgument a);
        ClassCopyArgument b;
        b = new a;
        return 0;
    endfunction
    ClassCopyArgument g = new;
    localparam int p = f(g);
//                       ^ ConstEvalClassType class types are not allowed in constant expressions
endmodule

// Class copy not allowed in constant expression
class ClassCopyLocal;
    int x;
endclass

module m_class_copy_not_allowed_in_constant_expression;
    function automatic int f();
        ClassCopyLocal a = new;
//                         ^^^ ConstEvalClassType class types are not allowed in constant expressions
        ClassCopyLocal b = new a;
        return 0;
    endfunction
    localparam int p = f();
//                     ^ NoteInCallTo in call to 'f()' - for ConstEvalClassType(new)
endmodule
