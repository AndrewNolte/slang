// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

// RUN: %slang %s --diag-json %t --error-limit=0 2>&1 || true
// CHECK-DIAGS: %t

// Array method with-clause plumbing
module m_array_method_with_clause_plumbing;
    typedef struct { int i; } asdf_t;
    function asdf_t foo;
    endfunction

    asdf_t a = '{default:0};

    int j = foo().i();
//          ^^^^^^^^^ ExpressionNotCallable expression is not callable
//                ^ - for ExpressionNotCallable
    int k = foo().i with (bar);
//                  ^^^^ UnexpectedWithClause unexpected 'with' clause
    int l = a.i with (bar);
//              ^^^^ UnexpectedWithClause unexpected 'with' clause
    int m = foo() with (bar);
//                ^^^^ WithClauseNotAllowed cannot use 'with' expression with 'foo'
    int n = $bits(a) with (bar);
//                   ^^^^ WithClauseNotAllowed cannot use 'with' expression with '$bits'

    int o[3] = '{default:0};
    int p = o.and(a);
//               ^^^ IteratorArgsWithoutWithClause cannot provide arguments to 'and' without corresponding 'with' clause
    int q = o.and();
    int r = o.and with (1) { 1; };
//                         ^^^^^^ UnexpectedConstraintBlock unexpected constraint block
    int s = o.and with;
//                ^^^^ ExpectedIterationExpression expected a single array iteration expression
    int t = o.and with (a, b);
//                     ^^^^^^ ExpectedIterationExpression expected a single array iteration expression
    int u = o.and(a, b, c) with (a == 1);
//               ^^^^^^^^^ TooManyArguments too many arguments for 'and'; expected 2 but 3 were provided
    int v = o.and(a[1]) with (a == 1);
//                ^^^^ ExpectedIteratorName expected name of iterator for use with 'with' expression
    int w = o.and(,) with (a == 1);
//                ^ EmptyArgNotAllowed empty argument not allowed
    int x = o.and(a, .foo()) with (a == 1);
//                   ^^^^^^ NamedArgNotAllowed named argument not allowed
    int y = o.and(posedge clk) with (a == 1);
//                ^^^^^^^^^^^ InvalidArgumentExpr sequence and property expressions are not valid in this context

    // These are ok.
    int z = o.and(b) with (b + 1);
    int aa = o.and with (item + x);
endmodule
