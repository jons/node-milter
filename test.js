var milter = require('./build/Release/milter');


/** */
function milter_connect (host)
{
  console.log("milter_connect");
  console.log(host);
  return 0;
}


process.on('uncaughtException', function (e) {
  console.log(e);
});

console.log(
  milter.main(
    "inet:2701",
    {
      connect: milter_connect
    }));
