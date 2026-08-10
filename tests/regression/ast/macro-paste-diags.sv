// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

// RUN: %slang %s --diag-json %t --error-limit=0 2>&1 || true
// CHECK-DIAGS: %t

`define MAKE_FUNCTION(PREFIX, SUFFIX) \
    function PREFIX``SUFFIX PREFIX``_make(PREFIX``_type value); \
    endfunction

module m;
    `MAKE_FUNCTION(item, _result) // unrelated trailing text
//                 ^^^^^^^^^^^ UndeclaredIdentifier use of undeclared identifier 'item_result'
//                           ^^^^^^^^^ UndeclaredIdentifier use of undeclared identifier 'item_type'
endmodule
