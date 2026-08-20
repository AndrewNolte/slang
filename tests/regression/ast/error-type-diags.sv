// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

// RUN: %slang %s --diag-json %t --error-limit=0 -Wunused 2>&1 || true
// CHECK-DIAGS: %t

typedef struct packed {
    real member;
//  ^^^^ PackedMemberNotIntegral packed members must be of integral type (not 'real')
} bad_struct_t;

nettype bad_struct_t bad_net;

module sink(input bad_net value);
//                        ^^^^^ UnusedPort unused port signal 'value'
endmodule

module top;
    parameter missing_macro = `MISSING_MACRO;
//                            ^^^^^^^^^^^^^^ UnknownDirective unknown macro or compiler directive '`MISSING_MACRO'
//                                          ^ ExpectedExpression expected expression
//            ^^^^^^^^^^^^^ UnusedParameter unused parameter 'missing_macro'
    missing_type opaque;
//  ^^^^^^^^^^^^ UndeclaredIdentifier use of undeclared identifier 'missing_type'

    enum bit [1:0][2:0] { A } bad_enum;
//       ^^^^^^^^^^^^^^ InvalidEnumBase invalid enum base type 'bit[1:0][2:0]' (must be a single dimensional integer type)
//                            ^^^^^^^^ UnusedVariable unused variable 'bad_enum'
    union packed { byte narrow; int wide; } bad_union;
//                                  ^^^^ PackedUnionWidthMismatch all members of a packed union must have the same width; 'wide' has width of 32, previously seen width was 8
//                                          ^^^^^^^^^ UnusedVariable unused variable 'bad_union'

    sink u_sink(0);
endmodule
