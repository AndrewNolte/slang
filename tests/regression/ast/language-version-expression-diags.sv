// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

// RUN: %slang %s --diag-json %t --error-limit=0 --std=1800-2023 2>&1 || true
// CHECK-DIAGS: %t

// Bad value range with non-numeric tolerance bounds
module m_bad_value_range_with_non_numeric_tolerance_bounds;
    chandle c;
    int x;
    initial if (x inside {[c +/- 1]}) begin end
//                         ^ BadValueRange invalid bounds in value range ('chandle' and 'int')
//                               ^ - for BadValueRange
endmodule
