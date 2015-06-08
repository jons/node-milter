var milter = require('./build/Release/milter');


function connect (host)
{
  console.log("connect");
  console.log(host);
  return 123;
}

process.on('uncaughtException', function (e) { console.log(e); });

console.log(milter.start("inet:12345"));
