/**
 * WFScript library composition: real open-source modules via relative ES imports.
 * Fraction.js 4.3.7, pako 1.0.11, base64-js 0.0.8 (licenses included).
 */
import Fraction from './vendor/fraction.mjs';
import { deflate, inflate } from './vendor/pako.mjs';
import { fromByteArray, toByteArray } from './vendor/base64.mjs';

/** Return exact sampleCount:cycle ratio as a rational number. */
export function samplesPerCycle(sampleRateHz, toneHz) {
    if (!(sampleRateHz > 0 && toneHz > 0) || !Number.isInteger(sampleRateHz) || !Number.isInteger(toneHz))
        throw Error('Rate and tone must be positive integers');
    const ratio = new Fraction(sampleRateHz, toneHz);
    return {
        fraction: ratio.toFraction(),
        decimal: ratio.valueOf()
    };
}

/** Losslessly compress a JSON capture using pako and encode with base64-js. */
export function packCapture(capture) {
    const json = JSON.stringify(capture);
    const zip = deflate(json);
    const b64 = fromByteArray(zip);
    return {
        encoded: b64,
        rawBytes: json.length,
        compressedBytes: zip.length,
        ratio: zip.length / json.length,
        encodedBytes: b64.length
    };
}

/** Decode and inflate, proving the packed capture can be reconstructed. */
export function unpackCapture(encoded) {
    return JSON.parse(inflate(toByteArray(encoded), { to: 'string' }));
}
