var milter = require('./build/Release/milter');

process.on('uncaughtException', function (e) { console.log(e); });

function connect (envelope, host, address)
{
  envelope.test = "does this persist?";

  console.log("*** connect");
  console.log(envelope);
  console.log(host);
  console.log(address);
  console.log("***");
  return milter.SMFIS_CONTINUE;
}

function close (envelope)
{
  console.log("*** close");
  console.log(envelope);
  console.log("***");
  return milter.SMFIS_CONTINUE;
}

var ok = milter.start("inet:12345", connect, close);
console.log(ok);
console.log(milter);
