var milter = require('./build/Release/milter');


process.on('uncaughtException', function (e) { console.log(e); });

var ok = milter.start("inet:12345");

milter.connect = function (host, address)
{
  console.log("node.js! connect");
  console.log(host);
  console.log(address);
  return 1357;
  //return milter.SMFIS_CONTINUE;
};

console.log(ok);
console.log(milter);
