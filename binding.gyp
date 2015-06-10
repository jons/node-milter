{
  "targets": [
    {
      "target_name": "milter",
      "sources": [ "src/events.cc", "src/milter.cc" ],
      "include_dirs": [ "." ],
      "cflags": [ "-g" ],
      "libraries" : [ "../libmilter/libmilter.so" ]
    }
  ]
}
