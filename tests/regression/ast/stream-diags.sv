// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

// RUN: %slang %s --diag-json %t --error-limit=0 2>&1 || true
// CHECK-DIAGS: %t

// Stream operator size overflow
module m_stream_operator_size_overflow;
    int i[];
    int j[99999999];
    assign i = {<< {j, j, j with [0+:400000000]}};
//             ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ ObjectTooLarge object size exceeds implementation limit of 2^31 bytes
//                                ^^^^^^^^^^^^ RangeOOB cannot select range of [0:399999999] from 'int$[99999999]'

    int k[$];
    int l[];
    always_comb l = {<< {k with [0+:999999999]}};
//                       ^^^^^^^^^^^^^^^^^^^^^ ObjectTooLarge object size exceeds implementation limit of 2^31 bytes

    typedef struct { int i[400000000]; logic l[$]; } asdf;
    asdf n_stream_operator_size_overflow [999999][][999999][];
    int o[];
    always {<< {o}} = {<< {n_stream_operator_size_overflow}};
//         ^^^^^^^^ BadStreamSize streaming operator target size 32*n does not fit source size <overflow>
//                  ^ - for BadStreamSize
//                    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ - for BadStreamSize

    struct { asdf a[]; int i[400000000]; logic l; } p;
    always {<< {p}} = {<< {o}};
//         ^^^^^^^^ BadStreamSize streaming operator target size <overflow> does not fit source size 32*n
//                  ^ - for BadStreamSize
//                    ^^^^^^^^ - for BadStreamSize

    class C;
        asdf n_stream_operator_size_overflow [999999][][999999][];
    endclass
    C c;
    always {<< {o}} = {<< {c}};
//         ^^^^^^^^ BadStreamSize streaming operator target size 32*n does not fit source size <overflow>
//                  ^ - for BadStreamSize
//                    ^^^^^^^^ - for BadStreamSize

    class D;
        int a[];
        int b[400000000];
    endclass
    D d;
    always {<< {o}} = {<< {d, d, d}};
//         ^^^^^^^^ BadStreamSize streaming operator target size 32*n does not fit source size <overflow>
//                  ^ - for BadStreamSize
//                    ^^^^^^^^^^^^^^ - for BadStreamSize
endmodule

function automatic int func1;
    typedef struct { int a[]; int i[400000000]; logic l; } asdf;
    struct { asdf a[]; bit b; asdf c; } p[];
    int o[] = {1};
    {<< {p}} = {<< {o}};
//  ^^^^^^^^ BadStreamSize streaming operator target size <overflow> does not fit source size 32
    return 0;
endfunction

function automatic int func2;
    typedef struct { int a[]; int i[400000000]; logic l; } asdf;
    struct { asdf a[]; bit b; asdf c; } p[];
    int o[] = {1};
    {<< {p}} = o;
//  ^^^^^^^^ BadStreamSize streaming operator target size <overflow> does not fit source size 32
    return 0;
endfunction

function automatic int func3(int a, int b);
    int foo[3];
    {<< {foo with [a:b]}} = 5;
//       ^^^^^^^^^^^^^^ RangeOOB cannot select range of [1:1000000000] from 'int$[3]'
//       ^^^^^^^^^^^^^^ ObjectTooLarge object size exceeds implementation limit of 2^31 bytes
endfunction

module n_stream_operator_size_overflow;
    localparam int q = func1();
//                     ^ NoteInCallTo in call to 'func1()' - for BadStreamSize(32)
    localparam int r = func2();
//                     ^ NoteInCallTo in call to 'func2()' - for BadStreamSize(32)
    localparam int s = func3(1, 1000000000);
//                     ^ NoteInCallTo in call to 'func3(1, 1000000000)' - for RangeOOB(int$[3], 1)
//                     ^ NoteInCallTo in call to 'func3(1, 1000000000)' - for ObjectTooLarge(2)
endmodule
