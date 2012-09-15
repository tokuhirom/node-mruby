http = NodeJS.require('http')

http.createServer {|req, res|
    GC.start
    res.end("Oh!\n")
}.listen(8124, '127.0.0.1')

__END__
http.createServer {|req, res|
#   NodeJS.eval('gc()')
    res.end("Oh!\n")
}.listen(8124, '127.0.0.1')

