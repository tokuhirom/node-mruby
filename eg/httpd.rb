http = NodeJS.require('http')

http.createServer {|req, res|
#   NodeJS.eval('gc()')
#   GC.start
    res.end("Oh!\n")
}.listen(8124, '127.0.0.1')

