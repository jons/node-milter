{
  "targets": [
    {
      "target_name": "milter",
      "sources": [ "src/events.cc", "src/milter.cc" ],
      "include_dirs": [ "." ],
      "cflags": [ "-g -Wall" ],
      "cflags!": [ "-O3" ],
      "libraries" : [ "../libmilter/libmilter.so" ]
    }
  ]
}
