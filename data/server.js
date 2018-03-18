var express = require('express');
var path = require('path');

var port = 3011;
var app = express();
app.use('/proxy', function (request, response, next) {
  response.header('Access-Control-Allow-Origin', '*');
  response.header('Cache-Control', 'no-cache, no-store, must-revalidate');
  response.header('Pragma', 'no-cache');
  response.header('Expires', '0');
  next(request, response);
});

app.use('/', express.static(path.resolve('.')));
app.listen(port, function () {
  console.log('listening at localhost:' + port);
});
