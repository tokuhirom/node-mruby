http = NodeJS.require('http')

http.createServer {|req, res|
    res.end("Oh!\n")
}.listen(8124, '127.0.0.1')

