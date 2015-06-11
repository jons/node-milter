var milter = require('./build/Release/milter');

process.on('uncaughtException', function (e) { console.log(e); });


function connect (envelope, host, address)
{
  console.log("*** connect");
  console.log("***", envelope);
  console.log("***", host);
  console.log("***", address);
  return milter.SMFIS_CONTINUE;
}

function unknown (envelope, command)
{
  console.log("*** unknown");
  console.log("***", envelope);
  console.log("***", command);
  return milter.SMFIS_CONTINUE;
}

function helo (envelope, identity)
{
  console.log("*** helo");
  console.log("***", envelope);
  console.log("***", identity);
  return milter.SMFIS_CONTINUE;
}

function mailfrom () { return milter.SMFIS_CONTINUE; }
function rcptto () { return milter.SMFIS_CONTINUE; }
function mstart () { return milter.SMFIS_CONTINUE; }
function header () { return milter.SMFIS_CONTINUE; }
function eoh () { return milter.SMFIS_CONTINUE; }
function segment () { return milter.SMFIS_CONTINUE; }
function mfinish () { return milter.SMFIS_CONTINUE; }
function abort () { return milter.SMFIS_CONTINUE; }

function close (envelope)
{
  console.log("*** close");
  console.log("***", envelope);
  return milter.SMFIS_CONTINUE;
}

var ok = milter.start(
  "inet:12345", // see postconf manual on smtpd_milters for details
  connect,  // connection event
  unknown,  // unknown client command
  helo,     // client "HELO" or "EHLO" i presume
  mailfrom, // client "MAIL FROM"
  rcptto,   // client "RCPT TO"
  mstart,   // client "DATA"
  header,   // parser event
  eoh,      // parser event
  segment,  // buffer of data
  mfinish,  // client "." (EOM)
  abort,    // ?
  close);   // connection event

console.log(ok);
