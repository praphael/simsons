const express = require('express')
const bodyParser = require('body-parser');
const app = express()
const port = 3000
const dbName = '/tmp/simsoms.db'
const sqlite3 = require('sqlite3').verbose();
const db = new sqlite3.Database(dbName, sqlite3.OPEN_READWRITE, (err) => {
  if (err) {
    console.error(err.message);
  }
  console.log('Connected to the database.');
});

const maxPosts = 10

app.get('/', (req, res) => {
  res.send('Hello World!')
})

app.get('/getfeed/:uname', (req, res) => {
  qy = "SELECT author, content, post_time_utc FROM posts WHERE author IN " +
            " (SELECT sub_to FROM subs WHERE username='" + req.params.uname + "')" + 
            " ORDER BY post_time_utc DESC LIMIT " + maxPosts;
  console.log(qy);
  res.type('json');  
  r = [];
  db.all(qy, (e, rows) => {
    if(e) {
      console.log(e);
      res.statusCode = 500;
      res.send({"posts":null, "error":e});
      return
    }
    
    rows.forEach((row) => {
      console.log(row);
      // build json array
      r.push([row.author, row.content,row.post_time_utc]);
    });
    res.statusCode = 200;
    res.send({"posts":r, "error": null});
  });
})

app.post('/postmsg/:uname', bodyParser.json(), (req, res) => {
  res.type('json'); 
  if (req.headers["content-type"] != "application/json") {
    res.statusCode = 400
    res.send({"error":"content-type must be 'application/json'"});
    return
  }

  if (!('msg' in req.body)) {
    res.statusCode = 400
    res.send({"error":"'msg' is blank"});
    return
  }
  
  const stmt = db.prepare("INSERT INTO posts (author, content, post_time_utc) VALUES ($a, $c, $t)", (e) => { 
    if(e) {
        console.log("ERROR db.prepare INSERT 'posts'");
        console.log(e);
        res.statusCode = 500;
        res.send({"error":e});
        return
    }
  });

  const t1 = Date.now()
   console.log("req.body " + JSON.stringify(req.body))
  const r = {$a : req.params.uname, $c : req.body['msg'], $t : t1 / 1000};
  console.log(r)
  stmt.run(r, (e) => { 
    if(e) {
        console.log("ERROR insert posts '" + req.params.uname + "' post '" + req.body['msg'] + "'");
        console.log(e);
        res.statusCode = 500;
        res.send({"error":e});
        return
    }

    res.statusCode = 200;
    res.send({"result":"ok"});
  });
});


app.listen(port, () => {
  console.log(`Example app listening on port ${port}`)
})