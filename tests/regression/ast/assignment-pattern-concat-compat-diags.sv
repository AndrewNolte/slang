// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

// RUN: %slang %s --diag-json %t --error-limit=0 --allow-array-concat-assign-pattern 2>&1 || true
// CHECK-DIAGS: %t

// Bare associative pattern in unpacked array concat -- with option
module top_bare_associative_pattern_in_unpacked_array_concat_with_optio;
    class C;
        string s[][int];
        function f();
            s = {
                {0: "sv1", 1: "sv2"},
//              ^ BareAssociativePattern associative array literal is missing the required apostrophe prefix
                {1: "sv2",  2: "sv4"}
//              ^ BareAssociativePattern associative array literal is missing the required apostrophe prefix
            };
        endfunction
    endclass
endmodule
