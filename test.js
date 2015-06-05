var milter = require('./build/Release/milter');

console.log(milter.register());
console.log(milter.setconn());
console.log(milter.main());
