app = NodeJS.require('http').createServer {|req, res|
    html = '
<html>
<head>
    <meta charset="utf8">
    <title>socket.io example</title>
</head>
<body>
<h1>This is a example script for node-mruby + socket.io</h1>
<script src="/socket.io/socket.io.js"></script>
<script>
    var socket = io.connect("http://localhost:8080");
    socket.on("news", function (data) {
        console.log(data);
        socket.emit("my other event", { my: "data" });
    });
</script>
</body>
</html>
'
    res.writeHead(200)
    res.end(html)
}
io = NodeJS.require('socket.io').listen(app)
fs = NodeJS.require('fs')

app.listen(8080)

io.sockets.on('connection') {|socket|
    socket.emit('news', {hello: 'world'})
    socket.on('my other event') {|data|
        p data
    }
}

__END__

var app = require('http').createServer(handler)
  , io = require('socket.io').listen(app)
  , fs = require('fs')

app.listen(80);

function handler (req, res) {
  fs.readFile(__dirname + '/index.html',
  function (err, data) {
    if (err) {
      res.writeHead(500);
      return res.end('Error loading index.html');
    }

    res.writeHead(200);
    res.end(data);
  });
}

io.sockets.on('connection', function (socket) {
  socket.emit('news', { hello: 'world' });
  socket.on('my other event', function (data) {
    console.log(data);
  });
});
