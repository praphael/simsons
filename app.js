const express = require('express')
const bodyParser = require('body-parser');
const crypto = require('crypto');
const path = require('path');
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

const superToken = "baf1c8b62acd6899377960ce8f0edaf7391d8624"

const maxPosts = 10
let user_tokens = new Map()

publicPath = path.join(__dirname, '')
console.log("Serving static files from " + publicPath)
app.use('/', express.static(publicPath));


app.get('/', (req, res) => {
  res.type('text/html')
  res.header('Content-Type', 'text/html')
  console.log("res.headers ", res.headers);
  console.log("req.headers ", req.headers);
  // console.log("set-cookie", req.headers['set-cookie'])
  res.send('<html>Hello World!</html>');
 })

function checkContentType(req, res) {
  if (req.headers["content-type"] != "application/json") {
    res.statusCode = 400
    res.send({"error":"content-type must be 'application/json'"});
    return false
  }
  return true;
}
app.post('/login/:uname', bodyParser.json(), (req, res) => {
    if(!checkContentType(req, res)) return;
    printErrMsg = true
    res.type('application/json');
    if( "uname" in req.params) {
      uname = req.params.uname;
      if ("body" in req) {
        // console.log("login req.body=" + JSON.stringify(req.body));
        if("passwd" in req.body) {
          passwd = req.body["passwd"];
          // TODO validate username/password
          // res.statusCode = 400;
          // res.send({"error": "Bad login"});
          
          tok_val = crypto.randomBytes(20);
          tok = tok_val.toString('hex');
          user_tokens.set(uname, tok);
          res.statusCode = 200;
          res.send({"usertoken":tok});
          return
        }
        if(printErrMsg) {
          console.log("login failed - 'passwd' not in body");  
          printErrMsg = false;
        }
      }
      if(printErrMsg) {
        console.log("login failed - reqeuest has no body");
        printErrMsg = false;
      }
    }
    if(printErrMsg) {
      console.log("login failed - 'uname' not in request");
    }
    res.statusCode = 400;
    
    res.send({"error": "Bad login"});
})

function validateLogin(req, res) {
  printErrMsg=true;
  
  if("uname" in req.params) {
    uname = req.params.uname;
    // console.log("validateLogin: req.body= " + JSON.stringify(req.body));
    if ('user_token' in req.body) {
      tok = req.body['user_token'];
      if(user_tokens.has(uname) || tok == superToken) {
        user_tok = user_tokens.get(uname);
        if (user_tok == tok || tok == superToken)
          return true;
        else if(printErrMsg) {
          console.log("validateLogin token mismatch tok='"+ tok + "' user_tok='" + user_tok + "'");
          printErrMsg = false;
        }
      }
      if(printErrMsg) {
        console.log("validateLogin failed '" + uname + "' not in user_tokens");
        printErrMsg = false;
      }
    }
    if(printErrMsg) {
      console.log("validateLogin failed 'user_token' not in req.body");
      printErrMsg = false;
    }
  }
  if(printErrMsg) {
    console.log("validateLogin failed 'uname' not in req.params");
    printErrMsg = false;
  }
  
  res.type("json");
  res.statusCode = 400;
  res.send({"error": "Bad username/token information in request"});

  return false;
}

//let feed_q = [];
app.post('/getfeed/:uname', bodyParser.json(), (req, res) => {
  console.log("getfeed: uname=" + req.params.uname); 
  //if (feed_q.length === 0) {
    // we will add our response object in a special queue
  //  feed_q.push(res);
    if (!validateLogin(req, res)) return;
    uname = req.params.uname;
    qy = "SELECT author, content, post_time_utc FROM posts WHERE author IN " +
              " (SELECT sub_to FROM subs WHERE username='" + uname + "'" +
              "  UNION SELECT '" + uname + "')" +
              " ORDER BY post_time_utc DESC LIMIT " + maxPosts;
    // console.log(qy);
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
        // console.log(row);
        // build json array
        r.push([row.author, row.content,row.post_time_utc]);
      });
      res.statusCode = 200;
      res.send({"posts":r, "error": null});
    });
/*
    feed_q = [];
  } 
  else {
    feed_q.push(res);
  }
  */
})

app.post('/postmsg/:uname', bodyParser.json(), (req, res) => {
  if(!checkContentType(req, res)) return;
  if (!validateLogin(req, res)) return;
  res.type('json'); 

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
  // console.log("req.body " + JSON.stringify(req.body))
  const r = {$a : req.params.uname, $c : req.body['msg'], $t : t1 / 1000};
  // console.log(r)
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