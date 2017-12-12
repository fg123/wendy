/* finance.w: WendyScript 2.0
 * Finance Module
 * By: Felix Guo
 * Provides: PV, FV, PMT
 */

import math

let fvoa => (rate, nper, pmt) pmt * ((pow(1 + rate, nper) - 1) / rate);
let fvad => (rate, nper, pmt) (1 + rate) * fvoa(rate, nper, pmt);
let pvoa => (rate, nper, pmt) pmt * ((1 - pow(1 + rate, -nper)) / rate);
let pvad => (rate, nper, pmt) pmt + pvoa(rate, nper - 1, pmt);

