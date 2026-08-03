// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

// RUN: %slang %s --diag-json %t --error-limit=0 2>&1 || true
// CHECK-DIAGS: %t

// Disallowed assignment contexts
module m_disallowed_assignment_contexts;
    int i;
    int j;
//      ^ NoteDeclarationHere declared here - for ConstEvalNonConstVariable(j)
    int p;
    logic [(j = 2) : 0] asdf;
//          ^ ConstEvalNonConstVariable reference to non-constant variable 'j' is not allowed in a constant expression
    assign i = 1 + (j = 1);
//                  ^^^^^ AssignmentNotAllowed assignment expressions are not allowed in this context

    initial p = {j = 1};
//               ^^^^^ AssignmentRequiresParens assignment expressions must be parenthesized
    initial if (p = 1) begin end
//              ^^^^^ AssignmentRequiresParens assignment expressions must be parenthesized

    assign i = i++;
//             ^^^ IncDecNotAllowed increment and decrement expressions are not allowed in this context
    assign i = ++i;
//             ^^^ IncDecNotAllowed increment and decrement expressions are not allowed in this context

    // This is ok
    initial p = 1 + (j = 1);

    function func(int k);
    endfunction

    // Initialization in a procedural context is also ok
    initial begin
        automatic int k = 1;
        automatic int l = k++;
        static int m = 2;
        static int n = m++;

        static int foo = k; // disallowed
//                       ^ AutoFromStaticInit cannot refer to automatic variable 'k' from static initializer

        func.k = 4; // ok, param is static
    end

    final begin
        p <= 5;
//      ^^^^^^ NonblockingInFinal nonblocking assignments in final blocks have no effect
    end
endmodule
