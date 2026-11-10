// WFScript v5.6.1 — open in Script Editor to inspect syntax colors.
// This is an editor demonstration only. It does not control the AD3.

const url = "https://example.org/a/* not a comment */";
const quoted = 'literal // text and \'escaped quote';
const regex = /https?:\/\/[^\s/]+\/a\/b/gi;
const quotient = 60 / 3;  // Division is not a regular expression.

function measureSomething(scope) {
    if (scope && scope.Channel1) {
        for (let i = 0; i < 3; i++) {
            console.log(`Iteration ${i}: ${Math.round(1_234.56)} volts`);
        }
    }
}

const template = `Header contains // literal comment text
    ${Math.max(2, 3)} is a calculated result
    ${({name: `nested ${Math.PI}`}).name}
    End of template`;

/* A multiline comment containing "strings" and `templates`:
   if(true) Scope.single(); // this must remain comment gray
*/

// Typical WFScript paths (comment out to keep this gallery non-operational):
// Wavegen1.Channel1.Simple.Frequency.value = 1000;
// Scope1.Channel1.measure("RMS");
