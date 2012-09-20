app = NodeJS.require('http').createServer {|req, res|
    html = '
<html>
<head>
    <meta charset="utf8">
    <title>socket.io example</title>
</head>
<body>
<h1>This is a example script for node-mruby + socket.io</h1>
<form id="chatform">
    <input type="text" id="msg">
    <input type="submit">
</form>
<div id="log">
</div>
<script src="/socket.io/socket.io.js"></script>
<script src="http://code.jquery.com/jquery-1.8.1.min.js"></script>
<script>
    var socket = io.connect("http://localhost:8080");
    console.log("Ready");
    var logElem = $("#log");
    socket.on("chat", function (data) {
        console.log(data);
        logElem.prepend($("<div>" + data["332"] + "</div>"));
        socket.emit("my other event", { my: "data" });
    });
    $("#chatform").submit(function () {
        socket.emit("chat", {msg: $("#msg").val()});
        logElem.prepend($("<div>" + $("#msg").val() + "</div>"));
        $("#msg").val("");
        return false;
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
    socket.emit('chat', {msg: 'initialized'})
    socket.on('chat') {|data|
        p data.msg
        socket.broadcast.emit('chat', {msg: data.msg})
    }
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
