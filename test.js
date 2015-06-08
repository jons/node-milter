var milter = require('./build/Release/milter');


function connect (host)
{
  console.log("connect");
  console.log(host);
  return milter.SMFIS_CONTINUE;
}

process.on('uncaughtException', function (e) { console.log(e); });

console.log(milter);
console.log(milter.start("inet:12345"));
