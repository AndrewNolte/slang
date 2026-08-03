// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

// RUN: %slang %s --diag-json %t --error-limit=0 2>&1 || true
// CHECK-DIAGS: %t

// Integer literal corner cases
`define FOO aa_ff
`define BAR 'h

module m1_integer_literal_corner_cases;

    int i = 35'd123498234978234;
//              ^ VectorLiteralOverflow vector literal too large for the given number of bits (47 bits needed)
    int j = 0'd234;
//          ^ LiteralSizeIsZero size of vector literal cannot be zero
    int k = 16777216'd1;
//          ^ LiteralSizeTooLarge size of vector literal is too large (> 16777215 bits)
    int l = 16   `BAR `FOO;
    integer m = 'b ??0101?1;
    int n = 999999999999;
//          ^ SignedIntegerOverflow signed integer literal overflows 32 bits, will be truncated to -727379969
    int o = 'b _?1;
//             ^ DigitsLeadingUnderscore numeric literals must not start with a leading underscore
    int p = 'b3;
//            ^ BadBinaryDigit expected binary digit
    int q = 'ox789;
//              ^ BadOctalDigit expected octal digit
    int r = 'd?;
    int s = 'd  z_;
    int t = 'd x1;
//              ^ DecimalDigitMultipleUnknown decimal literals cannot have multiple digits if at least one of them is X or Z
    int u = 'd a;
//             ^ BadDecimalDigit expected decimal digit
    int v = 'h g;
//             ^ BadHexDigit expected hexadecimal digit
    int w = 3'h f;
//              ^ VectorLiteralOverflow vector literal too large for the given number of bits (4 bits needed)
    int x = 'd;
//            ^ ExpectedVectorDigits expected vector literal digits

endmodule

// Real literal corner cases
module m1_real_literal_corner_cases;
    real a = 9999e99999;
//           ^ RealLiteralOverflow value of real literal is too large; maximum is 1.79769e+308
    real b = 9999e-99999;
//           ^ RealLiteralUnderflow value of real literal is too small; minimum is 4.94066e-324
endmodule
