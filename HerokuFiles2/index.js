var cool = require('cool-ascii-faces')
var express = require('express');
var app = express();
var pg = require('pg');
var moment = require('moment-timezone');
var jsdom = require("jsdom");

moment.tz.setDefault("America/Los_Angeles");

app.set('port', (process.env.PORT || 5000));

app.use(express.static(__dirname + '/public'));

// views is directory for all template files
app.set('views', __dirname + '/views');
app.set('view engine', 'ejs');

app.get('/', function (request, response) {
  response.render('pages/index');
});

app.get('/cool*', function (request, response) {
  response.send(cool());
  //console.log(request.url);
  //console.log(typeof request.url);

  var str = request.url;
  str = str.replace("/cool", '');
  //console.log(str);

  var array = str.split(",");

  var now = moment().format();
  //console.log(now);
  //console.log(typeof now);

  array[4] = array[4].substring(0, array[4].length - 1);

  var data_arr = array[4].split(".");
  var data_str = "";
  for (var i = 0; i < data_arr.length; i++)
  {
    data_str += ((((parseInt(data_arr[i],16) / 26214) - 1.25) / 1.25 ) * -.002).toFixed(6).toString();
    data_str += ","
  }
  data_str = data_str.substring(0, data_str.length - 1);

  pg.connect(process.env.DATABASE_URL, function(err, client, done) {

    //var q = "insert into team_ohm_table values (1234, '" + request.url + "')";
    var q = "insert into team_ohm_2016_table values ('" + array[0] + "','" + array[1] + "','" + array[2] + "','" + array[3] + "','" + now + "','" + data_str + "')";
    console.log(q);
    console.log(typeof q);
      client.query(q, function(err, result) {
        done();
        if (err)
         { console.error(err); response.send("Error " + err); }
      });
    });
});


app.get('/db', function (request, response) {
  pg.connect(process.env.DATABASE_URL, function(err, client, done) {
    client.query('SELECT * FROM team_ohm_2016_table', function(err, result) {
      done();
      if (err)
       { console.error(err); response.send("Error " + err); }
      else
       { 
        var newArray = new Array(result.rows.length);
        for (var i = 0; i < result.rows.length; i++)
        {
          newArray[i] = result.rows[result.rows.length - 1 - i];
        }

        response.render('pages/db', {results: newArray} ); }
    });
  });
})

app.get('/db.csv*', function (request, response) {
  var numRows = request.url.replace("/db.csv?num_rows=", '');

  pg.connect(process.env.DATABASE_URL, function(err, client, done) {
    client.query('SELECT * FROM team_ohm_2016_table', function(err, result) {
      done();
      if (err)
       { console.error(err); response.send("Error " + err); }
      else
       { 
        var newArray = new Array(result.rows.length);
        for (var i = 0; i < result.rows.length; i++)
        {
          newArray[i] = result.rows[result.rows.length - 1 - i];
        }

        // //var csvContent = "data:text/csv;charset=utf-8,";
        // var csvContent = newArray.join("\n");

        // response.set('Content-Type', 'application/octet-stream');
        // response.send(csvContent);

        var csvContent = 'netid,nodeid,channel,sample_number,datetime,data\n';

        var dl_rows;
        if (numRows.length == 0)
          dl_rows = newArray.length;
        else if (newArray.length > numRows)
          dl_rows = numRows;
        else
          dl_rows = newArray.length;

        for (var i = 0; i < dl_rows; i++)
        {
          csvContent += newArray[i].netid.toString() + ',' +
                        newArray[i].nodeid.toString() + ',' +
                        newArray[i].channel.toString() + ',' +
                        newArray[i].sample_number.toString() + ',' +
                        newArray[i].datetime.toString() + ',' +
                        newArray[i].data.toString() + '\n';
        }

        var t = numRows;
        console.log(t);
        console.log(t.length);
        console.log(typeof t);

        response.set('Content-Type', 'application/octet-stream');
        response.send(csvContent);
      

      }

    });
  });
})

app.get('/db_old', function (request, response) {
  pg.connect(process.env.DATABASE_URL, function(err, client, done) {
    client.query('SELECT * FROM test_table', function(err, result) {
      done();
      if (err)
       { console.error(err); response.send("Error " + err); }
      else
       {
        var newArray = new Array(result.rows.length);
        for (var i = 0; i < result.rows.length; i++)
        {
          newArray[i] = result.rows[result.rows.length - 1 - i];
        }

        response.render('pages/db_old', {results: newArray} ); }
    });
  });
})

app.get('/db_test', function (request, response) {
  pg.connect(process.env.DATABASE_URL, function(err, client, done) {
    client.query('SELECT * FROM team_ohm_test', function(err, result) {
      done();
      if (err)
       { console.error(err); response.send("Error " + err); }
      else
       { response.render('pages/db_test', {results: result.rows} ); }
    });
  });
})

app.post('/', function (request, response){
    console.log('POST /');
    console.dir(request.body);
    response.writeHead(200, {'Content-Type': 'text/html'});
    response.end('thanks');
});


app.listen(app.get('port'), function() {
  console.log('Node app is running on port', app.get('port'));
});

