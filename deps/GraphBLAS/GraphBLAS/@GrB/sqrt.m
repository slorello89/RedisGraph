function C = sqrt (G)
%SQRT square root.
% C = sqrt (G) is the square root of the entries of G.
%
% See also GrB.apply, GrB/hypot.

% SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2022, All Rights Reserved.
% SPDX-License-Identifier: GPL-3.0-or-later

G = G.opaque ;
C = GrB (gb_check_imag_zero (gb_trig ('sqrt', G))) ;

