name := shell
description := A command-line-interface shell on the console
objs-y := main.o commands.o http.o fs.o
libs-y := driver
