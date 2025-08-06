/* eslint-env mocha */
/* eslint func-names: 0 */

const assert = require('assert');
const path = require('path');
const unroll = require('unroll');
unroll.use(it);

const fs = require('fs');
const jsCompiler = require('../_js_tooling.js');

describe('js_tooling.js', () => {
  unroll('compiles #filename with #expectedResult', (done, fixture) => {
    var js_file = path.join('fixtures', fixture.filename);
    var js = fs.readFileSync(js_file, 'utf8');
    const result =  jsCompiler.createSnapshot(js);
    assert.equal(result.result, fixture.expectedResult);
    done();
  }, [
      ['filename', 'expectedResult'],
      ['multiple-emojis.js', 'success'],
      ['syntax-error.js', 'error']
  ]);
});
