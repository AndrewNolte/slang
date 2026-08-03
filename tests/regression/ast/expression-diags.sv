// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

// RUN: %slang %s --diag-json %t --error-limit=0 2>&1 || true
// CHECK-DIAGS: %t

// Wrong typename in error regress GH #1013
module t40_wrong_typename_in_error_regress_gh_1013;
typedef struct {
        logic a;
        logic b;
} t;
t x, y, z;
assign z = x & y;
//         ^ BadBinaryExpression invalid operands to binary expression ('t40_wrong_typename_in_error_regress_gh_1013.t' and 't40_wrong_typename_in_error_regress_gh_1013.t')
//           ^ - for BadBinaryExpression
//             ^ - for BadBinaryExpression

endmodule

// Bad concatenation expressions
module m_bad_concatenation_expressions;
    string s;
    int i;
    event e;
    initial begin
        i = {i, e};
//              ^ BadConcatExpression invalid operand type 'event' in concatenation
        s = {s, e};
//              ^ BadConcatExpression invalid operand type 'event' in concatenation
        s = {s, i};
//              ^ ConcatWithStringInt cannot mix strings and integers in a concatenation (use a cast if this is desired)
    end
endmodule

// Expression not allowed as a statement
module m_expression_not_allowed_as_a_statement;
    int x;
    initial begin
        x + 1;
//      ^^^^^ ExprNotStatement expression is not allowed as a statement
    end
endmodule

// Expression is not assignable diagnostic
module m_expression_is_not_assignable_diagnostic;
    initial begin
        (1 + 1) = 2;
//       ^^^^^ ExpressionNotAssignable expression is not assignable
    end
endmodule

// Bad replication expression operands
module m_bad_replication_expression_operands;
    real r;
    bit [3:0] x;
    initial x = {r{1'b1}};
//               ^ BadReplicationExpression invalid operands to replication expression ('real' and 'bit[0:0]')
//                ^^^^^^ - for BadReplicationExpression
endmodule

// Replication count zero outside concatenation
module m_replication_count_zero_outside_concatenation;
    bit b;
    initial b = {0{1'b1}};
//               ^ ReplicationZeroOutsideConcat replication constant can only be zero inside of a concatenation
endmodule

// Unknown built-in method on string
module m_unknown_built_in_method_on_string;
    string s;
    initial s.foobar();
//            ^^^^^^ UnknownSystemMethod unknown built-in method 'foobar' on type 'string'
endmodule

// Invalid member access on non-class type
module m_invalid_member_access_on_non_class_type;
    int x;
    int y;
    initial y = x.foo;
//              ^ InvalidMemberAccess invalid member access for type 'int'
//                ^^^ - for InvalidMemberAccess
endmodule

// Named argument not allowed in builtin method
module m_named_argument_not_allowed_in_builtin_method;
    string s = "hi";
    string r;
    initial r = s.substr(.x(0), .y(1));
//                       ^^^^^ NamedArgNotAllowed named argument not allowed
endmodule

// Empty argument not allowed in builtin method
module m_empty_argument_not_allowed_in_builtin_method;
    string s = "hi";
    string r;
    initial r = s.substr(,);
//                       ^ EmptyArgNotAllowed empty argument not allowed
endmodule

// Bad integer cast of non-integral expression
module m_bad_integer_cast_of_non_integral_expression;
    real r;
    int x;
    initial x = 4'(r);
//              ^ BadIntegerCast cannot change width or signedness of non-integral expression (type is 'real')
//                 ^ - for BadIntegerCast
endmodule

// Bad value range with non-numeric bounds
module m_bad_value_range_with_non_numeric_bounds;
    chandle c;
    int x;
    initial if (x inside {[c:c]}) begin end
//                         ^ BadValueRange invalid bounds in value range ('chandle' and 'chandle')
//                           ^ - for BadValueRange
endmodule

// Invalid class member access
class C;
    typedef int T;
endclass

module m_invalid_class_member_access;
    C c = new;
    int x;
    initial x = c.T;
//              ^ InvalidClassAccess cannot access 'T' in 'C' with '.'
//                ^ - for InvalidClassAccess
endmodule

// Redefinition of pattern variable
module m_redefinition_of_pattern_variable;
    typedef struct packed { int a; int b; } s_t;
    s_t s;
    initial if (s matches '{a: .x, b: .x}) begin end
//                                     ^ Redefinition redefinition of 'x'
//                              ^ NotePreviousDefinition previous definition here - for Redefinition(x)
endmodule

// Bad integer cast with signed cast of non-integral
module m_bad_integer_cast_with_signed_cast_of_non_integral;
    real r;
    int x;
    initial x = unsigned'(r);
//                        ^ BadIntegerCast cannot change width or signedness of non-integral expression (type is 'real')
endmodule

// Invalid member access without invocation syntax
module m_invalid_member_access_without_invocation_syntax;
    string s;
    int y;
    initial y = s.foo;
//              ^ InvalidMemberAccess invalid member access for type 'string'
//                ^^^ - for InvalidMemberAccess
endmodule
